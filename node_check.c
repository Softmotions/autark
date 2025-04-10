#include "script.h"
#include "basedefs.h"
#include "env.h"
#include "log.h"

#include <stdio.h>

static int _node_check_script(struct node *n) {
  int rc = 0;
  if (!g_env.quiet) {
    fprintf(stderr, "Checking %s\n", n->value);
  }

  return rc;
}

static int _node_check_special(struct node *n) {
  int rc = 0;
  // TODO:
  return AK_ERROR_NOT_IMPLEMENTED;
}

static int _node_check(struct node *n) {
  if (n->type == NODE_TYPE_VALUE) {
    return _node_check_script(n);
  } else if (n->type == NODE_TYPE_BAG) {
    return _node_check_special(n);
  } else {
    node_fatal(AK_ERROR_SCRIPT_ERROR, n, "Check is unsupported for script node");
  }
  return 0;
}

static int _resolve(struct node *n) {
  int rc = 0;
  for (struct node *c = n->child; c; c = c->next) {
    RCC(rc, finish, _node_check(c));
  }
finish:
  return rc;
}

static void _dispose(struct node *n) {
}

int node_check_setup(struct node *n) {
  n->resolve = _resolve;
  n->dispose = _dispose;
  return 0;
}
