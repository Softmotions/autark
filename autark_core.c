#ifndef _AMALGAMATE_

#ifndef META_VERSION
#define META_VERSION "dev"
#endif

#ifndef META_REVISION
#define META_REVISION ""
#endif

#include "env.h"
#include "utils.h"
#include "log.h"
#include "paths.h"
#include "script.h"
#include "autark.h"
#include "map.h"
#include "alloc.h"
#include "deps.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <glob.h>
#endif

struct env g_env;

static void _unit_destroy(struct unit *unit) {
  map_destroy(unit->env);
}

void unit_env_set_val(struct unit *u, const char *key, const char *val) {
  size_t len = sizeof(struct unit_env_item);
  if (val) {
    len += strlen(val) + 1;
  }
  struct unit_env_item *item = xmalloc(len);
  item->n = 0;
  if (len > sizeof(struct unit_env_item)) {
    char *wp = ((char*) item) + sizeof(struct unit_env_item);
    memcpy(wp, val, len - sizeof(struct unit_env_item));
    item->val = wp;
  } else {
    item->val = 0;
  }
  map_put_str(u->env, key, item);
}

void unit_env_set_node(struct unit *u, const char *key, struct node *n) {
  struct unit_env_item *item = xmalloc(sizeof(*item));
  item->val = 0;
  item->n = n;
  map_put_str(u->env, key, item);
}

struct node* unit_env_get_node(struct unit *u, const char *key) {
  struct unit_env_item *item = map_get(u->env, key);
  if (item) {
    return item->n;
  }
  return 0;
}

const char* unit_env_get_raw(struct unit *u, const char *key) {
  struct unit_env_item *item = map_get(u->env, key);
  if (item) {
    if (item->val) {
      return item->val;
    } else {
      return node_value(item->n);
    }
  }
  return 0;
}

const char* unit_env_get(struct node *n, const char *key) {
  struct unit *prev = 0;
  for (struct node *nn = n; nn; nn = nn->parent) {
    if (nn->unit && nn->unit != prev) {
      prev = nn->unit;
      const char *ret = unit_env_get_raw(nn->unit, key);
      if (ret) {
        return ret;
      }
    }
  }
  return getenv(key);
}

void unit_env_remove(struct unit *u, const char *key) {
  map_remove(u->env, key);
}

void on_unit_pool_destroy(struct pool *pool) {
  for (int i = 0; i < g_env.units.num; ++i) {
    struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
    if (unit->pool == pool) {
      map_remove(g_env.map_path_to_unit, unit->source_path);
      map_remove(g_env.map_path_to_unit, unit->cache_path);
      _unit_destroy(unit);
      ulist_remove(&g_env.units, i);
      --i;
    }
  }
}

struct unit* unit_for_path(const char *path) {
  struct unit *unit = map_get(g_env.map_path_to_unit, path);
  return unit;
}

struct unit* unit_create(const char *unit_path_, unsigned flags, struct pool *pool) {
  char path[PATH_MAX];
  const char *unit_path = unit_path_;

  if (path_is_absolute(unit_path)) {
    const char *p = path_is_prefix_for(g_env.project.cache_dir, unit_path, 0);
    if (p) {
      unit_path = p;
    } else {
      p = path_is_prefix_for(g_env.project.root_dir, unit_path, 0);
      if (!p) {
        akfatal(AK_ERROR_FAIL, "Unit path: %s must be either in project root or cache directory", unit_path_);
      }
      unit_path = p;
    }
  }

  struct unit *unit = pool_calloc(g_env.pool, sizeof(*unit));

  unit->flags = flags;
  unit->pool = pool;
  unit->env = map_create_str(map_kv_free);
  unit->rel_path = pool_strdup(pool, unit_path);
  unit->basename = path_basename((char*) unit->rel_path);

  unit->source_path = pool_printf(pool, "%s/%s", g_env.project.root_dir, unit_path);
  utils_strncpy(path, unit->source_path, sizeof(path));
  path_dirname(path);
  unit->dir = pool_strdup(pool, path);

