#include "script.h"
#include "xstr.h"
#include "utils.h"

static void _init(struct node *n) {
  const char *name = node_value(n->child);
  if (!name) {
    return;
  }
  struct node *nn = n->child->next;
  if (!nn) {
    node_env_set(n, name, "");
    return;
  }
  if (!nn->next && nn->value[0] != '.') { // Single value
    node_env_set(n, name, node_value(nn));
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
      xstr_cat(xstr, "\1");
      xstr_cat(xstr, v);
    }
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}

int node_set_setup(struct node *n) {
  n->init = _init;
  return 0;
}
