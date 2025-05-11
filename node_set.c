#include "script.h"

/// set { CFLAGS ${CFLAGS} -O1 }

static void _init(struct node *n) {

}

int node_set_setup(struct node *n) {
  n->init = _init;
  return 0;
}
