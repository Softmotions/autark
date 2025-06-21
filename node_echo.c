#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "xstr.h"
#include "utils.h"
#include <string.h>
#endif

struct _echo_ctx {
  struct node *n_init;
  struct node *n_setup;
  struct node *n_build;
};

static void _echo_item(struct xstr *xstr, const char *v, size_t len) {
  if (is_vlist(v)) {
    struct vlist_iter iter;
    vlist_iter_init(v, &iter);
    while (vlist_iter_next(&iter)) {
      _echo_item(xstr, iter.item, iter.len);
    }
  } else {
    if (xstr_size(xstr)) {
      xstr_cat2(xstr, " ", 1);
    }
    xstr_cat2(xstr, v, len);
  }
}

static void _echo(struct node *n) {
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      const char *v = node_value(nn);
      if (v) {
        _echo_item(xstr, v, strlen(v));
      }
    }
  }

  node_info(n, "%s", xstr_ptr(xstr));
  xstr_destroy(xstr);
}

static void _echo_init(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_init);
}

static void _echo_setup(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_setup);
}

static void _echo_build(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_build);
}

static void _echo_dispose(struct node *n) {
  free(n->impl);
}

int node_echo_setup(struct node *n) {
  struct _echo_ctx *ctx = xcalloc(1, sizeof(*ctx));
  n->impl = ctx;
  n->dispose = _echo_dispose;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_BAG) {
      if (!n->build && strcmp(nn->value, "build") == 0) {
        n->build = _echo_build;
        ctx->n_build = nn;
      } else if (!n->setup && strcmp(nn->value, "setup") == 0) {
        n->setup = _echo_setup;
        ctx->n_setup = nn;
      } else if (!n->init && strcmp(nn->value, "init") == 0) {
        n->init = _echo_init;
        ctx->n_init = nn;
      }
    }
  }
  if (!(n->init || n->setup || n->build)) {
    n->build = _echo_build;
    ctx->n_build = n;
  }
  return 0;
}
