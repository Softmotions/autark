#ifndef _AMALGAMATE_
#include "script.h"
#endif

static void _in_sources_init(struct node *n) {
  // Remove me from the tree
  struct node *nn = n->child;
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  if (nn) {
    n->next = nn;
    n->child = 0;
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    prev->next = next;
  } else {
    n->parent->child = next;
  }
}

int node_in_sources_setup(struct node *n) {
  n->init = _in_sources_init;
  return 0;
}
