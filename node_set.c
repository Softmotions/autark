#include "script.h"
#include "xstr.h"
#include "env.h"
#include "utils.h"

#include <string.h>

static void _split_value_add(struct node *n, const char *v, struct xstr *xstr) {
  if (env_value_is_list(v)) {
    xstr_cat(xstr, v);
    return;
  }
  char buf[strlen(v) + 1];
  const char *p = v;

  while (*p) {
    while (utils_char_is_space(*p)) ++p;
    if (*p == '\0') {
      break;
    }
    char *w = buf;
    char q = 0;

    while (*p && (q || !utils_char_is_space(*p))) {
      if (*p == '\\') {
        ++p;
        if (*p) {
          *w++ = *p++;
        }
      } else if (q) {
        if (*p == q) {
          q = 0;
          ++p;
        } else {
          *w++ = *p++;
        }
      } else if (*p == '\'' || *p == '"') {
        q = *p++;
      } else {
        *w++ = *p++;
      }
    }
    *w = '\0';
    xstr_cat(xstr, "\1");
    xstr_cat(xstr, buf);
  }
}

static void _init(struct node *n) {
  const char *name = node_value(n->child);
  if (!name) {
    return;
  }
  struct node *nn = n->child->next;
  if (!nn) {
    node_env_set(n, name, "");
    return;
  }
  if (!nn->next && nn->value[0] != '.') { // Single value
    node_env_set(n, name, node_value(nn));
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  for ( ; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (!v) {
      v = "";
    }
    if (nn->value[0] == '.' && nn->value[1] == '.') {
      _split_value_add(n, v, xstr);
    } else {
      xstr_cat(xstr, "\1");
      xstr_cat(xstr, v);
    }
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}

int node_set_setup(struct node *n) {
  n->init = _init;
  return 0;
}
