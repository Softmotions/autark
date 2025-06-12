#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#include "paths.h"
#include "log.h"
#include "utils.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#endif

static const char* _dir_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }

  char cwd[PATH_MAX];
  char buf[PATH_MAX];

  struct unit *root = unit_root();
  struct xstr *xstr = xstr_create_empty();
  const char *dir = strcmp(n->value, "CRC") == 0 ? root->cache_dir : root->dir;
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }

  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (xstr_size(xstr) && iter.len) {
          if (iter.item[0] != '/' && !utils_endswith(xstr_ptr(xstr), "/")) {
            xstr_cat2(xstr, "/", 1);
          }
        }
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, v);
    }
  }
  if (xstr_size(xstr) == 0) {
    xstr_cat2(xstr, ".", 1);
  }

  char *path = path_normalize_cwd(xstr_ptr(xstr), dir, buf);
  path = path_relativize_cwd(cwd, path, cwd);

  xstr_destroy(xstr);
  n->impl = path;
  return n->impl;
}

static void _dir_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

int node_dir_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _dir_value;
  n->dispose = _dir_dispose;
  return 0;
}
