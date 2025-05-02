#include "script.h"

#include <limits.h>
#include <unistd.h>
#include <libgen.h>

static void _setup(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_reset(nn);
  }
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_CHECK && !node_is_setup(nn) && node_is_included(nn)) {
      node_setup(nn);
    }
  }
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type != NODE_TYPE_CHECK && !node_is_setup(nn) && node_is_included(nn)) {
      node_setup(nn);
    }
  }
}

static void _setup2(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_setup2(nn);
  }
}

static void _build(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_is_included(nn)) {
      node_build(nn);
    }
  }
}

static void _dispose(struct node *n) {
}

int node_script_setup(struct node *n) {
  n->setup = _setup;
  n->setup2 = _setup2,
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}
