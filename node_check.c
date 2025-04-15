#include "script.h"
#include "basedefs.h"
#include "env.h"
#include "log.h"
#include "spawn.h"
#include "paths.h"
#include "utils.h"
#include "env.h"

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>


static bool _check_is_outdated(struct node *n) {
  // TODO:
  return true;
}

static int _check_script_run(struct node *n) {
  int rc = 0, code;
  char env_path[PATH_MAX], env_path_tmp[PATH_MAX];

  const char *unit = n->value;
  if (!g_env.quiet) {
    fprintf(stderr, "Checking %s\n", unit);
  }

  snprintf(env_path_tmp, sizeof(env_path_tmp), ".autark/%s.env.tmp", unit);
  unlink(env_path_tmp);

  struct spawn *spawn = spawn_create(unit, n);
  spawn_env_set(spawn, AUTARK_UNIT, unit);
  RCC(rc, finish, spawn_do(spawn));

  code = spawn_exit_code(spawn);
  if (code != 0) {
    rc = akerror(AK_ERROR_FAIL, "Check program exited with non-zero code: %d", code);
    goto finish;
  }

  snprintf(env_path, sizeof(env_path), ".autark/%s.env", unit);
  if (path_is_accesible_read(env_path_tmp)) {
    RCC(rc, finish, utils_rename_file(env_path_tmp, env_path));
    RCC(rc, finish, node_env_load(n, env_path));
  } else {
    unlink(env_path);
  }

finish:
  spawn_destroy(spawn);
  return rc;
}

static int _node_check_special(struct node *n) {
  int rc = 0;
  // TODO:
  return AK_ERROR_NOT_IMPLEMENTED;
}

static int _node_check_script(struct node *n) {
  return _check_script_run(n);
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
