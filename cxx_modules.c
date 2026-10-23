#ifndef _AMALGAMATE_
#include "cxx_modules.h"
#include "env.h"
#include "utils.h"
#include "xstr.h"
#include "log.h"
#include "paths.h"

#include <ctype.h>
#include <string.h>
#endif

/// Directory (relative to the project cache dir) holding compiled module interfaces.
#define CXX_MODULES_DIR ".cxx-modules"

/// Module map file name (gcc only) inside the BMI directory.
#define CXX_MODULES_MAP "modules.map"

/// Compiler specific module compilation style.
enum cxx_modules_style {
  CXX_MODULES_CLANG,
  CXX_MODULES_GCC,
};

/// C++20 module interface unit file extensions.
static const char* _cxx_modules_exts[] = {
  ".cppm",
  ".ixx",
  ".mpp",
  ".ccm",
  ".cxxm",
  0
};

/// Module unit declared by a source file of a cxx rule.
struct cxx_modules_src {
  const char *path;         // Absolute source path
  const char *name;         // Module name
  bool        is_interface; // Source is a module interface unit
};

/// Module interface unit registered in the project wide module registry.
struct cxx_module {
  const char  *name;   // Module name
  const char  *bmi;    // Absolute path to the compiled module interface
  const char  *tag;    // Toolchain tag of the compilation scope, zero for the native target
  struct node *owner;  // Rule providing the module
};

/// Project wide module registry.
///
/// It is populated while scripts are set up (all setups finish before any build
/// starts), so the module map of a build unit lists interfaces of all units.
/// The registry is owned by the build process and is not released explicitly.
static struct {
  struct pool *pool;
  struct ulist list; // struct cxx_module*
} _reg;

struct cxx_modules {
  struct node *n;
  struct pool *pool;
  struct ulist units;  // struct cxx_modules_src* in declaration order
  enum cxx_modules_style style;
  const char *tag;     // Toolchain tag of the compilation scope, can be zero
  const char *dir;     // Absolute BMI directory
  const char *map;     // Absolute path to the gcc module map file
};

static void _reg_init(void) {
  if (!_reg.pool) {
    _reg.pool = pool_create_empty();
    ulist_init(&_reg.list, 0, sizeof(struct cxx_module*));
  }
}

static inline bool _tag_eq(const char *a, const char *b) {
  return strcmp(a ? a : "", b ? b : "") == 0;
}

static struct cxx_module* _reg_find(const char *tag, const char *name) {
  _reg_init();
  for (int i = 0; i < _reg.list.num; ++i) {
    struct cxx_module *m = *(struct cxx_module**) ulist_get(&_reg.list, i);
    if (_tag_eq(m->tag, tag) && strcmp(m->name, name) == 0) {
      return m;
    }
  }
  return 0;
}

static bool _reg_has_tag(const char *tag) {
  _reg_init();
  for (int i = 0; i < _reg.list.num; ++i) {
    struct cxx_module *m = *(struct cxx_module**) ulist_get(&_reg.list, i);
    if (_tag_eq(m->tag, tag)) {
      return true;
    }
  }
  return false;
}

static void _reg_add(struct node *n, const char *tag, const char *name, const char *bmi) {
  struct cxx_module *prev = _reg_find(tag, name);
  if (prev) {
    node_fatal(AK_ERROR_FAIL, n, "C++ module '%s' is already provided by rule '%s': %s",
               name, prev->owner->name, prev->bmi);
  }
  _reg_init();
  struct cxx_module *m = pool_alloc(_reg.pool, sizeof(*m));
  *m = (struct cxx_module) {
    .name = pool_strdup(_reg.pool, name),
    .bmi = pool_strdup(_reg.pool, bmi),
    .tag = tag ? pool_strdup(_reg.pool, tag) : 0,
    .owner = n,
  };
  ulist_push(&_reg.list, &m);
}

static bool _is_module_ext(const char *path) {
  for (int i = 0; _cxx_modules_exts[i]; ++i) {
    if (utils_endswith(path, _cxx_modules_exts[i])) {
      return true;
    }
  }
  return false;
}

