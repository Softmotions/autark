#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stderr, "%s: %s", u->rel_path, buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stdout, "%s: %s", u->rel_path, buf);
}

static void _setup(struct node *n) {
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
    return;
  }
  if (!g_env.verbose) {
    akinfo("Run: %s", run_cmd);
  }
}

int node_run_setup(struct node *n) {
  n->setup = _setup;
  n->build = _build;
  return 0;
}
