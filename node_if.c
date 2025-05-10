#include "script.h"
#include "log.h"
#include "env.h"

#include <string.h>

static bool _defined_eval(struct node *mn) {
  struct node *n = mn->parent;
  const char *val = node_value(mn->child);
  if (val && node_env_get(n, val)) {
    return true;
  } else {
    return false;
  }
}

static bool _matched_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->child) : 0;
  if (val1 && val2) {
    return strcmp(val1, val2) == 0;
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}

static bool _cond_eval(struct node *n, struct node *mn) {
  const char *op = node_value(mn);
  if (!op) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Matching condition is not set");
  }
  bool matched = false;
  bool neg = false;
  if (*op == '!') {
    neg = true;
    ++op;
  }
  if (strcmp(op, "matched") == 0) {
    matched = _matched_eval(mn);
  } else if (strcmp(op, "defined") == 0) {
    matched = _defined_eval(mn);
  } else {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Unknown matching condition: %s", op);
  }
  if (neg) {
    matched = !matched;
  }
  if (g_env.verbose) {
    node_info(n, "Evaluated to: %s", (matched ? "true" : "false"));
  }
  return matched;
}

static inline bool _node_is_else(struct node *n) {
  return n && n->value && strcmp(n->value, "else") == 0;
}

static void _pull_else(struct node *n) {
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = next;

  if (nn && nn->child && _node_is_else(nn)) { // We have attached else
    next = nn->next;
    nn = nn->child;
    n->next = nn;
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    prev->next = next;
  } else {
    n->parent->child = next;
  }
}

static void _pull_if(struct node *n) {
  struct node *mn = n->child;
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = mn->next;

  if (next && next->child && _node_is_else(next)) {
    next = next->next; // Skip else block
  }
  if (nn) {
    n->next = nn;
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    prev->next = next;
  } else {
    n->parent->child = next;
  }
}

static void _init(struct node *n) {
  struct node *mn = n->child; // Match node
  if (!mn) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "'if {...}' must have a condition clause");
  }
  bool matched = _cond_eval(n, mn);
  if (matched) {
    _pull_if(n);
  } else {
    _pull_else(n);
  }
  n->init = 0; // Protect me from second call in any way
}

int node_if_setup(struct node *n) {
  n->init = _init;
  return 0;
}
