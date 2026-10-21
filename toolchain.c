#ifndef _AMALGAMATE_
#include "toolchain.h"
#include "script.h"
#include "env.h"
#include "utils.h"
#include "xstr.h"
#include "paths.h"
#include "log.h"
#include "alloc.h"

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#define TOOLCHAIN_VAR_PREFIX "TOOLCHAIN_"

/// Well known descriptor keys and the toolchain field they define.
///
/// Descriptor keys are not limited to this list: any other key declared by a
/// descriptor is still published as `TOOLCHAIN_<KEY>`.
struct _toolchain_key {
  const char *key;   // Normalized key name
  size_t     offset; // Offset of the target field in struct toolchain
};

#define TK(key__, field__) { key__, offsetof(struct toolchain, field__) }

static const struct _toolchain_key _toolchain_keys[] = {
  TK("id", id),
  TK("system", system),
  TK("os", system),
  TK("arch", arch),
  TK("arch_alt", arch_alt),
  TK("archalt", arch_alt),
  TK("bits", bits),
  TK("bitness", bits),
  TK("endian", endian),
  TK("endianness", endian),
  TK("cc", cc),
  TK("cxx", cxx),
  TK("ar", ar),
  TK("ranlib", ranlib),
  TK("strip", strip),
  TK("pkgconf", pkgconf),
  TK("cflags", cflags),
  TK("ldflags", ldflags),
  TK("host_cc", host_cc),
  TK("host_cxx", host_cxx),
  TK("host_ar", host_ar),
  TK("host_cflags", host_cflags),
  TK("host_cxxflags", host_cxxflags),
  TK("host_ldflags", host_ldflags),
  TK("runner", runner),
};

/// Normalizes a descriptor key: lowercase, `-` and `.` are treated as `_`.
static char* _key_normalize(char *wp, size_t wp_size, const char *rp) {
  char *ret = wp;
  char *ep = wp + wp_size - 1;
  for ( ; *rp && wp < ep; ++rp) {
    char ch = (char) tolower((unsigned char) *rp);
    if (ch == '-' || ch == '.') {
      ch = '_';
    }
    *wp++ = ch;
  }
  *wp = '\0';
  return ret;
}

const char* toolchain_var_get(struct toolchain *tc, const char *key) {
  if (!tc || !key) {
    return 0;
  }
  char normalized[256];
  _key_normalize(normalized, sizeof(normalized), key);
  for (int i = tc->vars.num - 1; i >= 0; --i) { // The last declaration wins
    struct toolchain_var *v = *(struct toolchain_var**) ulist_get(&tc->vars, i);
    if (strcmp(v->key, normalized) == 0) {
      return v->val;
    }
  }
  return 0;
}

static const struct _toolchain_key* _key_lookup(const char *normalized) {
  for (size_t i = 0; i < sizeof(_toolchain_keys) / sizeof(_toolchain_keys[0]); ++i) {
    if (strcmp(_toolchain_keys[i].key, normalized) == 0) {
      return &_toolchain_keys[i];
    }
  }
  return 0;
}

static const char* _endian_normalize(const char *v) {
  if (!v) {
    return 0;
  }
  char buf[32];
  size_t len = strlen(v);
  if (len >= sizeof(buf)) {
    return 0;
  }
  for (size_t i = 0; i <= len; ++i) {
    buf[i] = (char) tolower((unsigned char) v[i]);
  }
  if (!strcmp(buf, "little") || !strcmp(buf, "little-endian") || !strcmp(buf, "le") || !strcmp(buf, "lsb")) {
    return "little";
  }
  if (!strcmp(buf, "big") || !strcmp(buf, "big-endian") || !strcmp(buf, "be") || !strcmp(buf, "msb")) {
    return "big";
  }
  return 0;
}

