#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"
#include "utils.h"

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stderr, "%s: %s", u->rel_path, buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stdout, "%s: %s", u->rel_path, buf);
}

static void _setup2(struct node *n) {
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "products");
  if (nn) {
    for (nn = nn->child; nn; nn = nn->child) {
      if (nn->type == NODE_TYPE_VALUE) {
        if (g_env.verbose) {
          akinfo("%s: Product %s", n->name, nn->value);
        }
        node_product_add(n, nn->value);
      }
    }
  }
}

static void _on_depends_outdated(struct node_resolve *r, const struct deps *dep) {
}

static void _on_depends_resolve(struct node_resolve *r) {
}

static void _on_depends(struct node *n, struct node *nn) {
  struct node *na[] = { n, nn };
  struct node_resolve nr = {
    .path = n->vfile,
    .user_data = na,
    .on_outdated = _on_depends_outdated,
    .on_resolve = _on_depends_resolve,
  };
  node_resolve(&nr);
}

static void _build(struct node *n) {
  const char *run_cmd = n->value;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_VALUE) {
      run_cmd = nn->value;
      break;
    }
  }
  if (!run_cmd) {
    node_fatal(AK_ERROR_FAIL, n, "No run command specified");
  }
  if (!g_env.quiet) {
    akinfo("%s: %s", n->name, run_cmd);
  }
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "depends");
  for (nn = nn->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_VALUE) {
      _on_depends(n, nn);
    }
  }
}

int node_run_setup(struct node *n) {
  n->setup2 = _setup2;
  n->build = _build;
  return 0;
}
