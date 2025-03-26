
#include "project.h"

static int _settle(struct node *n) {
  return 0;
}

static int _build(struct node *n, int *updated) {
  return 0;
}

static void _dispose(struct node *n) {
}

int node_exec_setup(struct node *n) {
  n->settle = _settle;
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


