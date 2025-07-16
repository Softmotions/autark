#ifndef _AMALGAMATE_
#include "script.h"
#include "env.h"
#include "pool.h"
#include "paths.h"
#include "utils.h"
#include "xstr.h"
#include "ulist.h"
#include "alloc.h"
#include "utils.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#endif

struct _configure_ctx {
  struct pool *pool;
  struct node *n;
  struct ulist sources;  // char*
};

static char* _line_replace_def(struct node *n, struct deps *deps, char *line) {
  char key[4096];
  const char *ep = line + AK_LLEN("//autarkdef ");
  while (utils_char_is_space(*ep)) ++ep;

  const char *sp = ep;
  while (*ep != '\0' && !utils_char_is_space(*ep)) ++ep;

  const char *np = ep;
  while (*np != '\0' && utils_char_is_space(*np)) ++np;

  if (*np == '\"') {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s \"%s\"\n", key, val);
      return xstr_destroy_keep_ptr(xstr);
    }
  } else if (*np >= '0' && *np <= '9') {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s %s", key, np);
      return xstr_destroy_keep_ptr(xstr);
    } else {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s 0\n", key);
      return xstr_destroy_keep_ptr(xstr);
    }
  } else {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s\n", key);
      return xstr_destroy_keep_ptr(xstr);
    }
  }

  return line;
}

static char* _line_replace_subs(struct node *n, struct deps *deps, char *line) {
  char *s = line;
  char *sp = strchr(line, '@');
  if (!sp) {
    return line;
  }
  struct xstr *xstr = xstr_create_empty();
  struct xstr *kstr = xstr_create_empty();

  do {
    xstr_cat2(xstr, s, sp - s);
    xstr_clear(kstr);
    char *p = ++sp;
    sp = 0;

    while (*p) {
      if (*p == '@') {
        const char *key = xstr_ptr(kstr);
        const char *val = node_env_get(n, key);

        deps_add_env(deps, 0, key, val ? val : "");

        if (val && *val != '\0') {
          if (is_vlist(val)) {
            ++val;
            size_t len = strlen(val);
            char buf[len + 1];
            memcpy(buf, val, len + 1);
            utils_chars_replace(buf, '\1', ' ');
            xstr_cat(xstr, buf);
          } else {
            xstr_cat(xstr, val);
          }
        }
        s = ++p;
        sp = strchr(s, '@');
        xstr_clear(kstr);
        break;
      } else {
        xstr_cat2(kstr, p++, 1);
      }
    }
  } while (sp);

  if (xstr_size(kstr)) {
    xstr_cat2(xstr, "@", 1);
    xstr_cat(xstr, xstr_ptr(kstr));
  } else {
    xstr_cat(xstr, s);
  }
  xstr_destroy(kstr);

  return xstr_destroy_keep_ptr(xstr);
}

static void _process_file(struct node *n, const char *src, const char *tgt, struct deps *deps) {
  struct xstr *xstr = 0;
  if (strcmp(src, tgt) == 0) {
    xstr = xstr_create_empty();
    xstr_printf(xstr, "%s.tmp", src);
    tgt = xstr_ptr(xstr);
  }

  path_mkdirs_for(tgt);

  FILE *f = fopen(src, "r");
  if (!f) {
    node_fatal(errno, n, "Failed to open file: %s", src);
  }

  FILE *t = fopen(tgt, "w");
  if (!t) {
    node_fatal(errno, n, "Failed to open file for writing: %s", tgt);
  }

  char *line;
  char buf[16384];

  while ((line = fgets(buf, sizeof(buf), f))) {
    char *rl;
    if (utils_startswith(line, "//autarkdef ")) {
      rl = _line_replace_def(n, deps, line);
    } else {
      rl = _line_replace_subs(n, deps, line);
    }
    if (rl && fputs(rl, t) == EOF) {
      node_fatal(AK_ERROR_IO, n, "File write fail: %s", tgt);
    }
    if (rl != line) {
      free(rl);
    }
  }

  fclose(f);
  fclose(t);

  if (xstr) {
    int rc = utils_rename_file(tgt, src);
    if (rc) {
      node_fatal(rc, n, "Rename failed of %s to %s", tgt, src);
    }
    xstr_destroy(xstr);
  }
}

static void _on_resolve(struct node_resolve *r) {
  struct node *n = r->n;
  struct _configure_ctx *ctx = n->impl;
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };
  struct unit *unit = unit_peek();

  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);

  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') {
        slist = &ctx->sources;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }

  for (int i = 0; i < slist->num; ++i) {
    char buf[PATH_MAX];
    char *tgt, *src = *(char**) ulist_get(slist, i);
    src = path_normalize_cwd(src, unit->cache_dir, buf);
    bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
    if (!incache) {
      tgt = path_relativize_cwd(unit->dir, src, unit->dir);
      src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    } else {
      tgt = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      src = xstrdup(tgt);
    }
    if (utils_endswith(tgt, ".in")) {
      size_t len = strlen(tgt);
      tgt[len - AK_LLEN(".in")] = '\0';
    }

    _process_file(n, src, tgt, &deps);

    free(tgt);
    free(src);
  }

  for (int i = 0; i < ctx->sources.num; ++i) {
    char *src = *(char**) ulist_get(&ctx->sources, i);
    deps_add(&deps, DEPS_TYPE_FILE, 's', src, 0);
  }

  deps_close(&deps);

  ulist_destroy_keep(&rlist);
}

static void _configure_build(struct node *n) {
  node_resolve(&(struct node_resolve) {
    .n = n,
    .path = n->vfile,
    .on_resolve = _on_resolve,
  });
}

static void _configure_init(struct node *n) {
  struct pool *pool = pool_create_empty();
  struct _configure_ctx *ctx = n->impl = pool_calloc(pool, sizeof(*ctx));
  ctx->pool = pool;
  ulist_init(&ctx->sources, 32, sizeof(char*));
}

static void _setup_item(struct node *n, const char *v) {
  char buf[PATH_MAX];
  struct _configure_ctx *ctx = n->impl;
  struct unit *unit = unit_peek();

  char *tgt = 0, *src = path_normalize_cwd(v, unit->dir, buf);
  bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
  if (!incache) {
    tgt = path_relativize_cwd(unit->dir, src, unit->dir);
    src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
  } else {
    tgt = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    src = xstrdup(tgt);
  }
  if (utils_endswith(tgt, ".in")) {
    size_t len = strlen(tgt);
    tgt[len - AK_LLEN(".in")] = '\0';
  }

  node_product_add(n, tgt, 0);

  v = pool_strdup(ctx->pool, src);
  ulist_push(&ctx->sources, &v);

  free(tgt);
  free(src);
}

static void _configure_setup(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      while (vlist_iter_next(&iter)) {
        char buf[iter.len + 1];
        memcpy(buf, iter.item, iter.len);
        buf[iter.len] = '\0';
        _setup_item(n, buf);
      }
    } else {
      _setup_item(n, v);
    }
  }
}

static void _configure_dispose(struct node *n) {
  struct _configure_ctx *ctx = n->impl;
  if (ctx) {
    ulist_destroy_keep(&ctx->sources);
    pool_destroy(ctx->pool);
  }
}

int node_configure_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->dispose = _configure_dispose;
  n->init = _configure_init;
  n->setup = _configure_setup;
  n->build = _configure_build;
  return 0;
}
