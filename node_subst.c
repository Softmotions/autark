#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "spawn.h"
#include "xstr.h"
#include "utils.h"
#include "env.h"

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
      if (g_env.check.log) {
        xstr_printf(g_env.check.log, "%s: subst set from foreach: %s=%s\n", n->name, n->name, n->value);
      }
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
  if (cmd == 0) {
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
  for (int i = strlen(val) - 1; i >= 0 && utils_char_is_space(val[i]); --i) {
    val[i] = '\0';
  }
  n->impl = val;
  spawn_destroy(s);
  return n->impl;
}

int node_subst_setup(struct node *n) {
  if (strchr(n->value, '@')) {
    n->value_get = _subst_value_proc;
  } else {
    n->value_get = _subst_value;
  }
  n->dispose = _subst_dispose;
  return 0;
}