  unit->cache_path = pool_printf(pool, "%s/%s", g_env.project.cache_dir, unit_path);
  utils_strncpy(path, unit->cache_path, sizeof(path));
  path_dirname(path);
  unit->cache_dir = pool_strdup(pool, path);

  int rc = path_mkdirs(unit->cache_dir);
  if (rc) {
    akfatal(rc, "Failed to create directory: %s", path);
  }

  map_put_str_no_copy(g_env.map_path_to_unit, unit->source_path, unit);
  map_put_str_no_copy(g_env.map_path_to_unit, unit->cache_path, unit);
  ulist_push(&g_env.units, &unit);

  if (g_env.verbose) {
    if (unit->flags & UNIT_FLG_ROOT) {
      akinfo("%s: AUTARK_ROOT_DIR=%s", unit->rel_path, unit->dir);
      akinfo("%s: AUTARK_CACHE_DIR=%s", unit->rel_path, unit->cache_dir);
    } else {
      akinfo("%s: UNIT_DIR=%s", unit->rel_path, unit->dir);
      akinfo("%s: UNIT_CACHE_DIR=%s", unit->rel_path, unit->cache_dir);
    }
  }
  if (unit->flags & UNIT_FLG_ROOT) {
    unit_env_set_val(unit, "AUTARK_ROOT_DIR", unit->dir);
    unit_env_set_val(unit, "AUTARK_CACHE_DIR", unit->cache_dir);
  }
  unit_env_set_val(unit, "UNIT_DIR", unit->dir);
  unit_env_set_val(unit, "UNIT_CACHE_DIR", unit->cache_dir);

  return unit;
}

void unit_push(struct unit *unit, struct node *n) {
  akassert(unit);
  struct unit_ctx ctx = { unit, 0 };
  if (n) {
    ctx.flags = n->flags;
  }
  ulist_push(&g_env.stack_units, &ctx);
  unit_ch_dir(&ctx, 0);
  setenv(AUTARK_UNIT, unit->rel_path, 1);
}

struct unit* unit_pop(void) {
  akassert(g_env.stack_units.num > 0);
  struct unit_ctx *ctx = (struct unit_ctx*) ulist_get(&g_env.stack_units, g_env.stack_units.num - 1);
  ulist_pop(&g_env.stack_units);
  struct unit_ctx peek = unit_peek_ctx();
  if (peek.unit) {
    unit_ch_dir(&peek, 0);
    setenv(AUTARK_UNIT, peek.unit->rel_path, 1);
  } else {
    unsetenv(AUTARK_UNIT);
  }
  return ctx->unit;
}

struct unit* unit_peek(void) {
  struct unit *u = unit_peek_ctx().unit;
  akassert(u);
  return u;
}

struct unit* unit_root(void) {
  akassert(g_env.stack_units.num);
  struct unit_ctx uc = *(struct unit_ctx*) ulist_get(&g_env.stack_units, 0);
  return uc.unit;
}

struct unit* unit_parent(struct node *n) {
  struct unit *u = n->unit;
  for (struct node *nn = n->parent; nn; nn = nn->parent) {
    if (!u) {
      u = nn->unit;
    } else if (nn->unit && n->unit != nn->unit) {
      return nn->unit;
    }
  }
  return unit_root();
}

struct unit_ctx unit_peek_ctx(void) {
  if (g_env.stack_units.num == 0) {
    return (struct unit_ctx) {
             0, 0
    };
  }
  return *(struct unit_ctx*) ulist_get(&g_env.stack_units, g_env.stack_units.num - 1);
}

void unit_ch_dir(struct unit_ctx *ctx, char *prevcwd) {
  if (ctx->flags & NODE_FLG_IN_ANY) {
    if (ctx->flags & NODE_FLG_IN_SRC) {
      unit_ch_src_dir(ctx->unit, prevcwd);
    } else if (ctx->flags & NODE_FLG_IN_CACHE) {
      unit_ch_cache_dir(ctx->unit, prevcwd);
    }
  } else if (ctx->unit->flags & UNIT_FLG_SRC_CWD) {
    unit_ch_src_dir(ctx->unit, prevcwd);
  } else if (!(ctx->unit->flags & UNIT_FLG_NO_CWD)) {
    unit_ch_cache_dir(ctx->unit, prevcwd);
  }
}

