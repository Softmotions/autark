#include "script.h"
#include "pool.h"
#include "log.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>

struct _script {
  const char *dir; //< Script directory
};

static inline struct _script* _script(struct node *n) {
  akassert(n->type == NODE_TYPE_SCRIPT);
  return n->impl;
}

static int _resolve(struct node *n) {
  return 0;
}

static int _build(struct node *n) {
  return 0;
}

static void _dispose(struct node *n) {
}

int node_script_setup(struct node *n) {
  n->resolve = _resolve;
  n->build = _build;
  n->dispose = _dispose;

  struct _script *s = pool_calloc(n->ctx->pool, sizeof(*s));
  n->impl = s;

  char cwd[PATH_MAX], rpath[PATH_MAX];
  const char *value = n->value;
  if (!value || strcmp(value, "<script>") == 0) {
    value = getcwd(cwd, sizeof(cwd));
    if (!value) {
      return errno;
    }
  }
  if (!realpath(value, rpath)) {
    return errno;
  }
  s->dir = pool_strdup(n->ctx->pool, dirname(rpath));
  return 0;
}

const char* node_script_dir(struct node *n) {
  struct _script *s = _script(n);
  return s->dir;
}
