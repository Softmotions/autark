#include "script.h"

static void _build(struct node *n) {
  fprintf(stderr, "!!!! CC\n");
}

int node_cc_setup(struct node *n) {
  n->build = _build;
  return 0;
}
