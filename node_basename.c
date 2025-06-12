#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "xstr.h"

#include <stdlib.h>
#include <string.h>
#endif

static const char* _basename_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }

  const char *val = 0;
  const char *replace_ext = 0;
  if (!n->child || !(val = node_value(n->child))) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Argument is not set");
  }
  if (n->child->next) {
    replace_ext = node_value(n->child);
  }
  struct xstr *xstr = xstr_create_empty();
  const char *dp = strrchr(val, '.');
  if (dp) {
    xstr_cat2(xstr, val, (dp - val));
  } else {
    xstr_cat(xstr, val);
  }
  if (replace_ext) {
    xstr_cat(xstr, replace_ext);
  }
  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}

static void _basename_dispose(struct node *n) {
  free(n->impl);
}

int node_basename_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _basename_value;
  n->dispose = _basename_dispose;
  return 0;
}
