#include "script.h"
#include "log.h"
#include "env.h"

#include <string.h>

static bool _defined_eval(struct node *mn) {
  struct node *n = mn->parent;
  const char *val = node_value(mn->child);
  if (!val) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Defined directive must have a body: if { defined { ^ } ... ");
  }
  if (node_env_get(n, val)) {
    return true;
  } else {
    return false;
  }
}

static bool _matched_eval(struct node *mn) {
  struct node *n = mn->parent;



  return false;
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

static void _init(struct node *n) {
  // Eliminate if block from the Tree according to conditions
  struct node *pn = n->parent;
  struct node *mn = n->child; // Match node
  bool matched = _cond_eval(n, mn);
  // TODO: Tree shaking accourding to match result


  n->init = 0; // Protect me from second call in any way
}

int node_if_setup(struct node *n) {
  n->init = _init;
  return 0;
}
