#include "script.h"
#include "ulist.h"
#include "pool.h"
#include "log.h"
#include "paths.h"
#include "env.h"
#include "utils.h"
#include "xstr.h"
#include "spawn.h"

#include <string.h>

struct _ctx {
  struct pool *pool;
  struct ulist sources;     // char*
  struct ulist sources_rel; // char*
  struct ulist objects;     // char*
  struct ulist objects_rel; // char*
  struct node *n;
  struct node *n_cflags;
  struct node *n_sources;
  struct node *n_cc;
  const char  *cc;
};

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

static void _on_build_source(struct node *n, struct deps *deps, const char *src, const char *obj) {
  struct _ctx *ctx = n->impl;
  struct spawn *s = spawn_create(ctx->cc, ctx);
  spawn_set_stdout_handler(s, _stdout_handler);
  spawn_set_stderr_handler(s, _stderr_handler);
  if (ctx->n_cflags) {
    struct xstr *xstr = 0;
    const char *cflags = node_value(ctx->n_cflags);
    if (!utils_is_list_value(cflags)) {
      xstr = xstr_create_empty();
      utils_split_values_add(cflags, xstr);
      cflags = xstr_ptr(xstr);
    }
    for (char **p = utils_list_value_to_clist(cflags, ctx->pool); *p; ++p) {
      spawn_arg_add(s, *p);
    }
    xstr_destroy(xstr);
  }

  spawn_arg_add(s, "-c");
  spawn_arg_add(s, src);

  spawn_arg_add(s, "-o");
  spawn_arg_add(s, obj);

  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, ctx->n, "%s", ctx->cc);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", ctx->cc, code);
    }
  }
  spawn_destroy(s);
}

static void _on_resolve(struct node_resolve *r) {
  struct _ctx *ctx = r->user_data;
  struct unit *unit = unit_peek();
  struct ulist *slist = &ctx->sources_rel;
  if (r->deps_outdated.num) {
    slist = &r->deps_outdated;
    for (int i = 0; i < slist->num; ++i) {
      char *path = *(char**) ulist_get(slist, i);
      char *rpath = path_relativize_cwd(unit->cache_dir, path, unit->cache_path);
      ulist_set(slist, i, pool_strdup(r->pool, rpath));
      free(rpath);
    }
  }

  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }

  for (int i = 0; i < slist->num; ++i) {
    char *path = *(char**) ulist_get(slist, i);
    char *obj = *(char**) ulist_get(&ctx->objects_rel, i);
    _on_build_source(ctx->n, 0, path, obj);
    path = *(char**) ulist_get(&ctx->sources, i);
    deps_add(&deps, DEPS_TYPE_FILE, path);
  }

  node_add_unit_deps(&deps);
  deps_close(&deps);
}

static void _build(struct node *n) {
  char buf[PATH_MAX];
  struct _ctx *ctx = n->impl;
  struct unit *unit = unit_peek();

  for (int i = 0; i < ctx->sources.num; ++i) {
    const char *src = *(char**) ulist_get(&ctx->sources, i);
    char *npath = path_normalize_cwd(src, unit->dir, buf);
    if (!path_is_exist(npath)) {
      npath = path_normalize_cwd(src, unit->cache_dir, buf);
      struct node *pn = node_by_product_raw(n, npath);
      if (pn) {
        node_build(pn);
      } else {
        node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "'%s'", npath);
      }
    }
    src = pool_strdup(ctx->pool, npath);
    ulist_set(&ctx->sources, i, &src);

    char *rpath = path_relativize_cwd(unit->cache_dir, npath, unit->cache_dir);
    ulist_push(&ctx->sources_rel, &rpath);
  }

  node_resolve(&(struct node_resolve) {
    .path = n->vfile,
    .user_data = ctx,
    .on_resolve = _on_resolve,
  });
}

static void _source_add(struct node *n, char *path_) {
  char buf[PATH_MAX];
  struct _ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  {
    if (path_ == 0 || *path_ == '\0') {
      return;
    }
    char *p = strrchr(path_, '.');
    if (p == 0 || p[1] == '\0') {
      return;
    }
  }

  // Save raw source
  ulist_push(&ctx->sources, &path_);

  // Save object files as products
  char *npath = path_normalize_cwd(path_, unit->cache_dir, buf);
  char *path = pool_strdup(ctx->pool, npath);
  char *p = strrchr(path, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';

  ulist_push(&ctx->objects, &path);
  node_product_add_raw(n, path);

  char *rpath = path_relativize_cwd(unit->cache_dir, path, unit->cache_dir);
  ulist_push(&ctx->objects_rel, &rpath);
}

static void _setup(struct node *n) {
  struct _ctx *ctx = n->impl;
  const char *val = node_value(ctx->n_sources);
  struct pool *pool = pool_create_empty();
  if (utils_is_list_value(val)) {
    for (char **pp = utils_list_value_to_clist(val, ctx->pool); *pp; ++pp) {
      _source_add(n, *pp);
    }
  } else {
    _source_add(n, pool_strdup(ctx->pool, val));
  }
  if (ctx->n_cc) {
    ctx->cc = pool_strdup(ctx->pool, node_value(ctx->n_cc));
  }
  if (!ctx->cc) {
    const char *ename = strcmp(n->value, "cc") ? "CC" : "CXX";
    if (ename) {
      ctx->cc = pool_strdup(ctx->pool, ename);
      if (!ctx->cc) {
        node_warn(n, "Found %s compiler in environment: %s", ename, ctx->cc);
      }
    }
  }
  if (!ctx->cc) {
    ctx->cc = "cc";
    node_warn(n, "Fallback compiler: %s", ctx->cc);
  } else {
    node_info(n, "Compiler: %s", ctx->cc);
  }
  pool_destroy(pool);
}

static void _init(struct node *n) {
  struct _ctx *ctx = n->impl;
  ctx->n_sources = n->child;
  if (!ctx->n_sources) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "No sources specified");
  }
  if (ctx->n_sources->next) {
    ctx->n_cflags = ctx->n_sources->next;
  }
  if (ctx->n_cflags) {
    ctx->n_cc = ctx->n_cflags->next;
  }
}

static void _dispose(struct node *n) {
  struct _ctx *ctx = n->impl;
  if (ctx) {
    for (int i = 0; i < ctx->sources_rel.num; ++i) {
      char *v = *(char**) ulist_get(&ctx->sources_rel, i);
      free(v);
    }
    for (int i = 0; i < ctx->objects_rel.num; ++i) {
      char *v = *(char**) ulist_get(&ctx->objects_rel, i);
      free(v);
    }
    ulist_destroy_keep(&ctx->sources);
    ulist_destroy_keep(&ctx->sources_rel);
    ulist_destroy_keep(&ctx->objects);
    ulist_destroy_keep(&ctx->objects_rel);
    pool_destroy(ctx->pool);
  }
}

int node_cc_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->init = _init;
  n->setup = _setup;
  n->build = _build;
  n->dispose = _dispose;
  struct pool *pool = pool_create_empty();
  struct _ctx *ctx = pool_alloc(pool, sizeof(*ctx));
  *ctx = (struct _ctx) {
    .pool = pool,
    .n = n,
    .sources = { .usize = sizeof(char*) },
    .sources_rel = { .usize = sizeof(char*) },
    .objects = { .usize = sizeof(char*) },
    .objects_rel = { .usize = sizeof(char*) },
  };
  n->impl = ctx;
  return 0;
}
