#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"

#include <stdlib.h>
#endif

static const char* _join_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }

  struct xstr *xstr = xstr_create_empty();

  int c = 0;
  struct node *pair[] = { 0, 0 };
  for (struct node *nn = n->child; nn; nn = nn->next, ++c) {
    if (c < 2) {
      pair[c] = nn;
    }
  }

  if (c == 2) {
    const char *vpair[] = { node_value(pair[0]), node_value(pair[1]) };
    if ((vpair[0] && !is_vlist(vpair[0]) && is_vlist(vpair[1]))) {
      const char *prefix = vpair[0];
      size_t prefix_len = strlen(prefix);
      struct vlist_iter iter;
      vlist_iter_init(vpair[1], &iter);
      while (vlist_iter_next(&iter)) {
        xstr_cat2(xstr, "\1", 1);
        xstr_cat2(xstr, prefix, prefix_len);
        xstr_cat2(xstr, iter.item, iter.len);
      }
      n->impl = xstr_destroy_keep_ptr(xstr);
      return n->impl;
    } else if (vpair[1] && !is_vlist(vpair[1]) && is_vlist(vpair[0])) {
      const char *suffix = vpair[1];
      size_t suffix_len = strlen(suffix);
      struct vlist_iter iter;
      vlist_iter_init(vpair[0], &iter);
      while (vlist_iter_next(&iter)) {
        xstr_cat2(xstr, "\1", 1);
        xstr_cat2(xstr, iter.item, iter.len);
        xstr_cat2(xstr, suffix, suffix_len);
      }
      n->impl = xstr_destroy_keep_ptr(xstr);
      return n->impl;
    }
  }

  bool list = n->value[0] == '.';
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (list) {
      xstr_cat(xstr, "\1");
    }
    const char *val = node_value(nn);
    if (is_vlist(val)) {
      struct vlist_iter iter;
      vlist_iter_init(val, &iter);
      while (vlist_iter_next(&iter)) {
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, val);
    }
  }

  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}

static void _join_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}

int node_join_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _join_value;
  n->dispose = _join_dispose;
  return 0;
}