/// Collects all values of a descriptor field into a single whitespace separated string.
static char* _field_value(struct node *bag, struct xstr *xstr) {
  for (struct node *nn = bag->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (!v || *v == '\0') {
      continue;
    }
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (iter.len) {
          if (xstr_size(xstr)) {
            xstr_cat2(xstr, " ", 1);
          }
          xstr_cat2(xstr, iter.item, iter.len);
        }
      }
    } else {
      if (xstr_size(xstr)) {
        xstr_cat2(xstr, " ", 1);
      }
      xstr_cat(xstr, v);
    }
  }
  if (xstr_size(xstr)) {
    return xstr_destroy_keep_ptr(xstr);
  }
  xstr_destroy(xstr);
  return 0;
}

static struct node* _toolchain_rule_find(struct node *root) {
  for (struct node *nn = root->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_BAG && strcmp(nn->value, "toolchain") == 0) {
      return nn;
    }
  }
  return 0;
}

/// Resolves the structural parts of a descriptor fragment.
///
/// Only `set`/`let` and `if` rules are interpreted; any other rule is plain
/// key/value data. This keeps arbitrary keys usable even when they collide
/// with build rule names (`cc`, `check`, `run`, ...): descriptors are data,
/// not build scripts.
static void _fragment_resolve(struct node *n, int depth) {
  struct node *nn = n->child;
  while (nn) {
    if (nn->type == NODE_TYPE_IF) {
      node_init(nn);
      // The rule is spliced into its parent, so restart the sibling scan
      nn = n->child;
      continue;
    }
    struct node *next = nn->next;
    if (nn->type == NODE_TYPE_SET) {
      node_init(nn);
    } else if (nn->type == NODE_TYPE_BAG && depth < 3) {
      _fragment_resolve(nn, depth + 1);
    }
    nn = next;
  }
}

static char* _id_from_path(const char *path) {
  char buf[PATH_MAX];
  utils_strncpy(buf, path, sizeof(buf));
  char *base = path_basename(buf);
  char *dot = strrchr(base, '.');
  if (dot && dot != base) {
    *dot = '\0';
  }
  return xstrdup(base);
}

/// Registers a single descriptor key/value pair.
static void _var_add(struct pool *pool, struct toolchain *tc, struct node *n, const char *key, const char *val) {
  const struct _toolchain_key *k = _key_lookup(key);
  if (k) {
    *(const char**) ((char*) tc + k->offset) = val;
  } else if (g_env.verbose) {
    node_info(n, "Toolchain key: %s", key);
  }

  size_t vlen = strlen(TOOLCHAIN_VAR_PREFIX) + strlen(key) + 1;
  char *var = pool_alloc(pool, vlen);
  utils_strncpy(var, TOOLCHAIN_VAR_PREFIX, vlen);
  char *wp = var + strlen(TOOLCHAIN_VAR_PREFIX);
  for (const char *rp = key; *rp; ++rp) {
    *wp++ = (char) toupper((unsigned char) *rp);
  }
  *wp = '\0';

  struct toolchain_var *tv = pool_calloc(pool, sizeof(*tv));
  *tv = (struct toolchain_var) {
    .key = key,
    .var = var,
    .val = val,
  };
  ulist_push(&tc->vars, &tv);
}

struct toolchain* toolchain_load(struct pool *pool, const char *path) {
  char abs[PATH_MAX];
  if (!path_normalize_cwd(path, g_env.cwd, abs)) {
    akfatal(AK_ERROR_FAIL, "Failed to resolve toolchain descriptor path: %s", path);
  }
  if (!path_is_file(abs)) {
    akfatal(AK_ERROR_FAIL, "Toolchain descriptor not found: %s", abs);
  }

  char buf[PATH_MAX];
  utils_strncpy(buf, abs, sizeof(buf));
  const char *base = path_basename(buf);

  struct pool *fpool = pool_create(on_unit_pool_destroy);
  const char *unit_path = pool_printf(fpool, ".autark-toolchain/%s", base);

  struct sctx *sctx = 0;
  int rc = script_open_fragment(abs, unit_path, fpool, 0, &sctx);
  if (rc) {
    pool_destroy(fpool);
    akfatal(rc, "Failed to parse toolchain descriptor: %s", abs);
  }

