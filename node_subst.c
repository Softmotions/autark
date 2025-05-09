#include "script.h"

static void _setup(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_subst_setup(struct node *n) {
  n->setup = _setup;
  n->dispose = _dispose;
  return 0;
}

