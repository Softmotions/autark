#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"
#include "paths.h"

#include <unistd.h>

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stderr, "%s: %s", u->rel_path, buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stdout, "%s: %s", u->rel_path, buf);
}

static void _on_resolve(struct node_resolve *r) {
  struct node *n = r->user_data;
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
}

static void _setup2(struct node *n) {
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "products");
  if (nn) {
    for (nn = nn->child; nn; nn = nn->child) {
      if (nn->type == NODE_TYPE_VALUE) {
        if (g_env.verbose) {
          akinfo("%s: Product %s", n->name, nn->value);
        }
        node_product_add(n, nn->value, 0);
      }
    }
  }
}

static void _on_outdated(struct node_resolve *r, const struct deps *dep) {
  struct node *n = r->user_data;
  if (!path_is_exist(dep->resource)) {
    node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "Dependency: %s", dep->resource);
  }
}

static void _build(struct node *n) {
  node_consumes_resolve(n);

  node_resolve(&(struct node_resolve) {
    .path = n->vfile,
    .user_data = n,
    .on_outdated = _on_outdated,
    .on_resolve = _on_resolve,
  });
}

int node_run_setup(struct node *n) {
  n->setup2 = _setup2;
  n->build = _build;
  return 0;
}
