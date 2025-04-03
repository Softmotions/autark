#include "script.h"

static int _resolve(struct node *n) {
  return 0;
}

static int _build(struct node *n) {
  return 0;
}

static void _dispose(struct node *n) {
}

int node_if_setup(struct node *n) {
  n->resolve = _resolve;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}


