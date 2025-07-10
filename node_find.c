#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#include "utils.h"
#include "paths.h"
#include "alloc.h"
#include <string.h>
#endif

static void _library_find(struct node *n) {
  struct node *nn = n->child;
  struct unit *unit = unit_peek();

  struct xstr *names = xstr_create_empty();
  for (nn = nn->next; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      const char *val = node_value(nn);
      if (val && *val != '\0') {
        xstr_printf(names, "\1%s", val);
      }
    }
  }

  struct xstr *dirs = xstr_create_empty();
  const char *ldir = env_libdir();
  bool sld = strcmp(ldir, "lib") != 0;

  xstr_printf(dirs, "\1%s/%s/", unit->cache_dir, ldir);
  if (sld) {
    xstr_printf(dirs, "\1%s/lib/", unit->cache_dir);
  }
  if (strcmp(unit->cache_dir, g_env.project.cache_dir) != 0) {
    xstr_printf(dirs, "\1%s/%s/", g_env.project.cache_dir, ldir);
    if (sld) {
      xstr_printf(dirs, "\1%s/lib/", g_env.project.cache_dir);
    }
  }

  const char *home = getenv("HOME");
  if (home) {
    xstr_printf(dirs, "\1%s/.local/%s/", home, ldir);
    if (sld) {
      xstr_printf(dirs, "\1%s/.local/lib/", home);
    }
  }

  xstr_printf(dirs, "\1/usr/local/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/usr/local/lib/");
  }
  xstr_printf(dirs, "\1/usr/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/usr/lib/");
  }
  xstr_printf(dirs, "\1/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/lib/");
  }

  struct vlist_iter iter_dirs;
  vlist_iter_init(xstr_ptr(dirs), &iter_dirs);
  while (vlist_iter_next(&iter_dirs)) {
    struct vlist_iter iter_names;
    vlist_iter_init(xstr_ptr(names), &iter_names);
    while (vlist_iter_next(&iter_names)) {
      char buf[PATH_MAX];
      snprintf(buf, sizeof(buf), "%.*s%.*s",
               (int) iter_dirs.len, iter_dirs.item, (int) iter_names.len, iter_names.item);
      if (path_is_exist(buf)) {
        n->impl = xstrdup(buf);
        goto finish;
      }
    }
  }

finish:
  xstr_destroy(names);
  xstr_destroy(dirs);
}

static const char* _find_value_get(struct node *n) {
  if (n->impl) {
    if ((uintptr_t) n->impl == (uintptr_t) -1) {
      return "";
    } else {
      return n->impl;
    }
  }

  n->impl = (void*) (intptr_t) -1;


  if (strcmp("library", n->value) == 0) {
    _library_find(n);
  }

  if (!n->impl) {
    n->impl = (void*) (uintptr_t) -1;
  }
  if ((uintptr_t) n->impl == (uintptr_t) -1) {
    return "";
  } else {
    return n->impl;
  }
}

static struct unit* _unit_for_find(struct node *nn, const char **keyp) {
  if (nn->type == NODE_TYPE_BAG) {
    if (strcmp(nn->value, "root") == 0) {
      *keyp = node_value(nn->child);
      return unit_root();
    } else if (strcmp(nn->value, "parent") == 0) {
      *keyp = node_value(nn->child);
      return unit_parent();
    }
  } else {
    *keyp = node_value(nn);
  }
  return unit_peek();
}

static void _find_init(struct node *n) {
  const char *key = 0;
  struct unit *unit = n->child ? _unit_for_find(n->child, &key) : 0;
  if (!key) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "No name specified for '%s' directive", n->value);
    return;
  }
  unit_env_set_node(unit, key, n);
}

static void _find_dispose(struct node *n) {
  if ((uintptr_t) n->impl != (uintptr_t) -1) {
    free(n->impl);
  }
  n->impl = 0;
}

int node_find_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _find_init;
  n->value_get = _find_value_get;
  n->dispose = _find_dispose;
  return 0;
}
