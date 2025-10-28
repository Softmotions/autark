#ifndef _AMALGAMATE_
#include "script.h"
#include "utils.h"
#include "alloc.h"
#include "ulist.h"
#endif

#define MACRO_MAX_ARGS_NUM 64

struct _call {
  const char  *key;    ///< Macro name
  struct node *n;      ///< Call Parent node
  struct node *mn;     ///< Macro node
  struct node *prev;   ///< Call Prev node
  struct node *cloned; ///< Macro cloned node
  struct ulist nodes;  ///< Nodes stack. struct node*
  struct node *args[MACRO_MAX_ARGS_NUM];
};

static int _call_macro_visit(struct node *n, int lvl, void *d) {
  struct _call *call = d;

  if (call->mn == n /*skip macro itself */ || call->mn->child == n /* skip macro name */) {
    return 0;
  }

  if (lvl < 0) {
    ulist_pop(&call->nodes);
    return 0;
  }

  // Macro arg
  if (n->value[0] == '&' && n->value[0] == '\0' && n->child && !n->child->next) {
    int rc = 0;
    int idx = utils_strtol(n->child->value, 10, &rc);
    if (rc || idx < 1 || idx >= MACRO_MAX_ARGS_NUM) {
      node_fatal(rc, n, "Invalid macro arg index: %d", idx);
      return 0;
    }
    idx--;
    n = call->args[idx];
    if (!n) {
      node_fatal(rc, call->n, "Call argument: %d for macro: %s is not set", (idx + 1), call->key);
      return 0;
    }
  }

  struct node *parent = *(struct node**) ulist_peek(&call->nodes);
  struct node *nn = node_clone_and_register(n);
  nn->parent = parent;

  // Add node to the parent
  if (!parent->child) {
    parent->child = nn;
  } else {
    struct node *c = parent->child;
    while (c && c->next) {
      c = c->next;
    }
    c->next = nn;
  }

  node_bind(nn);
  ulist_push(&call->nodes, &nn);
  return 0;
}

static void _call_init(struct node *n) {
  struct unit *unit = unit_peek();
  const char *key = node_value(n->child);
  if (!key) {
    node_warn(n, "No name specified for 'call' directive");
    return;
  }
  struct node *mn = unit_env_get_node(unit, key);
  if (!mn) {
    node_fatal(0, n, "Unknown script macro: %s", key);
    return;
  }
  struct _call *call = xcalloc(1, sizeof(*call));
  call->key = key;
  call->mn = mn;
  call->n = n;
  call->prev = node_find_prev_sibling(n);
  ulist_init(&call->nodes, 32, sizeof(struct node*));
  n->impl = call;

  int idx = 0;
  for (struct node *pn = n->child->next; pn; pn = pn->next) {
    if (idx >= MACRO_MAX_ARGS_NUM) {
      node_fatal(0, n, "Exceeded the maximum number of macro args: " Q_STR(MACRO_MAX_ARGS_NUM));
      return;
    }
    call->args[idx++] = pn;
  }

  ulist_push(&call->nodes, &n->parent);
  int rc = node_visit(mn, 1, call, _call_macro_visit);
  if (rc) {
    node_fatal(rc, n, 0);
  }
}

static void _call_dispose(struct node *n) {
  struct _call *call = n->impl;
  if (call) {
    ulist_destroy_keep(&call->nodes);
    free(call);
  }
}

int node_call_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _call_init;
  n->dispose = _call_dispose;
  return 0;
}
