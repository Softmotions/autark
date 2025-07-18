#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "env.h"
#include "utils.h"
#include <string.h>
#endif

static bool _if_defined_eval(struct node *mn) {
  struct node *n = mn->parent;
  for (struct node *nn = mn->child; nn; nn = nn->next) {
    const char *val = node_value(mn->child);
    if (val && *val != '\0' && node_env_get(n, val)) {
      return true;
    }
  }
  return false;
}

static bool _if_matched_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return strcmp(val1, val2) == 0;
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}

static bool _if_prefix_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return utils_startswith(val1, val2);
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}

static bool _if_contains_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return strstr(val1, val2);
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}

static bool _if_cond_eval(struct node *n, struct node *mn);

static bool _if_OR_eval(struct node *n, struct node *mn) {
  for (struct node *nn = mn->child; nn; nn = nn->next) {
    if (_if_cond_eval(n, nn)) {
      return true;
    }
  }
  return false;
}

static bool _if_AND_eval(struct node *n, struct node *mn) {
  for (struct node *nn = mn->child; nn; nn = nn->child) {
    if (!_if_cond_eval(n, nn)) {
      return false;
    }
  }
  return mn->child != 0;
}

static bool _if_cond_eval(struct node *n, struct node *mn) {
  if (node_is_value(mn)) {
    return true;
  }
  const char *op = mn->value;
  if (!op) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Matching condition is not set");
  }
  bool eq = false;
  bool neg = false;
  if (mn->flags & NODE_FLG_NEGATE) {
    neg = true;
  }
  if (mn->type == NODE_TYPE_BAG) {
    if (*op == '!') {
      ++op;
    }
    if (strcmp(op, "eq") == 0) {
      eq = _if_matched_eval(mn);
    } else if (strcmp(op, "defined") == 0) {
      eq = _if_defined_eval(mn);
    } else if (strcmp(op, "or") == 0) {
      eq = _if_OR_eval(n, mn);
    } else if (strcmp(op, "and") == 0) {
      eq = _if_AND_eval(n, mn);
    } else if (strcmp(op, "prefix") == 0) {
      eq = _if_prefix_eval(mn);
    } else if (strcmp(op, "contains") == 0) {
      eq = _if_contains_eval(mn);
    } else {
      node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Unknown matching condition: %s", op);
    }
  } else if (node_is_can_be_value(mn)) {
    op = node_value(mn);
    if (is_vlist(op)) {
      struct vlist_iter iter;
      vlist_iter_init(op, &iter);
      if (vlist_iter_next(&iter) && !(iter.len == 1 && iter.item[0] == '0')) {
        eq = iter.len > 0 || vlist_iter_next(&iter);
      }
    } else {
      eq = (op && *op != '\0' && !(op[0] == '0' && op[1] == '\0'));
    }
  } else {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Unknown matching condition: %s", op);
  }
  if (neg) {
    eq = !eq;
  }
  if (g_env.verbose) {
    node_info(n, "Evaluated to: %s", (eq ? "true" : "false"));
  }
  return eq;
}

static inline bool _if_node_is_else(struct node *n) {
  return n && n->value && strcmp(n->value, "else") == 0;
}

static void _if_pull_else(struct node *n) {
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = next;
  bool elz = false;

  if (_if_node_is_else(nn)) {
    elz = true;
    next = nn->next;
  }

  n->child = 0;

  if (elz && nn->child) {
    nn = nn->child;
    n->next = nn; // Keep upper iterations (if any) be consistent.
    n->child = 0;
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
    n->next = next;
    prev->next = next;
  } else {
    n->next = next;
    n->parent->child = next;
  }
}

static void _if_pull_if(struct node *n) {
  struct node *mn = n->child;
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = mn->next;

  if (_if_node_is_else(next)) {
    next = next->next; // Skip else block
  }

  n->child = 0;

  if (nn) {
    n->next = nn; // Keep upper iterations (if any) be consistent.
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
    n->next = next;
    prev->next = next;
  } else {
    n->next = next;
    n->parent->child = next;
  }
}

static void _if_init(struct node *n) {
  struct node *cn = n->child; // Conditional node
  if (!cn) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "'if {...}' must have a condition clause");
  }
  node_init(cn); // Explicitly init conditional node since in script init worflow 'if' initiated first.

  bool matched = _if_cond_eval(n, cn);
  if (matched) {
    _if_pull_if(n);
  } else {
    _if_pull_else(n);
  }
  n->init = 0; // Protect me from second call in any way
}

int node_if_setup(struct node *n) {
  n->init = _if_init;
  return 0;
}
