#include "script.h"
#include "ulist.h"
#include "pool.h"

struct _ctx {
  struct pool *pool;
  struct ulist sources; // char*
  struct ulist objects; // char*
};

static void _build(struct node *n) {
}

static void _init(struct node *n) {
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