bool cxx_modules_is_module_path(const char *path) {
  return _is_module_ext(path);
}

static bool _is_ident_start(char c) {
  return isalpha((unsigned char) c) || c == '_';
}

static bool _is_ident(char c) {
  return isalnum((unsigned char) c) || c == '_';
}

struct _mod_scan {
  const char *p;
  const char *end;
};

/// Skips whitespace and comments.
static void _scan_skip(struct _mod_scan *s);

static void _scan_skip_line(struct _mod_scan *s) {
  while (s->p < s->end && *s->p != '\n') {
    ++s->p;
  }
}

static void _scan_skip_block_comment(struct _mod_scan *s) {
  s->p += 2;
  while (s->p + 1 < s->end && !(s->p[0] == '*' && s->p[1] == '/')) {
    ++s->p;
  }
  s->p = (s->p + 1 < s->end) ? s->p + 2 : s->end;
}

static void _scan_skip_quoted(struct _mod_scan *s) {
  char quote = *s->p++;
  while (s->p < s->end && *s->p != quote) {
    if (*s->p == '\\' && s->p + 1 < s->end) {
      ++s->p;
    }
    ++s->p;
  }
  if (s->p < s->end) {
    ++s->p;
  }
}

static void _scan_skip(struct _mod_scan *s) {
  for ( ;; ) {
    while (s->p < s->end && utils_char_is_space(*s->p)) {
      ++s->p;
    }
    if (s->p + 1 < s->end && s->p[0] == '/' && s->p[1] == '/') {
      _scan_skip_line(s);
      continue;
    }
    if (s->p + 1 < s->end && s->p[0] == '/' && s->p[1] == '*') {
      _scan_skip_block_comment(s);
      continue;
    }
    break;
  }
}

/// Scans a module declaration of a module unit.
///
/// Module names are not subject to macro replacement, so a lexical scan is
/// sufficient to extract them. Only the first module declaration is reported.
///
/// @param partition Set to true if the declaration names a module partition
/// @param is_interface Set to true if the declaration exports the module
///
/// @return Module name allocated from the pool or zero if the source does not
///         declare a module
static const char* _scan_module_name(
  struct node *n,
  const char *path,
  struct pool *pool,
  bool *partition,
  bool *is_interface) {
  struct value v = utils_file_as_buf(path, -1);
  if (v.error) {
    value_destroy(&v);
    node_fatal(AK_ERROR_IO, n, "Failed to read C++ module unit: %s", path);
  }
  if (!v.buf || v.len == 0) {
    value_destroy(&v);
    return 0;
  }

  const char *ret = 0;
  bool exported = false;
  bool line_start = true;
  struct _mod_scan s = { .p = v.buf, .end = (char*) v.buf + v.len };

  while (s.p < s.end) {
    char c = *s.p;
    if (c == '\n') {
      line_start = true;
      ++s.p;
      continue;
    }
    if (utils_char_is_space(c)) {
      ++s.p;
      continue;
    }
    if (line_start && c == '#') { // Skip preprocessor directives
      while (s.p < s.end && *s.p != '\n') {
        if (*s.p == '\\' && s.p + 1 < s.end && s.p[1] == '\n') {
          s.p += 2;
          continue;
        }
        ++s.p;
      }
      continue;
    }
    line_start = false;
    if (c == '/' && s.p + 1 < s.end && s.p[1] == '/') {
      _scan_skip_line(&s);
      continue;
    }
    if (c == '/' && s.p + 1 < s.end && s.p[1] == '*') {
      _scan_skip_block_comment(&s);
      continue;
    }
    if (c == '"' || c == '\'') {
      _scan_skip_quoted(&s);
      continue;
    }
    if (!_is_ident_start(c)) {
      exported = false;
      ++s.p;
      continue;
    }

    const char *tok = s.p;
    while (s.p < s.end && _is_ident(*s.p)) {
      ++s.p;
    }
    size_t tok_len = s.p - tok;

    if (tok_len == 6 && strncmp(tok, "export", 6) == 0) {
      exported = true;
      continue;
    }
    if (tok_len != 6 || strncmp(tok, "module", 6) != 0) {
      exported = false;
      continue;
    }

    _scan_skip(&s);
    if (s.p < s.end && *s.p == ';') { // Global module fragment: `module;`
      exported = false;
      continue;
    }
    if (s.p >= s.end || !_is_ident_start(*s.p)) {
      exported = false;
      continue;
    }

    const char *name = s.p;
    while (s.p < s.end && (_is_ident(*s.p) || *s.p == '.')) {
      ++s.p;
    }
    size_t name_len = s.p - name;
    _scan_skip(&s);
    if (s.p < s.end && *s.p == ':') {
      *partition = true;
    }
    ret = pool_strndup(pool, name, name_len);
    *is_interface = exported;
    break;
  }

  value_destroy(&v);
  return ret;
}

