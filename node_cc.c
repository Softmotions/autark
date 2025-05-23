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

static void _source_add(struct node *n, struct node *ns) {
  char buf[PATH_MAX];
  const char *val = node_value(ns);
  if (val == 0 || *val == '\0' || strrchr(val, '.') == 0) {
    return;
  }
  const char *path = val;
  if (path_is_absolute(path)) {
    char *p = path_relativize_cwd(path, buf, g_env.project.root_dir);
    if (!p) {
      node_fatal(errno, n, "Failed to relativize path: %s", path);
    }
    path = p;
  }
}

static void _setup(struct node *n) {
  struct _ctx *ctx = n->impl;
  for (struct node *ns = ctx->n_sources; ns; ns = ns->next) {
    _source_add(n, ns);
  }
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
