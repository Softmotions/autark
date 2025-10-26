#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "utils.h"
#endif

#define MACRO_MAX_ARGS_NUM 64

struct _macro {
  struct node *args[MACRO_MAX_ARGS_NUM];
};

static int _macro_args_visitor(struct node *n, int lvl, void *ctx) {
  if (lvl < 0) {
    return 0;
  }
  struct _macro *m = n->impl;
  if (!(n->value[0] == '&' && n->value[1] == '\0' && n->child)) {
    return 0;
  }
  int rc = 0;
  int idx = utils_strtol(n->child->value, 10, &rc);
  if (rc || idx < 1 || idx > MACRO_MAX_ARGS_NUM) {
    node_fatal(0, n, "Invalid macro argument index");
    return 0;
  }
  idx--;
  m->args[idx] = n;
  return 0;
}

static void _macro_args_init(struct node *n) {
  akcheck(node_visit(n, 1, 0, _macro_args_visitor));
}

static void _macro_hide(struct node *n) {
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
  if (!key) {
    node_warn(n, "No name specified for 'macro' directive");
    return;
  }
  struct _macro *m = xcalloc(1, sizeof(*m));
  _macro_args_init(n);
  _macro_hide(n);
  unit_env_set_node(unit, key, n);
}

static void _macro_dispose(struct node *n) {
  struct _macro *m = n->impl;
  free(m);
}

int node_macro_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _macro_init;
  n->dispose = _macro_dispose;
  return 0;
}
