#ifndef _AMALGAMATE_
#include "script.h"
#include "ulist.h"
#include "pool.h"
#include "log.h"
#include "paths.h"
#include "env.h"
#include "utils.h"
#include "xstr.h"
#include "spawn.h"
#include "alloc.h"
#include "map.h"
#include "toolchain.h"
#include "cxx_modules.h"
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#endif

/// Compilation database state.
struct _cdb {
  const char *path;
  char _path[PATH_MAX];
} _cdb;

struct _cc_ctx {
  struct pool *pool;
  struct ulist sources;     // char*
  struct ulist objects;     // char*
  struct node *n;
  struct node *n_cflags;
  struct node *n_sources;
  struct node *n_cc;
  struct node *n_consumes;
  struct node *n_objects;
  struct node *n_modules;
  struct cxx_modules *modules;  // C++20 modules compilation state (cxx rules only)
  const char  *cc;
  const char  *objskey;
  const char  *cflags_default; // Used when no explicit flags expression is given (host-cc/host-cxx)
  const char  *tag;            // Toolchain object tag. Zero if objects are not tagged.
  bool is_host;                // Compiles build tools running on the build host
  struct ulist consumes;       // sizeof(char*)
  int num_failed;
};

static void _cc_cdb_entry_add(struct node *n, struct spawn *s, const char *src, const char *tgt);

/// Directory (relative to the unit cache dir) holding objects of build tools.
#define CC_HOST_OBJS_DIR "host-tools"

/// Computes the object file path (relative to the unit cache dir) for a given source.
/// The result is allocated and should be freed by the caller.
static char* _cc_obj_path(struct _cc_ctx *ctx, const char *src, struct unit *unit) {
  char buf[PATH_MAX];
  const char *nsrc = path_normalize_cwd(src, unit->cache_dir, buf);
  bool incache = path_is_prefix_for(g_env.project.cache_dir, nsrc, unit->cache_dir);
  char *obj = incache ? path_relativize_cwd(unit->cache_dir, nsrc, unit->cache_dir)
              : path_relativize_cwd(unit->dir, nsrc, unit->dir);

  char *p = strrchr(obj, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';

  if (ctx->is_host) {
    size_t len = strlen(obj) + sizeof(CC_HOST_OBJS_DIR) + 2;
    char *prefixed = xmalloc(len);
    snprintf(prefixed, len, "%s/%s", CC_HOST_OBJS_DIR, obj);
    free(obj);
    obj = prefixed;
  }
  return obj;
}

/// Writes the toolchain tag sidecar of a compiled object file.
static void _cc_obj_tag_write(struct _cc_ctx *ctx, const char *obj) {
  // A zero tag removes a stale tag of a previously built (cross) target
  int rc = toolchain_object_tag_write(obj, ctx->tag);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to write toolchain tag of object file: %s", obj);
  }
}

