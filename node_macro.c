#ifndef _AMALGAMATE_
#include "script.h"
#endif

static void _macro_remove(struct node *n) {
  struct node *prev = node_find_prev_sibling(n);
  if (prev) {
    prev->next = n->next;
  } else {
    n->parent->child = n->next;
  }
}

static void _macro_init(struct node *n) {
  struct unit *unit = unit_peek();
  const char *key = node_value(n->child);
  if (!key || key[0] == '\0') {
    node_warn(n, "No name specified for 'macro' directive");
    return;
  }
  _macro_remove(n);
  unit_env_set_node(unit, key, n);
}

int node_macro_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _macro_init;
  return 0;
}