void unit_ch_cache_dir(struct unit *unit, char *prevcwd) {
  akassert(unit);
  if (prevcwd) {
    akassert(getcwd(prevcwd, PATH_MAX));
  }
  akcheck(chdir(unit->cache_dir));
}

void unit_ch_src_dir(struct unit *unit, char *prevcwd) {
  akassert(unit);
  if (prevcwd) {
    akassert(getcwd(prevcwd, PATH_MAX));
  }
  akcheck(chdir(unit->dir));
}

static int _usage_va(const char *err, va_list ap) {
  if (err) {
    fprintf(stderr, "\nError:  ");
    vfprintf(stderr, err, ap);
    fprintf(stderr, "\n\n");
  }
  fprintf(stderr, "Usage\n");
  fprintf(stderr, "\nCommon options:\n"
          "    -V, --verbose               Outputs verbose execution info.\n"
          "    -v, --version               Prints version info.\n"
          "    -h, --help                  Prints usage help.\n");
  fprintf(stderr,
          "\nautark [sources_dir/command] [options]\n"
          "  Build project in given sources dir.\n");
  fprintf(stderr,
          "    -H, --cache=<>              Project cache/build dir. Default: ./" AUTARK_CACHE "\n");
  fprintf(stderr,
          "    -c, --clean                 Clean build cache dir.\n");
  fprintf(stderr,
          "    -l, --options               List of all available project options and their description.\n");
  fprintf(stderr,
          "    -J  --jobs=<>               Number of jobs used in c/cxx compilation tasks. Default: 4\n");
  fprintf(stderr,
          "    -D<option>[=<val>]          Set project build option.\n");
  fprintf(stderr,
          "    -I, --install               Install all built artifacts\n");
  fprintf(stderr,
          "    -R, --prefix=<>             Install prefix. Default: $HOME/.local\n");
  fprintf(stderr,
          "        --bindir=<>             Path to 'bin' dir relative to a `prefix` dir. Default: bin\n");
  fprintf(stderr,
          "        --libdir=<>             Path to 'lib' dir relative to a `prefix` dir. Default: lib\n");
  fprintf(stderr,
          "        --datadir=<>            Path to 'data' dir relative to a `prefix` dir. Default: share\n");
  fprintf(stderr,
          "        --includedir=<>         Path to 'include' dir relative to `prefix` dir. Default: include\n");
  fprintf(stderr,
          "        --mandir=<>             Path to 'man' dir relative to `prefix` dir. Default: share/man\n");
#ifdef __FreeBSD__
  fprintf(stderr,
          "        --pkgconfdir=<>         Path to 'pkgconfig' dir relative to prefix dir. Default: libdata/pkgconfig");
#else
  fprintf(stderr,
          "        --pkgconfdir=<>         Path to 'pkgconfig' dir relative to prefix dir. Default: lib/pkgconfig");
#endif
  fprintf(stderr, "\nautark <cmd> [options]\n");
  fprintf(stderr, "  Execute a given command from check script.\n");
  fprintf(stderr,
          "\nautark set <key>=<value>\n"
          "  Sets key/value pair as output for check script.\n");
  fprintf(stderr,
          "\nautark dep <file>\n"
          "  Registers a given file as dependency for check script.\n");
  fprintf(stderr,
          "\nautark env <env>\n"
          "  Registers a given environment variable as dependency for check script.\n");
  fprintf(stderr,
          "\nautark glob <pattern>\n"
          "   -C, --dir                   Current directory for glob list.\n"
          "  Lists files in current directory filtered by glob pattern.\n");

  fprintf(stderr, "\n");
  return AK_ERROR_INVALID_ARGS;
}