static void _cc_deps_MMD_add(struct node *n, struct deps *deps, const char *src, const char *obj, char flags) {
  char buf[MAX(2 * PATH_MAX, 8192)];
  size_t len = strlen(obj);
  utils_strncpy(buf, obj, sizeof(buf));

  char *p = strrchr(buf, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'd';
  p[2] = '\0';

  FILE *f = fopen(buf, "r");
  if (!f) {
    if (g_env.verbose) {
      node_info(n, "No compiler generated (-MMD) dependency file: %s", buf);
    }
    return;
  }

  bool nl = false;

  while ((p = fgets(buf, sizeof(buf), f))) {
    if (!nl) {
      if (!utils_startswith(p, obj)) {
        continue;
      }
      p += len;
      // The first target may be followed by module specific targets (gcc)
      char *colon = strchr(p, ':');
      if (colon == 0) {
        continue;
      }
      p = colon + 1;
    } else {
      nl = false;
    }

    while (*p != '\0') {
      while (*p != '\0' && utils_char_is_space(*p)) {
        ++p;
      }
      if (p[0] == '\\' && p[1] == '\n') {
        nl = true;
        break;
      }
      char *sp = p;
      while (*p != '\0' && !utils_char_is_space(*p)) {
        ++p;
      }
      if (p > sp) {
        if (*p != '\0') {
          *p = '\0';
          ++p;
        }
        // Skip logical module references emitted by gcc (`.c++m`)
        if (utils_endswith(sp, ".c++m")) {
          continue;
        }
        deps_add_alias(deps, flags, src, sp);
      }
    }
  }
  fclose(f);
}

struct _cc_task {
  struct spawn *s;
  char *src;
  char *obj;
  pid_t pid;
};

static void _cc_task_destroy(struct _cc_task *t) {
  spawn_destroy(t->s);
  free(t->src);
  free(t->obj);
}

static void _cc_on_build_source(
  struct node     *n,
  const char      *src,
  const char      *obj,
  struct _cc_task *task) {
  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: build src=%s obj=%s\n", n->name, src, obj);
  }

  struct _cc_ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  struct spawn *s = spawn_create(ctx->cc, ctx);
  task->s = s;

  spawn_set_nowait(s, true);

  const char *cflags = ctx->n_cflags ? node_value(ctx->n_cflags) : ctx->cflags_default;
  if (cflags && *cflags != '\0') {
    struct xstr *xstr = 0;
    if (!is_vlist(cflags)) {
      xstr = xstr_create_empty();
      utils_split_values_add(cflags, xstr);
      cflags = xstr_ptr(xstr);
    }
    spawn_arg_add(s, cflags);
    xstr_destroy(xstr);
  }

  if (ctx->modules) {
    char buf[PATH_MAX];
    cxx_modules_args_add(ctx->modules, s, path_normalize_cwd(src, unit->cache_dir, buf));
  }

  if (!spawn_arg_starts_with(s, "-M")) {
    spawn_arg_add(s, "-MMD");
  }

  spawn_arg_add(s, "-I./"); // Current cache dir
  spawn_arg_add(s, "-c");
  spawn_arg_add(s, src);

  spawn_arg_add(s, "-o");
  spawn_arg_add(s, obj);

  _cc_cdb_entry_add(n, s, src, obj);

  int rc = spawn_do(s);
  if (rc) {
    task->s = 0;
    spawn_destroy(s);
    node_error(rc, ctx->n, "%s", ctx->cc);
    return;
  }

  task->pid = spawn_pid(s);
}

/// Compiles the given sources (up to `max_jobs` in parallel).
///
/// Failed sources are registered in `fmap` (cache relative source path -> failure).
/// Dependencies of compiled sources are recorded by the caller.
static void _cc_compile_sources(
  struct _cc_ctx *ctx,
  struct ulist   *slist,
  int             max_jobs,
  struct map     *fmap) {
  struct unit *unit = unit_peek();
  struct ulist tasks = { .usize = sizeof(struct _cc_task) };

  for (int i = 0; i < slist->num || tasks.num > 0; ) {
    while (tasks.num < max_jobs && i < slist->num) {
      char buf[PATH_MAX];
      char *obj, *src = *(char**) ulist_get(slist, i);
      obj = _cc_obj_path(ctx, src, unit);
      src = path_normalize_cwd(src, unit->cache_dir, buf);
      src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);

      struct _cc_task task = { .src = src, .obj = obj, .pid = -1 };
      _cc_on_build_source(ctx->n, src, obj, &task);

      if (task.pid == -1) {
        ++ctx->num_failed;
        map_put_str(fmap, src, (void*) (intptr_t) 1);
        _cc_task_destroy(&task);
      } else {
        ulist_push(&tasks, &task);
      }
      ++i;
    }

    if (tasks.num == 0) {
      continue;
    }

    int wstatus = 0;
    pid_t pid = wait(&wstatus);
    if (pid == -1) {
      akfatal(errno, "wait() syscall failed", 0);
    }

    for (int j = 0; j < tasks.num; ++j) {
      struct _cc_task *t = (struct _cc_task*) ulist_get(&tasks, j);
      if (t->pid == pid) {
        spawn_set_wstatus(t->s, wstatus);
        int code = spawn_exit_code(t->s);
        if (code != 0) {
          ++ctx->num_failed;
          map_put_str(fmap, t->src, (void*) (intptr_t) 1);
        } else {
          _cc_obj_tag_write(ctx, t->obj);
        }
        _cc_task_destroy(t);
        ulist_remove(&tasks, j);
        break;
      }
    }
  }
  ulist_destroy_keep(&tasks);
}

