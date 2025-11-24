#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "alloc.h"
#include "config.h"
#endif

struct _macro_state {
  int num_calls;
};

void macro_register_call(struct node *mn) {
  akassert(mn->type == NODE_TYPE_MACRO);
  struct _macro_state *s = mn->impl;
  if (!s) {
    s = xmalloc(sizeof(*s));
    s->num_calls = 0;
    mn->impl = s;
  }
  ++s->num_calls;
  if (s->num_calls >= MACRO_MAX_RECURSIVE_CALLS) {
    node_fatal(AK_ERROR_MACRO_MAX_RECURSIVE_CALLS, mn, 0);
  }
}

void macro_unregister_call(struct node *mn) {
  akassert(mn->type == NODE_TYPE_MACRO);
  struct _macro_state *s = mn->impl;
  akassert(s);
  --s->num_calls;
  akassert(s->num_calls >= 0);
}

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
  unit_env_set_node(unit, key, n, TAG_INIT);
}

static void _macro_dispose(struct node *n) {
  struct _macro_state *s = n->impl;
  if (s) {
    free(s);
  }
}

int node_macro_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _macro_init;
  n->dispose = _macro_dispose;
  return 0;
}
