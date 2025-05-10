#include "script.h"

static const char* _value(struct node *n) {
  if (n->child) {
    const char *key = node_value(n->child);
    if (!key) {
      return "";
    }
    const char *vv = node_env_get(n, key);
    if (vv) {
      return vv;
    }
  }
  return "";
}

int node_subst_setup(struct node *n) {
  n->value_get = _value;
  return 0;
}

