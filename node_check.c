#include "script.h"
#include "env.h"
#include "log.h"
#include "pool.h"
#include "spawn.h"


#include <limits.h>
#include <unistd.h>
#include <stdbool.h>

static bool _check_is_outdated(struct node *n) {
  // TODO:
  return true;
}

static void _check_script_run(struct node *n) {
  int rc = 0;
  const char *script = n->value;
  if (!g_env.quiet) {
    akinfo("Checking %s", script);
  }
  struct pool *pool = pool_create_empty();
  const char *path = pool_printf(pool, ".autark/%s", script);

  struct unit *unit = unit_create(path, UNIT_FLG_POOL_OWNER, pool);
  unit->impl = n;
  unit_push(unit);

  const char *env_tmp = pool_printf(pool, "%s.env.tmp", script);
  unlink(env_tmp);

  //struct spawn *spawn = spawn_create(script, )




  unit_pop();

  /*
     int rc = 0, code;
     char unit_path[PATH_MAX], env_path[PATH_MAX], env_path_tmp[PATH_MAX];

     const char *unit = n->value;
     if (!g_env.quiet) {
     fprintf(stderr, "Checking %s\n", unit);
     }

     snprintf(unit_path, sizeof(unit_path), ".autark/%s", unit);
     snprintf(env_path_tmp, sizeof(env_path_tmp), "%s/.autark/%s.env.tmp", g_env.unit.cache_dir, unit);
     unlink(env_path_tmp);

     struct spawn *spawn = spawn_create(unit_path, n);
     spawn_env_set(spawn, AUTARK_UNIT, unit_path);
     RCC(rc, finish, spawn_do(spawn));

     code = spawn_exit_code(spawn);
     if (code != 0) {
     rc = akerror(AK_ERROR_FAIL, "Check program: %s exited with non-zero code: %d", code);
     goto finish;
     }

     snprintf(env_path, sizeof(env_path), "%s/.autark/%s.env", g_env.unit.cache_dir, unit);
     if (path_is_accesible_read(env_path_tmp)) {
     RCC(rc, finish, utils_rename_file(env_path_tmp, env_path));
     RCC(rc, finish, node_env_load(n, env_path));
     } else {
     unlink(env_path);
     }

     finish:
     spawn_destroy(spawn);
     return rc;
   */
}

static void _node_check_special(struct node *n) {
}

static void _node_check_script(struct node *n) {
  _check_script_run(n);
}

static void _node_check(struct node *n) {
  if (n->type == NODE_TYPE_VALUE) {
    _node_check_script(n);
  } else if (n->type == NODE_TYPE_BAG) {
    _node_check_special(n);
  } else {
    node_fatal(AK_ERROR_SCRIPT_ERROR, n, "Check is unsupported for script node");
  }
}

static void _resolve(struct node *n) {
  for (struct node *c = n->child; c; c = c->next) {
    _node_check(c);
  }
}

static void _dispose(struct node *n) {
}

int node_check_setup(struct node *n) {
  n->resolve = _resolve;
  n->dispose = _dispose;
  return 0;
}