  struct node *root = sctx->root;
  _fragment_resolve(root, 0); // Resolves `set` variables and `if` conditions

  struct node *tn = _toolchain_rule_find(root);
  if (!tn) {
    script_close(&sctx);
    pool_destroy(fpool);
    akfatal(AK_ERROR_FAIL, "Toolchain descriptor has no `toolchain { }` rule: %s", abs);
  }

  struct toolchain *tc = pool_calloc(pool, sizeof(*tc));
  ulist_init(&tc->vars, 32, sizeof(struct toolchain_var*));
  tc->path = pool_strdup(pool, abs);

  for (struct node *nn = tn->child; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      node_warn(nn, "Toolchain descriptor expects `key { value }` declarations, ignored: %s", nn->value);
      continue;
    }
    char key[256];
    _key_normalize(key, sizeof(key), nn->value);
    struct xstr *xstr = xstr_create_empty();
    char *val = _field_value(nn, xstr);
    _var_add(pool, tc, nn, pool_strdup(pool, key), pool_strdup(pool, val ? val : ""));
    free(val);
  }

  if (!tc->id) {
    char *id = _id_from_path(abs);
    tc->id = pool_strdup(pool, id);
    free(id);
  }
  if (!tc->arch_alt) {
    tc->arch_alt = tc->arch;
  }
  if (tc->system) {
    char *system = xstrdup(tc->system);
    for (char *p = system; *p; ++p) {
      *p = (char) tolower((unsigned char) *p);
    }
    tc->system = pool_strdup(pool, system);
    free(system);
  }
  if (tc->endian) {
    const char *endian = _endian_normalize(tc->endian);
    if (!endian) {
      script_close(&sctx);
      pool_destroy(fpool);
      akfatal(AK_ERROR_FAIL, "Toolchain descriptor: unknown endianness: %s", tc->endian);
    }
    tc->endian = endian;
  }
  if (!tc->cc) {
    akwarn("Toolchain descriptor: %s declares no target C compiler (cc { ... })", abs);
  }

  script_close(&sctx);
  pool_destroy(fpool);
  return tc;
}

/// Publishes a value as a build script variable and as a process environment variable.
static void _publish(struct unit *root, const char *key, const char *val) {
  if (!val || *val == '\0') {
    return;
  }
  unit_env_set_val(root, key, val);
  setenv(key, val, 1);
}

