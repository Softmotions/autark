#ifndef _AMALGAMATE_
#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"
#include "spawn.h"
#include "utils.h"
#include "log.h"
#include "paths.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#endif

struct _run_spawn_data {
  struct node *n;
  const char  *cmd;
};

static void _run_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}

static void _run_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

struct _run_on_resolve_ctx {
  struct node_resolve *r;
  struct node_foreach *fe;
  struct ulist consumes;         // sizeof(char*)
  struct ulist consumes_foreach; // sizeof(char*)
};

static void _run_on_resolve_shell(struct node_resolve *r, struct node *nn_) {
  struct node *n = r->n;
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = nn_; nn; nn = nn->next) {
    if (nn != nn_) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (xstr_size(xstr)) {
          xstr_cat2(xstr, " ", 1);
        }
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, v);
    }
  }

  puts(xstr_ptr(xstr));
  int rc = system(xstr_ptr(xstr));
  if (rc == -1) {
    node_fatal(errno, n, "Failed to execute shell command: %s", xstr_ptr(xstr));
  } else if (rc) {
    node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", xstr_ptr(xstr), rc);
  }
  xstr_destroy(xstr);
}

static void _run_on_resolve_exec(struct node_resolve *r, struct node *ncmd) {
  struct node *n = r->n;
  const char *cmd = node_value(ncmd);
  if (!cmd) {
    node_fatal(AK_ERROR_FAIL, n, "No run command specified");
  }

  struct unit *unit = unit_peek();
  struct spawn *s = spawn_create(cmd, &(struct _run_spawn_data) {
    .cmd = cmd,
    .n = n
  });
  spawn_env_path_prepend(s, unit->dir);
  spawn_env_path_prepend(s, unit->cache_dir);
  spawn_set_stdout_handler(s, _run_stdout_handler);
  spawn_set_stderr_handler(s, _run_stderr_handler);

  for (struct node *nn = ncmd->next; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_VALUE || nn->type == NODE_TYPE_SUBST) {
      spawn_arg_add(s, node_value(nn));
    }
  }

  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: %s\n", n->name, cmd);
  }

  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", cmd);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", cmd, code);
    }
  }
  spawn_destroy(s);
}

static void _run_on_resolve_do(struct node_resolve *r, struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "exec") == 0) {
      _run_on_resolve_exec(r, nn->child);
    } else if (strcmp(nn->value, "shell") == 0) {
      _run_on_resolve_shell(r, nn->child);
    }
  }
}

static void _run_on_resolve(struct node_resolve *r) {
  struct _run_on_resolve_ctx *ctx = r->user_data;
  struct ulist _flist = { .usize = sizeof(char*) };
  struct ulist *flist = &ctx->consumes_foreach;
  struct node *n = r->n;

  if (ctx->fe) {
    if (r->resolve_outdated.num) {
      for (int i = 0; i < r->resolve_outdated.num; ++i) {
        struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
        if (u->flags != 'f') {
          flist = &ctx->consumes_foreach;
          break;
        }
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&_flist, &path);
        flist = &_flist;
      }
    }
    struct unit *unit = unit_peek();
    for (int i = 0; i < flist->num; ++i) {
      char *p = *(char**) ulist_get(flist, i);
      p = path_relativize_cwd(unit->cache_dir, p, unit->cache_dir);
      ctx->fe->value = p;
      _run_on_resolve_do(r, n);
      ctx->fe->value = 0;
      free(p);
    }
  } else {
    _run_on_resolve_do(r, n);
  }

  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }

  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }

  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes, i);
    deps_add(&deps, DEPS_TYPE_FILE, 0, path, 0);
  }

  for (int i = 0; i < ctx->consumes_foreach.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes_foreach, i);
    deps_add(&deps, DEPS_TYPE_FILE, 'f', path, 0);
  }

  node_add_unit_deps(&deps);
  deps_close(&deps);
  ulist_destroy_keep(&_flist);
}

static bool _run_setup_foreach(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (!fe) {
    return false;
  }
  struct node *pn = node_find_direct_child(n, NODE_TYPE_BAG, "produces");
  if (pn && pn->child) {
    struct vlist_iter iter;
    vlist_iter_init(fe->items, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[iter.len + 1];
      memcpy(buf, iter.item, iter.len);
      buf[iter.len] = '\0';
      fe->value = buf;
      for (struct node *nn = pn->child; nn; nn = nn->next) {
        if (nn->type == NODE_TYPE_VALUE || nn->type == NODE_TYPE_SUBST || nn->type == NODE_TYPE_SET) {
          const char *value = node_value(nn);
          if (g_env.verbose) {
            node_info(n, "Product: %s", value);
          }
          node_product_add(n, value, 0);
        }
      }
    }
  }
  return true;
}

static void _run_setup(struct node *n) {
  if (_run_setup_foreach(n)) {
    return;
  }
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "produces");
  if (nn && nn->child) {
    for (nn = nn->child; nn; nn = nn->next) {
      if (nn->type == NODE_TYPE_VALUE || nn->type == NODE_TYPE_SUBST || nn->type == NODE_TYPE_SET) {
        const char *value = node_value(nn);
        if (g_env.verbose) {
          node_info(n, "Product: %s", value);
        }
        node_product_add(n, value, 0);
      }
    }
  }
}

static void _run_on_consumed_resolved(const char *path_, void *d) {
  struct _run_on_resolve_ctx *ctx = d;
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes, &path);
}

static void _run_on_consumed_resolved_foreach(const char *path_, void *d) {
  struct _run_on_resolve_ctx *ctx = d;
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes_foreach, &path);
}

static void _run_on_resolve_init(struct node_resolve *r) {
  struct _run_on_resolve_ctx *ctx = r->user_data;
  ctx->r = r;
  struct node *nn = node_find_direct_child(r->n, NODE_TYPE_BAG, "consumes");
  if (nn && nn->child) {
    node_consumes_resolve(r->n, nn->child, 0, _run_on_consumed_resolved, ctx);
  }
  struct node_foreach *fe = node_find_parent_foreach(r->n);
  if (fe) {
    struct vlist_iter iter;
    struct ulist paths = { .usize = sizeof(char*) };
    vlist_iter_init(fe->items, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[iter.len + 1];
      memcpy(buf, iter.item, iter.len);
      char *p = pool_strndup(r->pool, buf, iter.len);
      ulist_push(&paths, &p);
    }
    node_consumes_resolve(r->n, 0, &paths, _run_on_consumed_resolved_foreach, ctx);
    ulist_destroy_keep(&paths);
  }
}

static void _run_build(struct node *n) {
  struct _run_on_resolve_ctx ctx = {
    .consumes = { .usize = sizeof(char*) },
    .consumes_foreach = { .usize = sizeof(char*) }
  };

  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = &ctx,
    .on_init = _run_on_resolve_init,
    .on_resolve = _run_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };

  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "exec") == 0 || strcmp(nn->value, "shell") == 0) {
      for (struct node *cn = nn->child; cn; cn = cn->next) {
        if (node_is_value_may_be_dep_saved(cn)) {
          ulist_push(&r.node_val_deps, &cn);
        }
      }
    }
  }

  node_resolve(&r);

  ulist_destroy_keep(&ctx.consumes);
  ulist_destroy_keep(&ctx.consumes_foreach);
}

int node_run_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->setup = _run_setup;
  n->build = _run_build;
  return 0;
}
