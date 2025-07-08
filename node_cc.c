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
#include <string.h>
#endif

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
  const char  *cc;
  struct ulist consumes;    // sizeof(char*)
  int num_failed;
};

static void _cc_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}

static void _cc_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

static void _cc_deps_MMD_item_add(const char *item, struct node *n, struct deps *deps, const char *src) {
  char buf[127];
  char *p = strrchr(item, '.');
  if (!p || p[1] == '\0') {
    return;
  }
  ++p;
  char *ext = utils_strncpy(buf, p, sizeof(buf));
  if (*ext == 'c' || *ext == 'C' || *ext == 'm') {
    // Skip source files
    return;
  }
  deps_add_alias(deps, 's', src, item);
}

static void _cc_deps_MMD_add(struct node *n, struct deps *deps, const char *src, const char *obj) {
  char buf[MAX(2 * PATH_MAX, 8192)];
  size_t len = strlen(obj);
  utils_strncpy(buf, obj, sizeof(buf));

  char *p = strrchr(buf, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'd';
  p[2] = '\0';

  FILE *f = fopen(buf, "r");
  if (!f) {
    node_warn(n, "Failed to open compiler generated (-MMD) dependency file: %s", buf);
    return;
  }

  bool nl = false;

  while ((p = fgets(buf, sizeof(buf), f))) {
    if (!nl) {
      if (!utils_startswith(p, obj)) {
        continue;
      }
      p += len;
      if (*p++ != ':') {
        continue;
      }
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
      char *ep = sp;
      while (*p != '\0' && !utils_char_is_space(*p)) {
        ep = p;
        ++p;
      }
      if (ep > sp) {
        if (*p != '\0') {
          *p = '\0';
          ++p;
        }
        _cc_deps_MMD_item_add(sp, n, deps, src);
      }
    }
  }
  fclose(f);
}

static bool _cc_on_build_source(struct node *n, struct deps *deps, const char *src, const char *obj) {
  int ret = true;
  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: build src=%s obj=%s\n", n->name, src, obj);
  }
  struct _cc_ctx *ctx = n->impl;
  struct spawn *s = spawn_create(ctx->cc, ctx);
  spawn_set_stdout_handler(s, _cc_stdout_handler);
  spawn_set_stderr_handler(s, _cc_stderr_handler);
  if (ctx->n_cflags) {
    struct xstr *xstr = 0;
    const char *cflags = node_value(ctx->n_cflags);
    if (!is_vlist(cflags)) {
      xstr = xstr_create_empty();
      utils_split_values_add(cflags, xstr);
      cflags = xstr_ptr(xstr);
    }
    spawn_arg_add(s, cflags);
    xstr_destroy(xstr);
  }

  if (!spawn_arg_starts_with(s, "-M")) {
    spawn_arg_add(s, "-MMD");
  }

  spawn_arg_add(s, "-I./"); // Current cache dir
  spawn_arg_add(s, "-c");
  spawn_arg_add(s, src);

  spawn_arg_add(s, "-o");
  spawn_arg_add(s, obj);

  int rc = spawn_do(s);
  if (rc) {
    node_error(rc, ctx->n, "%s", ctx->cc);
    ret = false;
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_error(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", ctx->cc, code);
      ret = false;
    }
  }
  spawn_destroy(s);
  return ret;
}

