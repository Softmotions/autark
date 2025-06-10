#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "alloc.h"
#include "utils.h"
#endif

static void _foreach_setup(struct node *n) {
  if (!n->child) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Foreach must have a variable name as first parameter");
  }
   struct node_foreach *fe = xcalloc(1, sizeof(*fe));
  n->impl = fe;
  fe->name = xstrdup(node_value(n->child));

  struct xstr *xstr = xstr_create_empty();
  const char *v = node_value(n->child->next);
  if (v && *v != '\0') {
    xstr_cat(xstr, v);
  }
  if (!is_vlist(xstr_ptr(xstr))) {
    xstr_unshift(xstr, "\1", 1);
  }
  fe->items = xstr_destroy_keep_ptr(xstr);
}

static void _foreach_dispose(struct node *n) {
  struct node_foreach *fe = n->impl;
  if (fe) {
    free(fe->items);
    free(fe->name);
    free(fe);
  }
}

static void _foreach_init(struct node *n) {
}

int node_foreach_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->init = _foreach_init;
  n->setup = _foreach_setup;
  n->dispose = _foreach_dispose;
  return 0;
}
