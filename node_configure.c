#include "script.h"
#include "env.h"
#include "pool.h"
#include "paths.h"
#include "utils.h"
#include "xstr.h"
#include "ulist.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

struct _ctx {
  struct pool *pool;
  struct node *n;
  struct ulist sources;  // char*
};

static char* _line_replace(struct node *n, struct deps *deps, char *line) {
  char *s = line;
  char *sp = strchr(line, '@');
  if (!sp) {
    return line;
  }
  struct xstr *xstr = xstr_create_empty();
  struct xstr *kstr = xstr_create_empty();

  do {
    xstr_clear(xstr);
    xstr_cat2(xstr, s, sp - s);
    char *p = ++sp;
    while (*p) {
      if (*p == '@') {
        const char *key = xstr_ptr(kstr);
        const char *val = node_env_get(n, key);

        deps_add_env(deps, 0, key, val);

        if (!val || *val == '\0') {
          xstr_cat2(xstr, "@", 1);
          xstr_cat(xstr, key);
          xstr_cat2(xstr, "@", 1);
        } else {
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
        ++p;
        s = p;
        sp = strchr(s, '@');
      } else {
        xstr_cat2(kstr, p, 1);
      }
    }
  } while (sp);

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
    char *rl = _line_replace(n, deps, line);
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
  struct _ctx *ctx = n->impl;
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };
  struct unit *unit = unit_peek();

  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }

  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') { // Rebuild all on any outdated non source dependency (DEPS_TYPE_ENV)
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
    char *tgt, *src = *(char**) ulist_get(slist, i);
    bool incache = path_is_prefix_for(g_env.project.cache_dir, src);
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

  node_add_unit_deps(&deps);
  deps_close(&deps);

  ulist_destroy_keep(&rlist);
}

static void _build(struct node *n) {
  node_resolve(&(struct node_resolve) {
    .n = n,
    .path = n->vfile,
    .on_resolve = _on_resolve,
  });
}

static void _init(struct node *n) {
  struct pool *pool = pool_create_empty();
  struct _ctx *ctx = n->impl = pool_calloc(pool, sizeof(*ctx));
  ctx->pool = pool;
  ulist_init(&ctx->sources, 32, sizeof(char*));
}

static void _setup(struct node *n) {
  char buf[PATH_MAX];
  struct _ctx *ctx = n->impl;
  struct unit *unit = unit_peek();

  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    char *tgt = 0, *src = path_normalize_cwd(v, unit->dir, buf);
    bool incache = path_is_prefix_for(g_env.project.cache_dir, src);
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
}

static void _dispose(struct node *n) {
  struct _ctx *ctx = n->impl;
  if (ctx) {
    ulist_destroy_keep(&ctx->sources);
    pool_destroy(ctx->pool);
  }
}

int node_configure_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->dispose = _dispose;
  n->init = _init;
  n->setup = _setup;
  n->build = _build;
  return 0;
}
