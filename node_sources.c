#include "script.h"

static int _settle(struct node *n) {
  return 0;
}

static int _build(struct node *n, int *updated) {
  return 0;
}

static void _dispose(struct node *n) {
}

int node_sources_setup(struct node *n) {
  n->settle = _settle;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}