/// Splits outdated sources into module interface units and regular sources.
static void _cc_sources_split(
  struct _cc_ctx *ctx,
  struct node_resolve *r,
  struct ulist *ifaces,
  struct ulist *sources) {
  struct map *outdated = 0;
  bool all = r->resolve_outdated.num == 0;

  for (int i = 0; !all && i < r->resolve_outdated.num; ++i) {
    struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
    if (u->flags != 's') { // Rebuild all on any outdated non source dependency
      all = true;
    }
  }

  if (!all) {
    outdated = map_create_str(map_k_free);
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      map_put_str(outdated, u->path, (void*) (intptr_t) 1);
    }
  }

  for (int i = 0; i < ctx->sources.num; ++i) {
    const char *src = *(char**) ulist_get(&ctx->sources, i);
    if (!all && !map_get(outdated, src)) {
      continue;
    }
    if (ctx->modules && cxx_modules_is_interface(ctx->modules, src)) {
      ulist_push(ifaces, &src);
    } else {
      ulist_push(sources, &src);
    }
  }
  map_destroy(outdated);
}

static void _cc_on_resolve(struct node_resolve *r) {
  struct deps deps;
  struct _cc_ctx *ctx = r->user_data;
  struct unit *unit = unit_peek();
  struct ulist ifaces = { .usize = sizeof(char*) };
  struct ulist slist = { .usize = sizeof(char*) };
  struct map *fmap = map_create_str(map_k_free);

  int max_jobs = g_env.max_parallel_jobs;
  if (max_jobs <= 0) {
    max_jobs = 1;
  }

  _cc_sources_split(ctx, r, &ifaces, &slist);

  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(ctx->n, &deps);

  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }

  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes, i);
    deps_add(&deps, DEPS_TYPE_FILE, 0, path, 0);
  }

  // Module interface units must be compiled before all importers.
  // They are compiled in their declaration order.
  _cc_compile_sources(ctx, &ifaces, 1, fmap);

  if (ctx->num_failed) {
    if (g_env.verbose && slist.num) {
      node_info(ctx->n, "Skipping %u source(s) due to C++ module compilation errors", slist.num);
    }
  } else {
    _cc_compile_sources(ctx, &slist, max_jobs, fmap);
  }

  for (int i = 0; i < ctx->sources.num; ++i) {
    char buf[PATH_MAX];
    char *obj, *abs = *(char**) ulist_get(&ctx->sources, i);
    bool iface = ctx->modules && cxx_modules_is_interface(ctx->modules, abs);
    char flags = iface ? 0 : 's'; // Interface units rebuild the whole group
    obj = _cc_obj_path(ctx, abs, unit);
    char *src = path_normalize_cwd(abs, unit->cache_dir, buf);
    src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);

    bool failed = map_get(fmap, src) != 0;
    deps_add(&deps, failed ? DEPS_TYPE_FILE_OUTDATED : DEPS_TYPE_FILE, flags, src, 0);

    if (!failed) {
      _cc_deps_MMD_add(ctx->n, &deps, src, obj, flags);
    }
    free(obj);
    free(src);
  }

  cxx_modules_deps_add(ctx->modules, &deps);

  deps_close(&deps);
  ulist_destroy_keep(&ifaces);
  ulist_destroy_keep(&slist);
  map_destroy(fmap);
}

static void _cc_on_consumed_resolved(const char *path_, void *d) {
  struct node *n = d;
  struct _cc_ctx *ctx = n->impl;
  const char *path = pool_strdup(ctx->pool, path_);
  ulist_push(&ctx->consumes, &path);
}

static void _cc_on_resolve_init(struct node_resolve *r) {
  struct _cc_ctx *ctx = r->n->impl;
  if (ctx->n_consumes) {
    node_consumes_resolve(r->n, ctx->n_consumes->child, 0, _cc_on_consumed_resolved, r->n);
  }
}

static void _cc_cdb_init(struct node *n) {
  if (!g_env.project.compile_commands) {
    return;
  }
  const char *path = getenv(AUTARK_COMPILE_COMMANDS_ENV);
  if (!path) {
    return;
  }
  if (g_env.project.compile_commands_own) {
    unlink(path);
  }
  utils_strncpy(_cdb._path, path, sizeof(_cdb._path));
  _cdb.path = _cdb._path;
}

struct _cc_cdb_arg_ctx {
  FILE *file;
  struct xstr *xstr;
};

