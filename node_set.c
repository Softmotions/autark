#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"
#include "alloc.h"
#include "env.h"
#endif

static const char* _set_value_get(struct node *n);

static struct unit* _unit_for_set(struct node *n, struct node *nn, const char **keyp) {
  if (nn->type == NODE_TYPE_BAG) {
    if (strcmp(nn->value, "root") == 0) {
      *keyp = node_value(nn->child);
      return unit_root();
    } else if (strcmp(nn->value, "parent") == 0) {
      *keyp = node_value(nn->child);
      return unit_parent(n);
    }
  } else {
    *keyp = node_value(nn);
  }
  return unit_peek();
}

static void _set_setup(struct node *n) {
  if (n->child && strcmp(n->value, "env") == 0) {
    const char *v = _set_value_get(n);
    if (v) {
      const char *key = 0;
      _unit_for_set(n, n->child, &key);
      if (key) {
        if (g_env.verbose) {
          node_info(n, "%s=%s", key, v);
        }
        setenv(key, v, 1);
      }
    }
  }
}

static void _set_init(struct node *n) {
  const char *key = 0;
  struct unit *unit = n->child ? _unit_for_set(n, n->child, &key) : 0;
  if (!key) {
    node_warn(n, "No name specified for 'set' directive");
    return;
  }
  struct node *nn = unit_env_get_node(unit, key);
  if (nn) {
    n->recur_next.n = nn;
  }
  unit_env_set_node(unit, key, n);
}

static const char* _set_value_get(struct node *n) {
  if (n->recur_next.active && n->recur_next.n) {
    return _set_value_get(n->recur_next.n);
  }
  n->recur_next.active = true;

  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    if ((uintptr_t) n->impl != (uintptr_t) -1) {
      free(n->impl);
    }
    n->impl = 0;
  }

  if ((uintptr_t) n->impl == (uintptr_t) -1) {
    n->recur_next.active = false;
    return 0;
  } else if (n->impl) {
    n->recur_next.active = false;
    return n->impl;
  }

  n->impl = (void*) (uintptr_t) -1;

  struct node *nn = n->child->next;
  if (!nn) {
    n->impl = xstrdup("");
    n->recur_next.active = false;
    return n->impl;
  }

  if (!nn->next && nn->value[0] != '.') { // Single value
    const char *v = node_value(nn);
    n->impl = xstrdup(v);
    n->recur_next.active = false;
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
  n->recur_next.active = false;
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
  n->setup = _set_setup;
  n->value_get = _set_value_get;
  n->dispose = _set_dispose;
  return 0;
}
