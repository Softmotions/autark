#ifndef _AMALGAMATE_
#include "script.h"
#include "ulist.h"
#include "env.h"
#include "log.h"
#include "paths.h"
#include "pool.h"
#include "utils.h"

#include "config.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#endif

struct _install_on_resolve_ctx {
  struct node_resolve *r;
  struct node *n;
  struct node *n_target;
  struct ulist consumes;  // sizeof(char*)
};

static void _install_symlink(struct _install_on_resolve_ctx *ctx, const char *src, const char *dst, struct stat *st) {
  char buf[PATH_MAX];
  struct node *n = ctx->n;
  node_info(n, "Symlink %s => %s", src, dst);

  ssize_t len = readlink(src, buf, sizeof(buf) - 1);
  if (len == -1) {
    node_fatal(errno, n, "Error reading symlink: %s", src);
  }
  buf[len] = '\0';

  if (symlink(buf, dst) == -1) {
    node_fatal(errno, n, "Error creating symlink: %s", dst);
  }

  struct timespec times[2] = { st->st_atim, st->st_mtim };
  utimensat(AT_FDCWD, dst, times, AT_SYMLINK_NOFOLLOW);
}

static void _install_file(struct _install_on_resolve_ctx *ctx, const char *src, const char *dst, struct stat *st) {
  struct node *n = ctx->n;
  node_info(n, "File %s => %s", src, dst);

  int in_fd = open(src, O_RDONLY);
  if (in_fd == -1) {
    node_fatal(errno, n, "Error opening file: %s", src);
  }
  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st->st_mode);
  if (out_fd == -1) {
    node_fatal(errno, n, "Error opening file: %s", dst);
  }

  char buf[8192];
  ssize_t r;
  while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
    if (write(out_fd, buf, r) != r) {
      node_fatal(errno, n, "Error writing file: %s", dst);
    }
  }
  if (r == -1) {
    node_fatal(errno, n, "Error reading file: %s", src);
  }

  fchmod(out_fd, st->st_mode);
  struct timespec times[2] = { st->st_atim, st->st_mtim };
  futimens(out_fd, times);

  close(in_fd);
  close(out_fd);
}

static void _install_do(struct node_resolve *r, const char *src, const char *target) {
  char src_buf[PATH_MAX];
  char dst_buf[PATH_MAX];

  struct stat st;
  struct _install_on_resolve_ctx *ctx = r->user_data;

  akcheck(lstat(src, &st));
  if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) {
    node_fatal(AK_ERROR_FAIL, ctx->n, "Cannot install unsupported file type. File: %s", src);
  }

  utils_strncpy(src_buf, src, sizeof(src_buf));
  snprintf(dst_buf, sizeof(dst_buf), "%s/%s", target, path_basename(src_buf));

  if (S_ISREG(st.st_mode)) {
    _install_file(ctx, src, dst_buf, &st);
  } else if (S_ISLNK(st.st_mode)) {
    _install_symlink(ctx, src, dst_buf, &st);
  }
}

static void _install_dep_add(struct deps *deps, const char *src, const char *target) {
  char src_buf[PATH_MAX];
  char dst_buf[PATH_MAX];

  deps_add(deps, DEPS_TYPE_FILE, 's', src, 0);
  utils_strncpy(src_buf, src, sizeof(src_buf));
  snprintf(dst_buf, sizeof(dst_buf), "%s/%s", target, path_basename(src_buf));
  deps_add_alias(deps, 's', src, dst_buf);
}

static void _install_on_resolve(struct node_resolve *r) {
  struct deps deps;
  struct node *n = r->n;
  struct _install_on_resolve_ctx *ctx = r->user_data;
  const char *target = node_value(ctx->n_target);

  if (!path_is_absolute(target)) {
    target = pool_printf(r->pool, "%s/%s", g_env.install.prefix_dir, target);
    target = path_normalize_pool(target, g_env.pool);
  }

  if (path_is_exist(target) && !path_is_dir(target)) {
    node_fatal(AK_ERROR_FAIL, n, "Target: %s is not a directory", target);
  }

  int rc = path_mkdirs(target);
  if (rc) {
    node_fatal(rc, n, "Failed to create target install directory: %s", target);
  }

  struct ulist rlist = { .usize = sizeof(char*) };
  struct ulist *slist = &ctx->consumes;

  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') { // Rebuild all on any outdated non source dependency
        slist = &ctx->consumes;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }

  for (int i = 0; i < slist->num; ++i) {
    const char *src = *(const char**) ulist_get(slist, i);
    _install_do(r, src, target);
  }

  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(&deps);

  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }

  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *src = *(const char**) ulist_get(&ctx->consumes, i);
    _install_dep_add(&deps, src, target);
  }

  deps_close(&deps);
  ulist_destroy_keep(&rlist);
}

static void _install_on_consumed_resolved(const char *path_, void *d) {
  struct _install_on_resolve_ctx *ctx = d;
  if (path_is_dir(path_)) {
    node_fatal(AK_ERROR_FAIL, ctx->n, "Installing of directories is not supported. Path: %s", path_);
  }
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes, &path);
}

static void _install_on_resolve_init(struct node_resolve *r) {
  struct _install_on_resolve_ctx *ctx = r->user_data;
  ctx->r = r;
  struct node *nn = ctx->n_target->next;
  if (nn) {
    node_consumes_resolve(r->n, nn, 0, _install_on_consumed_resolved, ctx);
  }
}

static void _install_build(struct node *n) {
  struct _install_on_resolve_ctx ctx = {
    .n = n,
    .n_target = n->child,
    .consumes = { .usize = sizeof(char*) },
  };
  if (!ctx.n_target || !node_is_can_be_value(ctx.n_target)) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "No target dir specified");
  }
  if (!ctx.n_target->next || !node_is_can_be_value(ctx.n_target->next)) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "At least one source file/dir should be specified");
  }
  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = &ctx,
    .on_init = _install_on_resolve_init,
    .on_resolve = _install_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };

  for (struct node *nn = ctx.n_target; nn; nn = nn->next) {
    if (node_is_value_may_be_dep_saved(nn)) {
      ulist_push(&r.node_val_deps, &nn);
    }
  }

  node_resolve(&r);
  ulist_destroy_keep(&ctx.consumes);
}

int node_install_setup(struct node *n) {
  if (!g_env.install.enabled || !g_env.install.prefix_dir) {
    return 0;
  }
  n->build = _install_build;
  return 0;
}
