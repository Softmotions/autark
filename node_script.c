#include "script.h"

#include <limits.h>
#include <unistd.h>
#include <libgen.h>

static void _init(struct node *n) {
  int c;
  do {
    c = 0;
    for (struct node *nn = n->child; nn; nn = nn->next) {
      if (!node_is_init(nn) && node_is_included(nn)) {
        ++c;
        node_init(nn);
      }
    }
  } while (c > 0);
}

static void _setup(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_setup(nn);
  }
}

static void _build(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_build(nn);
  }
}

static void _dispose(struct node *n) {
}

int node_script_setup(struct node *n) {
  n->init = _init;
  n->setup = _setup,
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}
