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
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };

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

  for (int i = 0; i < slist->num; ++i) {
    char *obj, *src = *(char**) ulist_get(slist, i);
    bool incache = strstr(src, unit->cache_dir) == src;
    if (!incache) {
      obj = path_relativize_cwd(unit->dir, src, unit->dir);
    } else {
      obj = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    }
    char *p = strrchr(obj, '.');
    akassert(p && p[1] != '\0');
    p[1] = 'o';
    p[2] = '\0';

    _on_build_source(ctx->n, 0, src, obj);
    free(obj);
  }

  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }

  for (int i = 0; i < ctx->sources.num; ++i) {
    char *src = *(char**) ulist_get(&ctx->sources, i);
    deps_add(&deps, DEPS_TYPE_FILE, 's', src);
  }
  node_add_unit_deps(&deps);
  deps_close(&deps);
  ulist_destroy_keep(&rlist);
}

static void _build(struct node *n) {
  struct _ctx *ctx = n->impl;

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

  node_resolve(&(struct node_resolve) {
    .path = n->vfile,
    .user_data = ctx,
    .on_resolve = _on_resolve,
  });
}

static void _source_add(struct node *n, const char *src) {
  char buf[PATH_MAX], *p;
  struct _ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  {
    if (src == 0 || *src == '\0') {
      return;
    }
    p = strrchr(src, '.');
    if (p == 0 || p[1] == '\0') {
      return;
    }
  }

  char *npath = path_normalize_cwd(src, unit->dir, buf);
  if (!path_is_exist(npath)) {
    npath = path_normalize_cwd(src, unit->cache_dir, buf);
  }

  char *nsrc = pool_strdup(ctx->pool, npath);
  ulist_push(&ctx->sources, &nsrc);

  npath = path_normalize_cwd(src, unit->cache_dir, buf);
  p = strrchr(buf, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';

  node_product_add_raw(n, buf);
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
    _source_add(n, val);
  }
  if (ctx->n_cc) {
    ctx->cc = pool_strdup(ctx->pool, node_value(ctx->n_cc));
  }
  if (!ctx->cc) {
    const char *key = strcmp(n->value, "cc") ? "CC" : "CXX";
    if (key) {
      ctx->cc = pool_strdup(ctx->pool, getenv(key));
      if (ctx->cc) {
        node_warn(n, "Found %s compiler in environment: %s", ctx->cc, key);
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
    ulist_destroy_keep(&ctx->sources);
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
  };
  n->impl = ctx;
  return 0;
}
