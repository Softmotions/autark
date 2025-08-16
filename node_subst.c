#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "spawn.h"
#include "xstr.h"
#include "utils.h"

#include <stdlib.h>
#endif

static const char* _subst_setval(struct node *n, const char *vv, const char *dv) {
  if (vv) {
    if (n->impl) {
      if (strcmp(n->impl, vv) == 0) {
        return n->impl;
      }
      free(n->impl);
    }
    n->impl = xstrdup(vv);
    return n->impl;
  } else {
    return dv;
  }
}

static const char* _subst_value(struct node *n) {
  if (n->child) {
    const char *dv_ = node_value(n->child->next);
    const char *dv = dv_;
    if (!dv) {
      dv = "";
    }
    const char *key = node_value(n->child);
    if (!key) {
      node_warn(n, "No key specified");
      return _subst_setval(n, 0, dv);
    }

    struct node_foreach *fe = node_find_parent_foreach(n);
    if (fe && strcmp(fe->name, key) == 0) {
      ++fe->access_cnt;
      return _subst_setval(n, fe->value, dv);
    }

    const char *vv = node_env_get(n, key);
    return _subst_setval(n, vv, dv);
  }
  return "";
}

static void _subst_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

struct _subst_ctx {
  struct node *n;
  struct xstr *xstr;
  const char  *cmd;
};

static void _subst_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}

static void _subst_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _subst_ctx *ctx = spawn_user_data(s);
  xstr_cat2(ctx->xstr, buf, buflen);
  fprintf(stdout, "%s", buf);
}

static const char* _subst_value_proc(struct node *n) {
  if (n->impl) {
    return n->impl;
  }
  const char *cmd = node_value(n->child);
  if (cmd == 0 || *cmd == '\0') {
    return 0;
  }
  struct xstr *xstr = xstr_create_empty();
  struct _subst_ctx ctx = {
    .n = n,
    .cmd = cmd,
    .xstr = xstr
  };
  struct spawn *s = spawn_create(cmd, &ctx);
  spawn_set_stdout_handler(s, _subst_stdout_handler);
  spawn_set_stderr_handler(s, _subst_stderr_handler);
  for (struct node *nn = n->child->next; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      spawn_arg_add(s, node_value(nn));
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
  char *val = xstr_destroy_keep_ptr(xstr);
  for (int i = strlen(val) - 1; i >= 0 && utils_char_is_space(val[i]); --i) {
    val[i] = '\0';
  }
  n->impl = val;
  spawn_destroy(s);
  return n->impl;
}

static void _subst_value_proc_cache_on_resolve(struct node_resolve *r) {
  struct node *n = r->n;
  _subst_value_proc(n);

  if (!n->impl) {
    n->impl = xstrdup("");
  }

  struct unit *unit = unit_peek();
  const char *path = pool_printf(r->pool, "%s/%s.cache", unit->cache_dir, r->path);
  int rc = utils_file_write_buf(path, (char*) n->impl, strlen(n->impl), false);
  if (rc) {
    node_fatal(rc, n, "Failed to write file: %s", path);
  }

  struct deps deps;
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);
  deps_add(&deps, DEPS_TYPE_FILE, 0, path, 0);
  deps_close(&deps);
}

static const char* _subst_value_proc_cache(struct node *n) {
  if (n->impl) {
    return n->impl;
  }

  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .on_resolve = _subst_value_proc_cache_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) },
  };

  for (struct node *cn = n->child; cn; cn = cn->next) {
    if (node_is_value_may_be_dep_saved(cn)) {
      ulist_push(&r.node_val_deps, &cn);
    }
  }

  node_resolve(&r);

  if (!n->impl) {
    struct unit *unit = unit_peek();
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.cache", unit->cache_dir, r.path);
    struct value val = utils_file_as_buf(path, 1024 * 1024); // 1Mb max
    if (val.error) {
      node_fatal(val.error, n, "Failed to read cache file: %s", path);
    }
    n->impl = val.buf;
  }

  return n->impl;
}

int node_subst_setup(struct node *n) {
  if (strstr(n->value, "@@")) {
    n->value_get = _subst_value_proc_cache;
  } else if (strchr(n->value, '@')) {
    n->value_get = _subst_value_proc;
  } else {
    n->flags |= NODE_FLG_NO_CWD;
    n->value_get = _subst_value;
  }
  n->dispose = _subst_dispose;
  return 0;
}
