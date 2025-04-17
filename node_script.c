#include "script.h"

#include <limits.h>
#include <unistd.h>
#include <libgen.h>

static void _resolve(struct node *n) {
}

static void _build(struct node *n) {
}

static void _dispose(struct node *n) {
}

int node_script_setup(struct node *n) {
  n->resolve = _resolve;
  n->build = _build;
  n->dispose = _dispose;
  return 0;
}