static void _cc_on_resolve(struct node_resolve *r) {
  struct deps deps;
  struct _cc_ctx *ctx = r->user_data;
  struct unit *unit = unit_peek();
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };
  struct map *fmap = map_create_str(map_k_free);

  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') { // Rebuild all on any outdated non source dependency
        slist = &ctx->sources;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }

  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(&deps);

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

  for (int i = 0; i < slist->num; ++i) {
    char buf[PATH_MAX];
    char *obj, *src = *(char**) ulist_get(slist, i);
    src = path_normalize_cwd(src, unit->cache_dir, buf);
    bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
    if (!incache) {
      obj = path_relativize_cwd(unit->dir, src, unit->dir);
      src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    } else {
      obj = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      src = xstrdup(obj);
    }

    char *p = strrchr(obj, '.');
    akassert(p && p[1] != '\0');
    p[1] = 'o';
    p[2] = '\0';

    bool failed = !_cc_on_build_source(ctx->n, &deps, src, obj);
    if (failed) {
      ++ctx->num_failed;
      map_put_str(fmap, src, (void*) (intptr_t) 1);
    }

    if (slist == &ctx->sources) {
      deps_add(&deps, failed ? DEPS_TYPE_FILE_OUTDATED : DEPS_TYPE_FILE, 's', src, 0);
      if (!failed) {
        _cc_deps_MMD_add(ctx->n, &deps, src, obj);
      }
    } else {
    }

    free(obj);
    free(src);
  }

  if (slist != &ctx->sources) {
    for (int i = 0; i < ctx->sources.num; ++i) {
      char buf[PATH_MAX];
      char *obj, *src = *(char**) ulist_get(&ctx->sources, i);
      src = path_normalize_cwd(src, unit->cache_dir, buf);
      bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
      if (!incache) {
        obj = path_relativize_cwd(unit->dir, src, unit->dir);
        src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      } else {
        obj = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
        src = xstrdup(obj);
      }
      char *p = strrchr(obj, '.');
      akassert(p && p[1] != '\0');
      p[1] = 'o';
      p[2] = '\0';

      bool failed = map_get(fmap, src) != 0;
      deps_add(&deps, failed ? DEPS_TYPE_FILE_OUTDATED : DEPS_TYPE_FILE, 's', src, 0);
      if (!failed) {
        _cc_deps_MMD_add(ctx->n, &deps, src, obj);
      }

      free(obj);
      free(src);
    }
  }

  deps_close(&deps);
  ulist_destroy_keep(&rlist);
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

static void _cc_build(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
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

  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = ctx,
    .on_init = _cc_on_resolve_init,
    .on_resolve = _cc_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };

  if (node_is_value_may_be_dep_saved(ctx->n_cc)) {
    ulist_push(&r.node_val_deps, &ctx->n_cc);
  }

  if (node_is_value_may_be_dep_saved(ctx->n_cflags)) {
    ulist_push(&r.node_val_deps, &ctx->n_cflags);
  }

  if (node_is_value_may_be_dep_saved(ctx->n_sources)) {
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

  bool incache = path_is_prefix_for(g_env.project.cache_dir, p, unit->dir);
  const char *dir = incache ? unit->cache_dir : unit->dir;
  char *obj = path_relativize_cwd(dir, p, dir);

  p = strrchr(obj, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';
  ulist_push(&ctx->objects, &obj);
  node_product_add(n, obj, 0);
}

static void _cc_setup(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
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
    const char *key = strcmp(n->value, "cc") == 0 ? "CC" : "CXX";
    if (key) {
      ctx->cc = pool_strdup(ctx->pool, node_env_get(n, key));
      if (g_env.verbose && ctx->cc) {
        node_info(n, "Found '%s' compiler in ${%s}", ctx->cc, key);
      }
    }
  }
  if (!ctx->cc) {
    ctx->cc = "cc";
    node_warn(n, "Fallback compiler: %s", ctx->cc);
  } else if (g_env.verbose) {
    node_info(n, "Compiler: %s", ctx->cc);
  }

  const char *objskey = 0;
  if (ctx->n_objects && ctx->n_objects->child) {
    objskey = node_value(ctx->n_objects->child);
  }
  if (!objskey) {
    if (strcmp(n->value, "cc") == 0) {
      objskey = "CC_OBJS";
    } else {
      objskey = "CXX_OBJS";
    }
  }
  if (g_env.verbose) {
    node_info(n, "Objects in ${%s}", objskey);
  }
  char *objs = ulist_to_vlist(&ctx->objects);
  node_env_set(n, objskey, objs);
  free(objs);
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
