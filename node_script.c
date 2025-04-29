#include "script.h"

#include <limits.h>
#include <unistd.h>
#include <libgen.h>

static void _setup(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_script_setup(struct node *n) {
  n->setup = _setup;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}
