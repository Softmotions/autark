#include "script.h"

static void _resolve(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_if_setup(struct node *n) {
  n->setup = _resolve;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}


