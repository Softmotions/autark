#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include <stdio.h>
#endif

int node_option_setup(struct node *n) {
  if (!n->child) {
    return 0;
  }
  const char *name = n->child->value;
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child->next; nn; nn = nn->next) {
    if (xstr_size(xstr)) {
      xstr_cat2(xstr, " ", 1);
    }
    xstr_cat(xstr, nn->value);
  }
  fprintf(stdout, "%s: %s\n", name, xstr_ptr(xstr));
  xstr_destroy(xstr);
  return 0;
}