__attribute__((noreturn)) static void _usage(const char *err, ...) {
  va_list ap;
  va_start(ap, err);
  _usage_va(err, ap);
  va_end(ap);
  _exit(1);
}

void autark_build_prepare(const char *script_path) {
  if (g_env.project.prepared) {
    return;
  }
  g_env.project.prepared = true;

  char path_buf[PATH_MAX];
  autark_init();

  if (!g_env.project.root_dir) {
    if (script_path) {
      utils_strncpy(path_buf, script_path, PATH_MAX);
      g_env.project.root_dir = pool_strdup(g_env.pool, path_dirname(path_buf));
    } else {
      g_env.project.root_dir = g_env.cwd;
    }
    akassert(g_env.project.root_dir);
  }

  const char *root_dir = path_normalize_pool(g_env.project.root_dir, g_env.pool);
  if (!root_dir) {
    akfatal(AK_ERROR_FAIL, "%s is not a directory", root_dir);
  }

  g_env.project.root_dir = root_dir;
  g_env.cwd = root_dir;
  if (chdir(root_dir)) {
    akfatal(errno, "Failed to change dir to: %s", root_dir);
  }

  if (!g_env.project.cache_dir) {
    g_env.project.cache_dir = AUTARK_CACHE;
  }

  g_env.project.cache_dir = path_normalize_pool(g_env.project.cache_dir, g_env.pool);
  if (!g_env.project.cache_dir) {
    akfatal(errno, "Failed to resolve project CACHE dir: %s", g_env.project.cache_dir);
  }

  if (path_is_prefix_for(g_env.project.cache_dir, g_env.project.root_dir, 0)) {
    akfatal(AK_ERROR_FAIL, "Project cache dir cannot be parent of project root dir", 0);
  }

  if (g_env.project.cleanup) {
    if (path_is_dir(g_env.project.cache_dir)) {
      int rc = path_rm_cache(g_env.project.cache_dir);
      if (rc) {
        akfatal(rc, "Failed to remove cache directory: %s", g_env.project.cache_dir);
      }
    }
  }

  utils_strncpy(path_buf, script_path, PATH_MAX);
  const char *path = path_basename(path_buf);

  struct unit *unit = unit_create(path, UNIT_FLG_SRC_CWD | UNIT_FLG_ROOT, g_env.pool);
  unit_push(unit, 0);

  if (!path_is_dir(g_env.project.cache_dir) || !path_is_accesible_read(g_env.project.cache_dir)) {
    akfatal(AK_ERROR_FAIL, "Failed to access build CACHE directory: %s", g_env.project.cache_dir);
  }

  setenv(AUTARK_ROOT_DIR, g_env.project.root_dir, 1);
  setenv(AUTARK_CACHE_DIR, g_env.project.cache_dir, 1);

  if (g_env.verbose) {
    setenv(AUTARK_VERBOSE, "1", 1);
  }
}

static void _project_env_read(void) {
  autark_init();

  const char *val = getenv(AUTARK_CACHE_DIR);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_CACHE_DIR env variable", 0);
  }
  g_env.project.cache_dir = pool_strdup(g_env.pool, val);

  val = getenv(AUTARK_ROOT_DIR);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_ROOT_DIR env variable", 0);
  }
  g_env.project.root_dir = pool_strdup(g_env.pool, val);

  val = getenv(AUTARK_UNIT);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_UNIT env variable", 0);
  }
  if (path_is_absolute(val)) {
    akfatal(AK_ERROR_FAIL, "AUTARK_UNIT cannot be an absolute path", 0);
  }

  struct unit *unit = unit_create(val, UNIT_FLG_NO_CWD, g_env.pool);
  unit_push(unit, 0);
}

