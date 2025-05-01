#include "script.h"
#include "env.h"
#include "log.h"
#include "pool.h"
#include "spawn.h"


#include <limits.h>
#include <unistd.h>
#include <stdio.h>

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stderr, "%s: %s", u->rel_path, buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct unit *u = spawn_user_data(s);
  fprintf(stdout, "%s: %s", u->rel_path, buf);
}

static void _check_on_env_value(struct node_resolve *nr, const char *key, const char *val) {
  struct unit *unit = nr->user_data;
  akassert(unit->n);
  if (!g_env.quiet) {
    akinfo("%s %s=%s", unit->rel_path, key, val);
  }
  node_env_set(unit->n, key, val);
}

static void _check_on_resolve(struct node_resolve *r) {
  struct unit *unit = r->user_data;
  struct spawn *spawn = spawn_create(unit->source_path, unit);
  spawn_set_stdout_handler(spawn, _stdout_handler);
  spawn_set_stderr_handler(spawn, _stderr_handler);
  int rc = spawn_do(spawn);
  if (rc) {
    node_fatal(rc, unit->n, "%s", unit->source_path);
  } else {
    int code = spawn_exit_code(spawn);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, unit->n, "%s: %d", unit->source_path, code);
    }
  }
  spawn_destroy(spawn);
}

static void _node_check_script(struct node *n) {
  const char *script = n->value;
  if (!g_env.quiet) {
    akinfo("Check: %s", script);
  }
  struct pool *pool = pool_create(on_unit_pool_destroy);
  const char *path = pool_printf(pool, ".autark/%s", script);
  struct unit *unit = unit_create(path, 0, pool);
  unit->n = n;
  unit_push(unit);

  struct node_resolve nr = {
    .mode = NODE_RESOLVE_ENV_ALWAYS,
    .path = unit->cache_path,
    .user_data = unit,
    .on_env_value = _check_on_env_value,
    .on_resolve = _check_on_resolve,
  };
  node_resolve(&nr);

  unit_pop();
  pool_destroy(pool);
}

static void _node_check_special(struct node *n) {
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

static void _setup(struct node *n) {
  for (struct node *c = n->child; c; c = c->next) {
    _node_check(c);
  }
}

static void _dispose(struct node *n) {
}

int node_check_setup(struct node *n) {
  n->setup = _setup;
  n->dispose = _dispose;
  return 0;
}
