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
  fprintf(stdout, "%s", buf);
}

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

struct _on_resolve_ctx {
  struct node *n;
  struct node_resolve *r;
  struct ulist consumes; // sizeof(char*)
};

static void _on_resolve(struct node_resolve *r) {
  struct _on_resolve_ctx *ctx = r->user_data;
  struct node *n = ctx->n;
  struct node *ncmd = n->child;
  const char *cmd = node_value(ncmd);
  if (!cmd) {
    node_fatal(AK_ERROR_FAIL, n, "No run command specified");
  }
  node_info(n, "%s", cmd);

  struct unit *unit = unit_peek();
  struct spawn *s = spawn_create(cmd, &(struct _spawn_data) {
    .cmd = cmd,
    .n = n
  });
  spawn_env_path_prepend(s, unit->dir);
  spawn_env_path_prepend(s, unit->cache_dir);
  spawn_set_stdout_handler(s, _stdout_handler);
  spawn_set_stderr_handler(s, _stderr_handler);

  if (ncmd->child) {
    for (struct node *nn = ncmd->child; nn; nn = nn->next) {
      if (nn->type == NODE_TYPE_VALUE || nn->type == NODE_TYPE_SUBST) {
        spawn_arg_add(s, node_value(nn));
      }
    }
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

  struct deps deps;
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }

  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes, i);
    deps_add(&deps, DEPS_TYPE_FILE, path);
  }
  node_add_unit_deps(&deps);
  deps_close(&deps);
}

static void _setup(struct node *n) {
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "products");
  if (nn && nn->child) {
    for (nn = nn->child; nn; nn = nn->next) {
      if (nn->type == NODE_TYPE_VALUE || nn->type == NODE_TYPE_SUBST) {
        const char *value = node_value(nn);
        if (g_env.verbose) {
          node_info(n, "Product: %s", value);
        }
        node_product_add(n, value, 0);
      }
    }
  }
}

static void _on_consumed_resolved(const char *path_, void *d) {
  struct _on_resolve_ctx *ctx = d;
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes, &path);
}

static void _on_resolve_init(struct node_resolve *r) {
  struct _on_resolve_ctx *ctx = r->user_data;
  ctx->r = r;
  struct node *n = ctx->n;
  node_consumes_resolve(n, _on_consumed_resolved, ctx);
}

static void _build(struct node *n) {
  struct _on_resolve_ctx ctx = {
    .n = n,
    .consumes = { .usize = sizeof(char*) }
  };
  node_resolve(&(struct node_resolve) {
    .path = n->vfile,
    .user_data = &ctx,
    .on_init = _on_resolve_init,
    .on_resolve = _on_resolve,
  });
  ulist_destroy_keep(&ctx.consumes);
}

int node_run_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->setup = _setup;
  n->build = _build;
  return 0;
}
