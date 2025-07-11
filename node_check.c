#ifndef _AMALGAMATE_
#include "script.h"
#include "env.h"
#include "log.h"
#include "pool.h"
#include "paths.h"
#include "spawn.h"
#include "deps.h"

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#endif

static void _check_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}

static void _check_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

static void _check_on_env_value(struct node_resolve *nr, const char *key, const char *val) {
  struct unit *unit = nr->user_data;
  akassert(unit->n);
  if (g_env.verbose) {
    akinfo("%s %s=%s", unit->rel_path, key, val);
  }
  node_env_set(unit->n, key, val);
}

static void _check_on_resolve(struct node_resolve *r) {
  struct unit *unit = r->user_data;
  struct node *n = unit->n;
  const char *path = unit->impl;

  struct spawn *s = spawn_create(path, unit);
  spawn_set_stdout_handler(s, _check_stdout_handler);
  spawn_set_stderr_handler(s, _check_stderr_handler);
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      spawn_arg_add(s, node_value(nn));
    }
  }

  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", unit->source_path);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", unit->source_path, code);
    }
  }
  spawn_destroy(s);

  // Good, now add dependency on itself
  struct deps deps;
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, unit->n, "Failed to open depencency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(&deps);
  deps_close(&deps);
}

static char* _resolve_check_path(struct pool *pool, struct node *n, const char *script, struct unit **out_u) {
  *out_u = 0;
  char buf[PATH_MAX];
  for (int i = (int) g_env.stack_units.num - 1; i >= 0; --i) {
    struct unit_ctx *c = (struct unit_ctx*) ulist_get(&g_env.stack_units, i);
    struct unit *u = c->unit;
    snprintf(buf, sizeof(buf), "%s/.autark/%s", u->dir, script);
    if (path_is_exist(buf)) {
      *out_u = u;
      return pool_strdup(pool, buf);
    }
  }
  node_fatal(AK_ERROR_FAIL, n, "Check script is not found: %s", script);
  return 0;
}

static void _check_script(struct node *n) {
  const char *script = node_value(n);
  if (g_env.verbose) {
    node_info(n->parent, "%s", script);
  }
  struct unit *parent = 0;
  struct pool *pool = pool_create(on_unit_pool_destroy);

  char *path = _resolve_check_path(pool, n, script, &parent);
  const char *vpath = pool_printf(pool, "%s/.autark/%s", parent->dir, n->vfile);

  struct unit *unit = unit_create(vpath, 0, pool);
  unit->n = n;
  unit->impl = path;
  unit_push(unit, n);

  struct node_resolve nr = {
    .n = n,
    .mode = NODE_RESOLVE_ENV_ALWAYS,
    .path = n->vfile,
    .user_data = unit,
    .on_env_value = _check_on_env_value,
    .on_resolve = _check_on_resolve,
  };

  node_resolve(&nr);

  unit_pop();
  pool_destroy(pool);
}

static void _check_init(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    _check_script(nn);
  }
}

int node_check_setup(struct node *n) {
  n->init = _check_init;
  return 0;
}