static void _on_command_set(int argc, const char **argv) {
  _project_env_read();
  const char *kv;
  if (optind >= argc) {
    _usage("Missing <key> argument");
  }
  struct unit *unit = unit_peek();
  const char *env_path = pool_printf(g_env.pool, "%s.env.tmp", unit->cache_path);
  FILE *f = fopen(env_path, "a+");
  if (!f) {
    akfatal(errno, "Failed to open file: %s", env_path);
  }
  for ( ; optind < argc; ++optind) {
    kv = argv[optind];
    if (strchr(kv, '=')) {
      if (g_env.verbose) {
        akinfo("autark set %s", kv);
      }
      fprintf(f, "%s\n", kv);
    }
  }
  fclose(f);
}

static void _on_command_dep(int argc, const char **argv) {
  _project_env_read();
  if (optind >= argc) {
    _usage("Missing required dependency option");
  }
  const char *file = argv[optind];
  if (g_env.verbose) {
    akinfo("autark dep %s", file);
  }
  int type = DEPS_TYPE_FILE;
  if (strcmp(file, "-") == 0) {
    type = DEPS_TYPE_OUTDATED;
  }
  struct deps deps;
  struct unit *unit = unit_peek();
  const char *deps_path = pool_printf(g_env.pool, "%s.%s", unit->cache_path, "deps.tmp");
  int rc = deps_open(deps_path, false, &deps);
  if (rc) {
    akfatal(rc, "Failed to open deps file: %s", deps_path);
  }
  rc = deps_add(&deps, type, 0, file, 0);
  if (rc) {
    akfatal(rc, "Failed to write deps file: %s", deps_path);
  }
  deps_close(&deps);
}

static void _on_command_dep_env(int argc, const char **argv) {
  _project_env_read();
  if (optind >= argc) {
    _usage("Missing required dependency option");
  }
  const char *key = argv[optind];
  if (g_env.verbose) {
    akinfo("autark dep env %s", key);
  }
  const char *val = getenv(key);
  if (!val) {
    val = "";
  }
  struct deps deps;
  struct unit *unit = unit_peek();
  const char *deps_path = pool_printf(g_env.pool, "%s.%s", unit->cache_path, "deps.tmp");
  int rc = deps_open(deps_path, false, &deps);
  if (rc) {
    akfatal(rc, "Failed to open deps file: %s", deps_path);
  }
  rc = deps_add_sys_env(&deps, 0, key, val);
  if (rc) {
    akfatal(rc, "Failed to write deps file: %s", deps_path);
  }
  deps_close(&deps);
}

static void _on_command_glob(int argc, const char **argv, const char *cdir) {
  if (cdir) {
    akcheck(chdir(cdir));
  }
  do {
    const char *pattern = "*";
    if (optind < argc) {
      pattern = argv[optind];
    }
    glob_t g;
    int rc = glob(pattern, 0, 0, &g);
    if (rc == 0) {
      for (int i = 0; i < g.gl_pathc; ++i) {
        puts(g.gl_pathv[i]);
      }
    }
    ;
    globfree(&g);
    if (rc && rc != GLOB_NOMATCH) {
      exit(1);
    }
  } while (++optind < argc);
}

#ifdef TESTS

void on_command_set(int argc, const char **argv) {
  _on_command_set(argc, argv);
}

void on_command_dep(int argc, const char **argv) {
  _on_command_dep(argc, argv);
}

void on_command_dep_env(int argc, const char **argv) {
  _on_command_dep_env(argc, argv);
}

#endif

static void _build(struct ulist *options) {
  struct sctx *x;
  int rc = script_open(AUTARK_SCRIPT, &x);
  if (rc) {
    akfatal(rc, "Failed to open script: %s", AUTARK_SCRIPT);
  }
  struct unit *root = unit_root();
  for (int i = 0; i < options->num; ++i) {
    const char *opt = *(char**) ulist_get(options, i);
    char *p = strchr(opt, '=');
    if (p) {
      *p = '\0';
      char *val = p + 1;
      for (int vlen = strlen(val); vlen >= 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r'); --vlen) {
        val[vlen - 1] = '\0';
      }
      unit_env_set_val(root, opt, val);
    } else {
      unit_env_set_val(root, opt, "");
    }
  }
  script_build(x);
  script_close(&x);
  akinfo("[%s] Build successful", g_env.project.root_dir);
}

