#include "script.h"
#include "xstr.h"
#include "utils.h"
#include "alloc.h"

static void _init(struct node *n) {
  const char *name = node_value(n->child);
  if (!name) {
    return;
  }
  struct node *nn = n->child->next;
  if (!nn) {
    node_env_set(n, name, "");
    n->impl = xstrdup("");
    return;
  }
  if (!nn->next && nn->value[0] != '.') { // Single value
    const char *v = node_value(nn);
    node_env_set(n, name, v);
    n->impl = xstrdup(v);
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  for ( ; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (!v) {
      v = "";
    }
    if (nn->value[0] == '.' && nn->value[1] == '.') {
      utils_split_values_add(v, xstr);
    } else {
      if (!is_vlist(v)) {
        xstr_cat(xstr, "\1");
      }
      xstr_cat(xstr, v);
    }
  }
  node_env_set(n, name, xstr_ptr(xstr));
  n->impl = xstr_destroy_keep_ptr(xstr);
}

static const char* _value_get(struct node *n) {
  return n->impl;
}

static void _dispose(struct node *n) {
  free(n->impl);
}

int node_set_setup(struct node *n) {
  n->init = _init;
  n->dispose = _dispose;
  n->value_get = _value_get;
  return 0;
}