static void _cc_cdb_arg_add(int num, const char *arg, void *d) {
  struct _cc_cdb_arg_ctx *ctx = d;
  xstr_clear(ctx->xstr);
  utils_json_escape_str(arg, -1, ctx->xstr);
  if (num) {
    fprintf(ctx->file, ",\n      %s", xstr_ptr(ctx->xstr));
  } else {
    fprintf(ctx->file, "\n      %s", xstr_ptr(ctx->xstr));
  }
}

static void _cc_cdb_entry_add(struct node *n, struct spawn *s, const char *src, const char *tgt) {
  if (!_cdb.path) {
    return;
  }
  FILE *f = fopen(_cdb.path, "a");
  if (!f) {
    node_fatal(errno, n, "Error opening file for writing: %s", _cdb.path);
  }
  long int pos = ftell(f);
  if (pos == -1) {
    node_fatal(errno, n, "File: %s", _cdb.path);
  }

  struct xstr *xstr = xstr_create_empty();
  if (pos > 0) {
    fprintf(f, ",\n  {\n");
  } else {
    fprintf(f, "\n  {\n");
  }
  fprintf(f, "    \"arguments\": [");
  spawn_visit_cmd(s, &(struct _cc_cdb_arg_ctx) { f, xstr }, _cc_cdb_arg_add);
  fprintf(f, "\n    ],\n");

  struct unit *unit = unit_peek();
  xstr_clear(xstr);
  utils_json_escape_str(unit->cache_dir, -1, xstr);
  fprintf(f, "    \"directory\": %s,\n", xstr_ptr(xstr));

  xstr_clear(xstr);
  utils_json_escape_str(src, -1, xstr);
  fprintf(f, "    \"file\": %s,\n", xstr_ptr(xstr));

  xstr_clear(xstr);
  utils_json_escape_str(tgt, -1, xstr);
  fprintf(f, "    \"output\": %s\n", xstr_ptr(xstr));

  fprintf(f, "  }");
  xstr_destroy(xstr);
  fclose(f);
}

static void _cc_post_build(struct node *n) {
  int rc = 0;
  if (!_cdb.path || !g_env.project.compile_commands_own) {
    return;
  }
  const char *path = _cdb.path;
  _cdb.path = 0;

  FILE *sf = fopen(path, "r");
  if (!sf) {
    return;
  }
  unlink(path);
  path = _cdb._path;
  snprintf(_cdb._path, sizeof(_cdb._path), "%s/compile_commands.json", g_env.project.cache_dir);
  FILE *tf = fopen(path, "w");
  if (!tf) {
    rc = errno;
    fclose(sf);
    node_fatal(rc, n, "Error opening file for writing: %s", path);
  }
  fputs("[", tf);
  rc = utils_copy_file_streams(sf, tf);
  if (rc) {
    fclose(sf);
    fclose(tf);
    node_fatal(rc, n, "Error writing file: %s", path);
  }
  fputs("\n]", tf);
  fclose(sf);
  fclose(tf);
  node_info(n, "Compilation database file created: %s", path);
}

static void _cc_build(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  char *objs = ulist_to_vlist(&ctx->objects);
  node_env_set(n, ctx->objskey, objs);
  free(objs);

  for (int i = 0; i < ctx->sources.num; ++i) {
    const char *src = *(char**) ulist_get(&ctx->sources, i);
    if (!path_is_exist(src)) {
      struct node *pn = node_by_product_raw(n, src);
      if (pn) {
        node_build(pn);
      } else {
        node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "'%s'", src);
      }
    }
  }

  cxx_modules_prepare(ctx->modules);

  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = ctx,
    .on_init = _cc_on_resolve_init,
    .on_resolve = _cc_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };

  if (node_is_value_may_be_dep_saved(ctx->n_cc, NODE_TYPE_VALUE)) {
    ulist_push(&r.node_val_deps, &ctx->n_cc);
  }

  if (node_is_value_may_be_dep_saved(ctx->n_cflags, NODE_TYPE_VALUE)) {
    ulist_push(&r.node_val_deps, &ctx->n_cflags);
  }

  if (node_is_value_may_be_dep_saved(ctx->n_sources, NODE_TYPE_VALUE)) {
    ulist_push(&r.node_val_deps, &ctx->n_sources);
  }

  node_resolve(&r);

  if (ctx->num_failed) {
    akfatal2("Compilation terminated with errors!");
  }
}

