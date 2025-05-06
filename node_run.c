#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"
#include "spawn.h"

#include <unistd.h>

struct _spawn_data {
  struct node *n;
  const char  *cmd;
};

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _spawn_data *d = spawn_user_data(s);
  fprintf(stderr, "%s: %s", d->cmd, buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _spawn_data *d = spawn_user_data(s);
  fprintf(stdout, "%s: %s", d->cmd, buf);
}

static void _on_resolve(struct node_resolve *r) {
  struct node *n = r->user_data;
  const char *cmd = n->child ? n->child->value : 0;
  if (!cmd) {
    node_fatal(AK_ERROR_FAIL, n, "No run command specified");
  }
  if (!g_env.quiet) {
    akinfo("%s: %s", n->name, cmd);
  }

  struct spawn *spawn = spawn_create(cmd, &(struct _spawn_data) {
    .cmd = cmd,
    .n = n
  });
  spawn_set_stdout_handler(spawn, _stdout_handler);
  spawn_set_stderr_handler(spawn, _stderr_handler);
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_VALUE) {
      spawn_arg_add(spawn, nn->value);
    }
  }

  int rc = spawn_do(spawn);
  if (rc) {
    node_fatal(rc, n, "%s", cmd);
  } else {
    int code = spawn_exit_code(spawn);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", cmd, code);
    }
  }
  spawn_destroy(spawn);

  struct deps deps;
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(&deps);
  deps_close(&deps);
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

static void _build(struct node *n) {
  node_consumes_resolve(n);
  node_resolve(&(struct node_resolve) {
    .path = n->vfile,
    .user_data = n,
    .on_resolve = _on_resolve,
  });
}

int node_run_setup(struct node *n) {
  n->setup2 = _setup2;
  n->build = _build;
  return 0;
}
