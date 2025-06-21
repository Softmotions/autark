#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "log.h"
#endif

static void _error_setup(struct node *n) {
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_value(nn)) {
      if (xstr_size(xstr)) {
        xstr_cat2(xstr, " ", 1);
      }
      xstr_cat(xstr, node_value(nn));
    }
  }
  node_fatal(AK_ERROR_FAIL, n, xstr_ptr(xstr));
}

int node_error_setup(struct node *n) {
  n->setup = _error_setup;
  return 0;
}
