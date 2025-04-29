#include "script.h"

static void _setup(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_exec_setup(struct node *n) {
  n->setup = _setup;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}

int node_static_setup(struct node *n) {
  return node_exec_setup(n);
}

int node_shared_setup(struct node *n) {
  return node_exec_setup(n);
}