static void _cc_source_add(struct node *n, const char *src) {
  char buf[PATH_MAX], *p;
  if (src == 0 || *src == '\0') {
    return;
  }
  p = strrchr(src, '.');
  if (p == 0 || p[1] == '\0') {
    return;
  }

  struct _cc_ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  const char *npath = src;

  if (!path_is_absolute(src)) {
    npath = path_normalize_cwd(src, unit->dir, buf);
  }

  p = pool_strdup(ctx->pool, npath);
  ulist_push(&ctx->sources, &p);

  char *obj = _cc_obj_path(ctx, npath, unit);
  ulist_push(&ctx->objects, &obj);
  node_product_add(n, obj, 0);
}

/// Initializes C++20 modules compilation of the rule.
///
/// Sources listed in the optional `modules { }` bag are treated as module units
/// unconditionally, other sources are detected by their file extension.
static void _cc_modules_setup(struct _cc_ctx *ctx, bool is_cxx) {
  struct node *n = ctx->n;
  struct unit *unit = unit_peek();

  if (ctx->n_modules && !is_cxx) {
    node_fatal(AK_ERROR_SCRIPT, n, "`modules { }` is supported only by cxx rules");
  }
  if (!is_cxx) {
    for (int i = 0; i < ctx->sources.num; ++i) {
      const char *src = *(char**) ulist_get(&ctx->sources, i);
      if (cxx_modules_is_module_path(src)) {
        node_fatal(AK_ERROR_SCRIPT, n, "C++ module units require a cxx rule: %s", src);
      }
    }
    return;
  }

  ctx->modules = cxx_modules_create(n, ctx->cc, ctx->tag);
  struct map *explicit = ctx->n_modules ? map_create_str(map_k_free) : 0;

  if (explicit) {
    for (struct node *nn = ctx->n_modules->child; nn; nn = nn->next) {
      const char *val = node_value(nn);
      if (!val || *val == '\0') {
        continue;
      }
      struct vlist_iter iter;
      vlist_iter_init(val, &iter);
      while (vlist_iter_next(&iter)) {
        char buf[PATH_MAX], path[PATH_MAX];
        utils_strnncpy(buf, iter.item, iter.len, sizeof(buf));
        const char *abs = path_is_absolute(buf)
                          ? utils_strncpy(path, buf, sizeof(path))
                          : path_normalize_cwd(buf, unit->dir, path);
        map_put_str(explicit, abs, (void*) (intptr_t) 1);
      }
    }
  }

  for (int i = 0; i < ctx->sources.num; ++i) {
    const char *src = *(char**) ulist_get(&ctx->sources, i);
    bool required = explicit && map_get(explicit, src) != 0;
    cxx_modules_add_source(ctx->modules, src, required);
    if (required) {
      map_remove(explicit, src);
    }
  }

  if (explicit) {
    struct map_iter it;
    map_iter_init(explicit, &it);
    if (map_iter_next(&it)) {
      node_fatal(AK_ERROR_SCRIPT, n,
                 "Source listed in `modules { }` is not a part of the cxx sources: %s", (const char*) it.key);
    }
    map_destroy(explicit);
  }
}