static void _options(void) {
  struct sctx *x;
  int rc = script_open(AUTARK_SCRIPT, &x);
  if (rc) {
    akfatal(rc, "Failed to open script: %s", AUTARK_SCRIPT);
  }
  node_init(x->root);
  if (xstr_size(g_env.project.options)) {
    fprintf(stderr, "\n%s", xstr_ptr(g_env.project.options));
  }
}

void autark_init(void) {
  if (!g_env.pool) {
    g_env.stack_units.usize = sizeof(struct unit_ctx);
    g_env.units.usize = sizeof(struct unit*);
    g_env.pool = pool_create_empty();
    char buf[PATH_MAX];
    if (!getcwd(buf, PATH_MAX)) {
      akfatal(errno, 0, 0);
    }
    g_env.cwd = pool_strdup(g_env.pool, buf);
    g_env.map_path_to_unit = map_create_str(0);
  }
}

AK_DESTRUCTOR void autark_dispose(void) {
  if (g_env.pool) {
    struct pool *pool = g_env.pool;
    g_env.pool = 0;
    for (int i = 0; i < g_env.units.num; ++i) {
      struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
      _unit_destroy(unit);
    }
    ulist_destroy_keep(&g_env.units);
    ulist_destroy_keep(&g_env.stack_units);
    map_destroy(g_env.map_path_to_unit);
    pool_destroy(pool);
    memset(&g_env, 0, sizeof(g_env));
  }
}

const char* env_libdir(void) {
 #if defined(__linux__)
  #if defined(__x86_64__)
  // RPM-style or Debian-style
    #if defined(DEBIAN_MULTIARCH)
  return "lib/x86_64-linux-gnu";      // Debian/Ubuntu
    #else
  return "lib64";                     // RHEL/Fedora
    #endif
  #elif defined(__i386__)
  return "lib";
  #elif defined(__aarch64__)
    #if defined(DEBIAN_MULTIARCH)
  return "aarch64-linux-gnu";
    #else
  return "lib64";
    #endif
  #elif defined(__arm__)
  return "lib";
  #else
  return "lib";
  #endif
#elif defined(__APPLE__)
  return "lib";    // macOS uses universal binaries
#elif defined(_WIN64)
  return "C:\\Windows\\System32";
#elif defined(_WIN32)
  return "C:\\Windows\\SysWOW64";
#else
  return "lib";
#endif
}

