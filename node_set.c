#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"
#include "alloc.h"
#endif

static void _set_init(struct node *n) {
  const char *name = node_value(n->child);
  if (!name) {
    return;
  }
  node_env_set_node(n, name);
}

static const char* _set_value_get(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    if ((uintptr_t) n->impl != (uintptr_t) -1) {
      free(n->impl);
    }
    n->impl = 0;
  }

  if ((uintptr_t) n->impl == (uintptr_t) -1) {
    return 0;
  } else if (n->impl) {
    return n->impl;
  }

  n->impl = (void*) (uintptr_t) -1;

  struct node *nn = n->child->next;
  if (!nn) {
    n->impl = xstrdup("");
    return n->impl;
  }

  if (!nn->next && nn->value[0] != '.') { // Single value
    const char *v = node_value(nn);
    n->impl = xstrdup(v);
    return n->impl;
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

  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}

static void _set_dispose(struct node *n) {
  if ((uintptr_t) n->impl != (uintptr_t) -1) {
    free(n->impl);
  }
  n->impl = 0;
}

int node_set_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _set_init;
  n->value_get = _set_value_get;
  n->dispose = _set_dispose;
  return 0;
}
