#include "script.h"
#include "log.h"

static void _include(struct node *n, struct node *cn) {
  const char *file = node_value(cn);
  if (!file || *file == '\0') {
    node_fatal(AK_ERROR_SCRIPT, n, "No Autark script specified");
  }

  struct node *nn = 0;
  struct node *pn = node_find_prev_sibling(n);

  int rc = script_include(n->parent, file, &nn);
  if (rc) {
    node_fatal(rc, n, "Failed to open Autark script: %s", file);
  }
  akassert(nn);

  if (pn) {
    pn->next = nn;
  } else if (n->parent) {
    n->parent->child = nn;
  }
  nn->next = n->next;
  n->next = nn; // Keep upper iterations (if any) be consistent.
  n->child = 0;
}

static void _init(struct node *n) {
  struct node *cn = n->child;
  if (!cn) {
    node_fatal(AK_ERROR_SCRIPT, n, "Missing path in 'include {...}' directive");
  }
  node_init(cn); // Explicitly init conditional node since in script init worflow 'include' initiated first.
  _include(n, cn);
  n->init = 0;
}

int node_include_setup(struct node *n) {
  n->init = _init;
  return 0;
}
