#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"

#include <stdlib.h>
#endif

static const char* _join_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }

  struct xstr *xstr = xstr_create_empty();
  bool list = n->value[0] == '.';
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (list) {
      xstr_cat(xstr, "\1");
    }
    xstr_cat(xstr, node_value(nn));
  }
  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}

static void _join_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

int node_join_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _join_value;
  n->dispose = _join_dispose;
  return 0;
}
