#include "script.h"

static void _init(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_flags_setup(struct node *n) {
  n->init = _init;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}