static struct cxx_modules_src* _find_src(struct cxx_modules *m, const char *path) {
  for (int i = 0; i < m->units.num; ++i) {
    struct cxx_modules_src *u = *(struct cxx_modules_src**) ulist_get(&m->units, i);
    if (strcmp(u->path, path) == 0) {
      return u;
    }
  }
  return 0;
}

static enum cxx_modules_style _detect_style(struct node *n, const char *compiler) {
  const char *style = node_env_get(n, "CXX_MODULE_STYLE");
  if (style && *style != '\0') {
    if (strcmp(style, "clang") == 0) {
      return CXX_MODULES_CLANG;
    }
    if (strcmp(style, "gcc") == 0) {
      return CXX_MODULES_GCC;
    }
    node_fatal(AK_ERROR_SCRIPT, n, "Unsupported CXX_MODULE_STYLE value: %s (expected `clang` or `gcc`)", style);
  }
  char buf[PATH_MAX];
  utils_strncpy(buf, compiler, sizeof(buf));
  return strstr(path_basename(buf), "clang") ? CXX_MODULES_CLANG : CXX_MODULES_GCC;
}

struct cxx_modules* cxx_modules_create(struct node *n, const char *compiler, const char *tag) {
  struct pool *pool = pool_create_empty();
  struct cxx_modules *m = pool_alloc(pool, sizeof(*m));
  const char *dir = pool_printf(pool, "%s/%s", g_env.project.cache_dir, CXX_MODULES_DIR);
  if (tag && *tag != '\0') {
    dir = pool_printf(pool, "%s/%s", dir, tag);
  }
  *m = (struct cxx_modules) {
    .n = n,
    .pool = pool,
    .style = _detect_style(n, compiler),
    .tag = tag ? pool_strdup(pool, tag) : 0,
    .dir = dir,
    .map = pool_printf(pool, "%s/%s", dir, CXX_MODULES_MAP),
  };
  ulist_init(&m->units, 0, sizeof(struct cxx_modules_src*));
  return m;
}

void cxx_modules_destroy(struct cxx_modules *m) {
  if (m) {
    ulist_destroy_keep(&m->units);
    pool_destroy(m->pool);
  }
}

bool cxx_modules_add_source(struct cxx_modules *m, const char *src, bool required) {
  if (!required && !_is_module_ext(src)) {
    return false;
  }
  if (_find_src(m, src)) {
    return false;
  }

  bool partition = false;
  bool is_interface = false;
  const char *name = _scan_module_name(m->n, src, m->pool, &partition, &is_interface);
  if (!name) {
    if (required) {
      node_fatal(AK_ERROR_SCRIPT, m->n,
                 "No C++ module declaration found in the source listed in `modules { }`: %s", src);
    }
    return false;
  }
  if (partition) {
    node_fatal(AK_ERROR_NOT_IMPLEMENTED, m->n,
               "Module partition units are not supported: %s (module '%s')", src, name);
  }

  struct cxx_modules_src *u = pool_alloc(m->pool, sizeof(*u));
  *u = (struct cxx_modules_src) { .path = pool_strdup(m->pool, src), .name = name, .is_interface = is_interface };
  ulist_push(&m->units, &u);

  if (!is_interface) {
    return false;
  }

  const char *ext = m->style == CXX_MODULES_CLANG ? "pcm" : "gcm";
  const char *bmi = pool_printf(m->pool, "%s/%s.%s", m->dir, name, ext);
  _reg_add(m->n, m->tag, name, bmi);
  node_product_add(m->n, bmi, 0);
  if (g_env.verbose) {
    node_info(m->n, "C++ module interface unit: %s (module '%s')", src, name);
  }
  return true;
}

