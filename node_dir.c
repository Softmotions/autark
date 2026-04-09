#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#include "paths.h"
#include "utils.h"
#include <unistd.h>
#endif

static void _node_dir_normalize_add(const char *dir, struct xstr *xstr, const char *v, char buf[PATH_MAX]) {
  if (xstr_size(xstr)) {
    if (!is_vlist(xstr_ptr(xstr))) {
      xstr_unshift(xstr, "\1", 1);
    }
    xstr_cat2(xstr, "\1", 1);
  }
  char *path = path_normalize_cwd(v, dir, buf);
  xstr_cat(xstr, path);
}

static const char* _dir_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }

  char buf[PATH_MAX];
  struct unit *root = unit_root();
  struct unit *unit = unit_peek();
  struct xstr *xstr = xstr_create_empty();
  const char *dir = 0;

  if (n->value[0] == 'S') {
    if (n->value[1] == 'S') {
      dir = unit->dir;
    } else {
      dir = root->dir;
    }
  } else {
    if (n->value[1] == 'C') {
      dir = unit->cache_dir;
    } else {
      dir = root->cache_dir;
    }
  }

  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (iter.len) {
          char vbuf[iter.len + 1];
          utils_strnncpy(vbuf, iter.item, iter.len, iter.len + 1);
          _node_dir_normalize_add(dir, xstr, vbuf, buf);
        }
      }
    } else {
      _node_dir_normalize_add(dir, xstr, v, buf);
    }
  }

  if (xstr_size(xstr) == 0) {
    _node_dir_normalize_add(dir, xstr, ".", buf);
  }

  n->impl = xstr_destroy_keep_ptr(xstr);
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