static void _publish_upper(struct unit *root, const char *prefix, const char *val) {
  if (!val || *val == '\0') {
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  xstr_cat(xstr, prefix);
  for (const char *p = val; *p; ++p) {
    char ch = (char) toupper((unsigned char) *p);
    if (ch == '-' || ch == '.') {
      ch = '_';
    }
    xstr_cat2(xstr, &ch, 1);
  }
  _publish(root, xstr_ptr(xstr), "1");
  xstr_destroy(xstr);
}

void toolchain_activate(void) {
  if (g_env.project.toolchain || !g_env.project.toolchain_path) {
    return;
  }
  struct toolchain *tc = toolchain_load(g_env.pool, g_env.project.toolchain_path);
  toolchain_apply(tc);
}

const char* toolchain_id(void) {
  return g_env.project.toolchain ? g_env.project.toolchain->id : 0;
}

void toolchain_apply(struct toolchain *tc) {
  if (!tc) {
    return;
  }
  g_env.project.toolchain = tc;
  struct unit *root = unit_root();

  // Every declared key is exposed as TOOLCHAIN_<KEY>
  for (int i = 0; i < tc->vars.num; ++i) {
    struct toolchain_var *v = *(struct toolchain_var**) ulist_get(&tc->vars, i);
    _publish(root, v->var, v->val);
  }

  _publish(root, AUTARK_TARGET_ENV, tc->id);
  _publish(root, AUTARK_TOOLCHAIN_ENV, tc->path);
  _publish(root, "SYSTEM_CROSS", "1");
  _publish(root, "SYSTEM_TARGET", tc->id);
  _publish(root, "SYSTEM_TOOLCHAIN", tc->path);
  _publish(root, "SYSTEM_ARCH", tc->arch);
  _publish(root, "SYSTEM_ARCH_ALT", tc->arch_alt);
  if (tc->system) {
    _publish(root, "SYSTEM_NAME", tc->system);
    _publish_upper(root, "SYSTEM_", tc->system);
  }
  // Declared bitness/endianness are the fallback values for check scripts.
  // When a runner is declared the target probe is executed, so the measured
  // facts must not be shadowed by stale declared ones.
  bool declared_facts = (tc->runner == 0 || *tc->runner == '\0');
  if (tc->bits) {
    struct xstr *xstr = xstr_create_empty();
    xstr_printf(xstr, "SYSTEM_BITNESS_%s", tc->bits);
    _publish(root, "SYSTEM_BITNESS", tc->bits);
    if (declared_facts) {
      _publish(root, xstr_ptr(xstr), "1");
    }
    xstr_destroy(xstr);
  }
  if (tc->endian) {
    _publish(root, "SYSTEM_ENDIAN", tc->endian);
    if (declared_facts) {
      if (strcmp(tc->endian, "big") == 0) {
        _publish(root, "SYSTEM_BIGENDIAN", "1");
      } else {
        _publish(root, "SYSTEM_LITTLE_ENDIAN", "1");
      }
    }
  }
  _publish(root, "RUNNER", tc->runner);
  _publish(root, "SYSTEM_RUNNER", tc->runner);

  _publish(root, "CC", tc->cc);
  _publish(root, "CXX", tc->cxx);
  _publish(root, "AR", tc->ar);
  _publish(root, "RANLIB", tc->ranlib);
  _publish(root, "STRIP", tc->strip);
  _publish(root, "PKGCONF", tc->pkgconf);
  _publish(root, "CFLAGS_TARGET", tc->cflags);
  _publish(root, "LDFLAGS_TARGET", tc->ldflags);

  _publish(root, "HOST_CC", tc->host_cc);
  _publish(root, "HOST_CXX", tc->host_cxx);
  _publish(root, "HOST_AR", tc->host_ar);
  _publish(root, "HOST_CFLAGS", tc->host_cflags);
  _publish(root, "HOST_CXXFLAGS", tc->host_cxxflags);
  _publish(root, "HOST_LDFLAGS", tc->host_ldflags);
}

const char* toolchain_object_tag(bool host_scope) {
  if (host_scope) {
    return TOOLCHAIN_TAG_HOST;
  }
  return toolchain_id();
}

static int _tag_path(const char *obj_path, char buf[PATH_MAX]) {
  int rc = snprintf(buf, PATH_MAX, "%s.autark-tag", obj_path);
  if (rc < 0 || rc >= PATH_MAX) {
    return ENAMETOOLONG;
  }
  return 0;
}

int toolchain_object_tag_write(const char *obj_path, const char *tag) {
  char path[PATH_MAX];
  int rc = _tag_path(obj_path, path);
  if (rc) {
    return rc;
  }
  if (!tag || *tag == '\0') {
    unlink(path);
    return 0;
  }
  return utils_file_write_buf(path, tag, strlen(tag), false);
}

const char* toolchain_object_check(const char *obj_path, bool host_scope, char *buf, size_t buflen) {
  char path[PATH_MAX];
  if (_tag_path(obj_path, path)) {
    return 0;
  }
  FILE *f = fopen(path, "r");
  if (!f) {
    return 0;
  }
  char *rp = fgets(buf, (int) buflen, f);
  fclose(f);
  if (!rp) {
    return 0;
  }
  size_t len = strlen(buf);
  while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
    buf[--len] = '\0';
  }
  if (len == 0) {
    return 0;
  }
  const char *expect = toolchain_object_tag(host_scope);
  if (expect && strcmp(expect, buf) == 0) {
    return 0;
  }
  return buf;
}
