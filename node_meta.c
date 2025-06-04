#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"

#include <string.h>
#endif

static void _meta_on_let(struct node *n, struct node *e) {
  const char *name = node_value(e->child);
  if (name == 0) {
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = e->child->next; nn; nn = nn->next) {
    if (nn != e->child->next) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *nv = node_value(nn);
    xstr_cat(xstr, nv);
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}

static void _meta_on_entry(struct node *n, struct node *e) {
  if (e->type != NODE_TYPE_BAG) {
    return;
  }
  const int plen = AK_LLEN("META_");
  char name[plen + strlen(e->value) + 1];
  char *wp = name;
  memcpy(wp, "META_", plen), wp += plen;
  memcpy(wp, e->value, sizeof(name) - plen);
  for (int i = plen; i < sizeof(name) - 1; ++i) {
    name[i] = utils_toupper_ascii(name[i]);
  }
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = e->child; nn; nn = nn->next) {
    if (nn != e->child) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *nv = node_value(nn);
    xstr_cat(xstr, nv);
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}

static void _meta_init(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "let") == 0) {
      _meta_on_let(n, nn);
    } else {
      _meta_on_entry(n, nn);
    }
  }
}

int node_meta_setup(struct node *n) {
  n->init = _meta_init;
  return 0;
}