void autark_run(int argc, const char **argv) {
  akassert(argc > 0 && argv[0]);
  autark_init();

  static const struct option long_options[] = {
    { "cache", 1, 0, 'H' },
    { "clean", 0, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "verbose", 0, 0, 'V' },
    { "version", 0, 0, 'v' },
    { "options", 0, 0, 'l' },
    { "install", 0, 0, 'I' },
    { "prefix", 1, 0, 'R' },
    { "dir", 1, 0, 'C' },
    { "jobs", 1, 0, 'J' },
    { "bindir", 1, 0, -1 },
    { "libdir", 1, 0, -2 },
    { "includedir", 1, 0, -3 },
    { "pkgconfdir", 1, 0, -4 },
    { "mandir", 1, 0, -5 },
    { "datadir", 1, 0, -6 },
    { 0 }
  };

  bool version = false;
  const char *cdir = 0;
  struct ulist options = { .usize = sizeof(char*) };

  for (int ch; (ch = getopt_long(argc, (void*) argv, "+H:chVvlR:C:D:J:I", long_options, 0)) != -1; ) {
    switch (ch) {
      case 'H':
        g_env.project.cache_dir = pool_strdup(g_env.pool, optarg);
        break;
      case 'V':
        g_env.verbose = 1;
        break;
      case 'c':
        g_env.project.cleanup = true;
        break;
      case 'v':
        version = true;
        break;
      case 'l':
        if (!g_env.project.options) {
          g_env.project.options = xstr_create_empty();
        }
        break;
      case 'D': {
        char *p = pool_strdup(g_env.pool, optarg);
        ulist_push(&options, &p);
        break;
      }
      case 'R':
        g_env.install.enabled = true;
        g_env.install.prefix_dir = path_normalize_cwd_pool(optarg, g_env.cwd, g_env.pool);
        break;
      case 'I':
        g_env.install.enabled = true;
        break;
      case 'C':
        cdir = pool_strdup(g_env.pool, optarg);
        break;
      case -1:
        g_env.install.bin_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -2:
        g_env.install.lib_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -3:
        g_env.install.include_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -4:
        g_env.install.pkgconf_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -5:
        g_env.install.man_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -6:
        g_env.install.data_dir = pool_strdup(g_env.pool, optarg);
        break;
      case 'J': {
        int rc = 0;
        g_env.max_parallel_jobs = utils_strtol(optarg, 10, &rc);
        if (rc) {
          akfatal(AK_ERROR_FAIL, "Command line option -J,--jobs value: %s should be non negative number", optarg);
        }
        break;
      }
      case 'h':
      default:
        _usage(0);
    }
  }

  if (version) {
    printf(META_VERSION "." META_REVISION);
    if (g_env.verbose) {
      puts("\nRevision: " META_REVISION);
    }
    fflush(stdout);
    _exit(0);
  }

  if (!g_env.verbose) {
    const char *v = getenv(AUTARK_VERBOSE);
    if (v && *v == '1') {
      g_env.verbose = true;
    }
  }

  char buf[PATH_MAX];
  const char *autark_home = getenv("AUTARK_HOME");
  if (!autark_home) {
    utils_strncpy(buf, argv[0], sizeof(buf));
    autark_home = path_dirname(buf);
  }
  autark_home = path_normalize_pool(autark_home, g_env.pool);
  if (g_env.verbose) {
    akinfo("AUTARK_HOME=%s", autark_home);
  }
  g_env.spawn.extra_env_paths = autark_home;

  if (!g_env.install.prefix_dir) {
    char *home = getenv("HOME");
    if (home) {
      g_env.install.prefix_dir = pool_printf(g_env.pool, "%s/.local", home);
    } else {
      g_env.install.prefix_dir = "/usr/local";
    }
  }
  if (!g_env.install.bin_dir) {
    g_env.install.bin_dir = "bin";
  }
  if (!g_env.install.lib_dir) {
    g_env.install.lib_dir = env_libdir();
  }
  if (!g_env.install.data_dir) {
    g_env.install.data_dir = "share";
  }
  if (!g_env.install.include_dir) {
    g_env.install.include_dir = "include";
  }
  if (!g_env.install.man_dir) {
    g_env.install.man_dir = "share/man";
  }
  if (!g_env.install.pkgconf_dir) {
#ifdef __FreeBSD__
    g_env.install.pkgconf_dir = "libdata/pkgconfig";
#else
    g_env.install.pkgconf_dir = "lib/pkgconfig";
#endif
  }
  if (g_env.max_parallel_jobs <= 0) {
    g_env.max_parallel_jobs = 4;
  }
  if (g_env.max_parallel_jobs > 16) {
    g_env.max_parallel_jobs = 16;
  }

  if (optind < argc) {
    const char *arg = argv[optind];
    ++optind;
    if (strcmp(arg, "set") == 0) {
      _on_command_set(argc, argv);
      return;
    } else if (strcmp(arg, "dep") == 0) {
      _on_command_dep(argc, argv);
      return;
    } else if (strcmp(arg, "env") == 0) {
      _on_command_dep_env(argc, argv);
      return;
    } else if (strcmp(arg, "glob") == 0) {
      _on_command_glob(argc, argv, cdir);
      return;
    } else { // Root dir expected
      g_env.project.root_dir = pool_strdup(g_env.pool, arg);
    }
  }

  autark_build_prepare(AUTARK_SCRIPT);

  if (g_env.project.options) {
    _options();
  } else {
    _build(&options);
  }

  ulist_destroy_keep(&options);
}