bool cxx_modules_is_interface(struct cxx_modules *m, const char *src) {
  struct cxx_modules_src *u = _find_src(m, src);
  return u && u->is_interface;
}

void cxx_modules_prepare(struct cxx_modules *m) {
  if (!m || m->style != CXX_MODULES_GCC || !_reg_has_tag(m->tag)) {
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  for (int i = 0; i < _reg.list.num; ++i) {
    struct cxx_module *mod = *(struct cxx_module**) ulist_get(&_reg.list, i);
    if (_tag_eq(mod->tag, m->tag)) {
      xstr_printf(xstr, "%s %s\n", mod->name, mod->bmi);
    }
  }

  if (xstr_size(xstr) > 0) {
    struct value prev = utils_file_as_buf(m->map, -1);
    bool same = !prev.error && prev.buf
                && prev.len == xstr_size(xstr)
                && memcmp(prev.buf, xstr_ptr(xstr), prev.len) == 0;
    value_destroy(&prev);
    if (!same) {
      int rc = path_mkdirs(m->dir);
      if (rc == 0) {
        rc = utils_file_write_buf(m->map, xstr_ptr(xstr), xstr_size(xstr), false);
      }
      if (rc) {
        node_fatal(rc, m->n, "Failed to write C++ module map file: %s", m->map);
      }
    }
  }
  xstr_destroy(xstr);
}

void cxx_modules_args_add(struct cxx_modules *m, struct spawn *s, const char *src) {
  if (!m) {
    return;
  }
  struct cxx_modules_src *u = _find_src(m, src);
  if (m->style == CXX_MODULES_CLANG) {
    if (u && u->is_interface) {
      struct cxx_module *mod = _reg_find(m->tag, u->name);
      akassert(mod);
      spawn_arg_add(s, "-x");
      spawn_arg_add(s, "c++-module");
      spawn_arg_add(s, pool_printf(m->pool, "-fmodule-output=%s", mod->bmi));
    } else if (u) { // Module implementation unit
      spawn_arg_add(s, "-x");
      spawn_arg_add(s, "c++");
    }
    if (_reg_has_tag(m->tag)) {
      spawn_arg_add(s, pool_printf(m->pool, "-fprebuilt-module-path=%s", m->dir));
    }
  } else {
    if (u) {
      spawn_arg_add(s, "-x");
      spawn_arg_add(s, "c++");
    }
    if (_reg_has_tag(m->tag)) {
      spawn_arg_add(s, "-fmodules-ts");
      spawn_arg_add(s, pool_printf(m->pool, "-fmodule-mapper=%s", m->map));
    }
  }
}

void cxx_modules_deps_add(struct cxx_modules *m, struct deps *deps) {
  if (!m || !_reg_has_tag(m->tag)) {
    return;
  }
  for (int i = 0; i < _reg.list.num; ++i) {
    struct cxx_module *mod = *(struct cxx_module**) ulist_get(&_reg.list, i);
    if (_tag_eq(mod->tag, m->tag) && path_is_file(mod->bmi)) {
      deps_add(deps, DEPS_TYPE_FILE, 0, mod->bmi, 0);
    }
  }
  if (m->style == CXX_MODULES_GCC && path_is_file(m->map)) {
    deps_add(deps, DEPS_TYPE_FILE, 0, m->map, 0);
  }
}
