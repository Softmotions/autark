#include "script.h"

static void _setup(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_sources_setup(struct node *n) {
  n->init = _setup;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}

