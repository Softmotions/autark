#ifndef _AMALGAMATE_
#include "env.h"
#include "script.h"
#include "paths.h"
#include "fetchreg.h"
#endif

static void _fetch_url_regcb(const struct fetcherg_entry *e, void *d) {
  struct node *n = d;
  if (e->target) {
    struct xstr *xstr = xstr_create_empty();
    xstr_printf(xstr, "dir://%s/%s", g_env.project.cache_overlay_dir, e->target);
    n->impl = xstr_destroy_keep_ptr(xstr);
  }
}

static const char* _fetch_url_value_get(struct node *n) {
  if (n->impl) {
    return n->impl;
  }
  if (!n->child) {
    return "";
  }
  const char *url = node_value(n->child);
  if (!g_env.project.cache_overlay_dir) {
    return url;
  }
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/" AUTARK_FETCHED_REG_DIST, g_env.project.cache_overlay_dir);
  if (!path_is_file(path)) {
    return url;
  }
  struct fetchreg *reg;
  int rc = fetchreg_open(path, &reg);
  if (rc) {
    node_fatal(rc, n, "Error opening fetched registry: %s", path);
  }
  fetchreg_find(reg, url, n, _fetch_url_regcb);
  fetchreg_close(reg);
  return n->impl ? n->impl : url;
}

static void _fetch_url_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

int node_fetch_url_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _fetch_url_value_get;
  n->dispose = _fetch_url_dispose;
  return 0;
}