static void _cc_setup(struct node *n) {
  _cc_cdb_init(n);

  struct _cc_ctx *ctx = n->impl;
  bool is_cxx = strcmp(n->value, "cxx") == 0 || strcmp(n->value, "host-cxx") == 0;
  ctx->is_host = utils_startswith(n->value, "host-");
  ctx->tag = toolchain_object_tag(ctx->is_host);

  const char *val = node_value(ctx->n_sources);
  if (is_vlist(val)) {
    struct vlist_iter iter;
    vlist_iter_init(val, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[PATH_MAX];
      utils_strnncpy(buf, iter.item, iter.len, sizeof(buf));
      _cc_source_add(n, buf);
    }
  } else {
    _cc_source_add(n, val);
  }
  if (ctx->n_cc) {
    const char *cc = node_value(ctx->n_cc);
    if (cc && *cc != '\0') {
      ctx->cc = pool_strdup(ctx->pool, cc);
    }
  }
  if (!ctx->cc) {
    const char *key = ctx->is_host
                      ? (is_cxx ? "HOST_CXX" : "HOST_CC")
                      : (is_cxx ? "CXX" : "CC");
    ctx->cc = pool_strdup(ctx->pool, node_env_get(n, key));
    if ((!ctx->cc || *ctx->cc == '\0') && ctx->is_host && !toolchain_id()) {
      // Native builds compile build tools with the native compiler
      ctx->cc = pool_strdup(ctx->pool, node_env_get(n, is_cxx ? "CXX" : "CC"));
    }
    if (g_env.verbose && ctx->cc) {
      node_info(n, "Found '%s' compiler in ${%s}", ctx->cc, key);
    }
  }
  if (!ctx->cc) {
    ctx->cc = is_cxx ? "c++" : "cc";
    node_warn(n, "Fallback compiler: %s", ctx->cc);
  } else if (g_env.verbose) {
    node_info(n, "Compiler: %s", ctx->cc);
  }

  if (ctx->is_host && !ctx->n_cflags) {
    // Build tools are compiled with host compiler flags unless the rule
    // provides its own flags expression. Native builds fall back to the
    // detected compiler flags of the corresponding language.
    if (is_cxx) {
      ctx->cflags_default = node_env_get(n, "HOST_CXXFLAGS");
      if ((!ctx->cflags_default || *ctx->cflags_default == '\0')) {
        ctx->cflags_default = toolchain_id() ? node_env_get(n, "HOST_CFLAGS")
                                            : node_env_get(n, "CXXFLAGS");
      }
    } else {
      ctx->cflags_default = node_env_get(n, "HOST_CFLAGS");
      if ((!ctx->cflags_default || *ctx->cflags_default == '\0') && !toolchain_id()) {
        ctx->cflags_default = node_env_get(n, "CFLAGS");
      }
    }
  }

  const char *objskey = 0;
  if (ctx->n_objects && ctx->n_objects->child) {
    objskey = node_value(ctx->n_objects->child);
  }
  if (!objskey) {
    if (ctx->is_host) {
      objskey = is_cxx ? "HOST_CXX_OBJS" : "HOST_CC_OBJS";
    } else {
      objskey = is_cxx ? "CXX_OBJS" : "CC_OBJS";
    }
  }
  if (g_env.verbose) {
    node_info(n, "Objects in ${%s}", objskey);
  }

  ctx->objskey = pool_strdup(ctx->pool, objskey);
  char *objs = ulist_to_vlist(&ctx->objects);
  node_env_set(n, ctx->objskey, objs);
  free(objs);

  _cc_modules_setup(ctx, is_cxx);
}

static void _cc_init(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "consumes") == 0) {
      ctx->n_consumes = nn;
      continue;
    } else if (strcmp(nn->value, "objects") == 0) {
      ctx->n_objects = nn;
      continue;
    } else if (strcmp(nn->value, "modules") == 0) {
      ctx->n_modules = nn;
      continue;
    }
    if (!ctx->n_sources) {
      ctx->n_sources = nn;
      continue;
    }
    if (!ctx->n_cflags) {
      ctx->n_cflags = nn;
      continue;
    }
    if (!ctx->n_cc) {
      ctx->n_cc = nn;
    }
  }
  if (!ctx->n_sources) {
    node_fatal(AK_ERROR_SCRIPT, n, "No sources specified");
  }
}

static void _cc_dispose(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  if (ctx) {
    for (int i = 0; i < ctx->objects.num; ++i) {
      char *p = *(char**) ulist_get(&ctx->objects, i);
      free(p);
    }
    cxx_modules_destroy(ctx->modules);
    ulist_destroy_keep(&ctx->sources);
    ulist_destroy_keep(&ctx->objects);
    ulist_destroy_keep(&ctx->consumes);
    pool_destroy(ctx->pool);
  }
}

int node_cc_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->init = _cc_init;
  n->setup = _cc_setup;
  n->build = _cc_build;
  n->post_build = _cc_post_build;
  n->dispose = _cc_dispose;
  struct pool *pool = pool_create_empty();
  struct _cc_ctx *ctx = pool_alloc(pool, sizeof(*ctx));
  *ctx = (struct _cc_ctx) {
    .pool = pool,
    .n = n,
    .sources = { .usize = sizeof(char*) },
    .objects = { .usize = sizeof(char*) },
    .consumes = { .usize = sizeof(char*) },
  };
  n->impl = ctx;
  return 0;
}
