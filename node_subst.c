#include "script.h"
#include "alloc.h"

#include <stdlib.h>

static const char* _setval(struct node *n, const char *vv, const char *dv) {
  if (vv) {
    if (n->impl) {
      if (strcmp(n->impl, vv) == 0) {
        return n->impl;
      }
      free(n->impl);
    }
    n->impl = xstrdup(vv);
    return n->impl;
  } else {
    return dv;
  }
}

static const char* _value(struct node *n) {
  if (n->child) {
    const char *dv_ = node_value(n->child->next);
    const char *dv = dv_;
    if (!dv) {
      dv = "";
    }
    const char *key = node_value(n->child);
    if (!key) {
      node_warn(n, "No key specified");
      return _setval(n, 0, dv);
    }
    const char *vv = node_env_get(n, key);
    if (vv) {
      return _setval(n, vv, 0);
    }
    const char *ev = getenv(key);
    if (!ev && !dv_) {
      node_warn(n, "${%s} env variable not found", key);
    }
    return _setval(n, ev, dv);
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
