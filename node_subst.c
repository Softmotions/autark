#include "script.h"
#include "alloc.h"
#include "spawn.h"
#include "xstr.h"
#include "utils.h"

#include <stdlib.h>

static const char* _setval(struct node *n, const char *vv, const char *dv) {
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

static const char* _value(struct node *n) {
  if (n->child) {
    const char *dv_ = node_value(n->child->next);
    const char *dv = dv_;
    if (!dv) {
      dv = "";
    }
    const char *key = node_value(n->child);
    if (!key) {
      node_warn(n, "No key specified");
      return _setval(n, 0, dv);
    }
    const char *vv = node_env_get(n, key);
    if (vv) {
      return _setval(n, vv, 0);
    }
    const char *ev = getenv(key);
    if (!ev && !dv_) {
      node_warn(n, "${%s} env variable not found", key);
    }
    return _setval(n, ev, dv);
  }
  return "";
}

static void _dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

struct _sctx {
  struct node *n;
  struct xstr *xstr;
  const char *cmd;
};

static void _stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _sctx *ctx = spawn_user_data(s);
  fprintf(stdout, "%s: %s", ctx->cmd, buf);
}

static void _stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _sctx *ctx = spawn_user_data(s);
  xstr_cat2(ctx->xstr, buf, buflen);
  fprintf(stdout, "%s: %s", ctx->cmd, buf);
}

static const char* _value_proc(struct node *n) {
  if (n->impl) {
    return n->impl;
  }
  const char *cmd = node_value(n->child);
  if (cmd == 0) {
    return 0;
  }

  struct xstr *xstr = xstr_create_empty();
  struct _sctx ctx = {
    .n = n,
    .cmd = cmd,
    .xstr = xstr
  };
  struct spawn *s = spawn_create(cmd, &ctx);
  spawn_set_stdout_handler(s, _stdout_handler);
  spawn_set_stderr_handler(s, _stderr_handler);
  for (struct node *nn = n->child->next; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_VALUE) {
      spawn_arg_add(s, nn->value);
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
  for (int i = strlen(val) - 1; i >= 0; --i) {
    if (utils_char_is_space(val[i])) {
      val[i] = '\0';
    }
  }
  n->impl = val;
  spawn_destroy(s);
  return n->impl;
}

int node_subst_setup(struct node *n) {
  if (strchr(n->value, '@')) {
    n->value_get = _value_proc;
  } else {
    n->value_get = _value;
  }
  n->dispose = _dispose;
  return 0;
}
