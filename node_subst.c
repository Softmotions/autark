#include "script.h"
#include "alloc.h"
#include <stdlib.h>

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
    vv = getenv(key);
    if (vv) {
      // For the sake of safety, save only copy of env variable
      if (n->impl) {
        if (strcmp(n->impl, vv) == 0) {
          return n->impl;
        }
        free(n->impl);
      }
      n->impl = xstrdup(vv);
      return n->impl;
    }
  }
  return "";
}

static void _dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

int node_subst_setup(struct node *n) {
  n->value_get = _value;
  n->dispose = _dispose;
  return 0;
}
