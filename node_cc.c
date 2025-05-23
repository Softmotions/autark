#include "script.h"
#include "ulist.h"
#include "pool.h"
#include "log.h"
#include "paths.h"
#include "env.h"

#include <string.h>
#include <errno.h>

struct _ctx {
  struct pool *pool;
  struct ulist sources; // char*
  struct ulist objects; // char*
  struct node *n_cflags;
  struct node *n_sources;
  struct node *n_cc;
};

static void _build(struct node *n) {
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
  char *npath = path_normalize_cwd(path_, unit->dir, buf);
  if (!npath) {
    node_fatal(errno, n, 0);
  }
  char *path = pool_strdup(ctx->pool, npath);
  ulist_push(&ctx->sources, &path);

  npath = path_normalize_cwd(path_, unit->cache_dir, buf);
  path = pool_strdup(ctx->pool, npath);
  char *p = strrchr(path, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';
  ulist_push(&ctx->objects, &path);
}

static void _setup(struct node *n) {
  struct _ctx *ctx = n->impl;
  const char *val = node_value(ctx->n_sources);
  struct pool *pool = pool_create_empty();
  if (env_value_is_list(val)) {
    for (char **pp = env_value_to_clist(val, ctx->pool); *pp; ++pp) {
      _source_add(n, *pp);
    }
  } else {
    _source_add(n, pool_strdup(ctx->pool, val));
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
    ulist_destroy_keep(&ctx->objects);
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
    .sources = { .usize = sizeof(char*) },
    .objects = { .usize = sizeof(char*) },
  };
  n->impl = ctx;
  return 0;
}
