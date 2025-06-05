#ifndef _AMALGAMATE_

#define VERSION  "dev"
#define REVISION ""

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
#endif

struct env g_env;

static void _unit_destroy(struct unit *unit) {
  map_destroy(unit->env);
}

void unit_env_set(struct unit *u, const char *key, const char *val_) {
  char *val = val_ ? xstrdup(val_) : 0;
  map_put_str(u->env, key, val);
}

const char* unit_env_get(struct unit *u, const char *key) {
  struct unit *prev = 0;
  for (int i = g_env.stack_units.num - 1; i >= 0; --i) {
    struct unit_ctx *c = (struct unit_ctx*) ulist_get(&g_env.stack_units, i);
    if (prev != c->unit) {
      const char *ret = map_get(c->unit->env, key);
      if (ret) {
        return ret;
      }
    }
    prev = c->unit;
  }
  return 0;
}

void unit_env_remove(struct unit *u, const char *key) {
  map_remove(u->env, key);
}

const char* unit_env_get(struct unit*, const char *key);

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
    unit_env_set(unit, "AUTARK_ROOT_DIR", unit->dir);
    unit_env_set(unit, "AUTARK_CACHE_DIR", unit->cache_dir);
  }
  unit_env_set(unit, "UNIT_DIR", unit->dir);
  unit_env_set(unit, "UNIT_CACHE_DIR", unit->cache_dir);

  return unit;
}

void unit_push(struct unit *unit, struct node *n) {
  akassert(unit);
  struct unit_ctx ctx = { unit, n };
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
  }
  return ctx->unit;
}

struct unit* unit_peek(void) {
  return unit_peek_ctx().unit;
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
  if (ctx->node && (ctx->node->flags & NODE_FLG_IN_ANY)) {
    if (ctx->node->flags & NODE_FLG_IN_SRC) {
      unit_ch_src_dir(ctx->unit, prevcwd);
    } else if (ctx->node->flags & NODE_FLG_IN_CACHE) {
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
          "    -q, --quiet                 Do not output anything.\n"
          "    -v, --version               Prints version info.\n"
          "    -h, --help                  Prints usage help.\n");
  fprintf(stderr,
          "\nautark [sources_dir] [options]\n"
          "  Build project in given sources dir.\n");
  fprintf(stderr,
          "    -C, --cache=<>              Project cache/build dir. Default: ./" AUTARK_CACHE "\n");
  fprintf(stderr,
          "    -c, --clean                 Clean build cache dir.\n");
  fprintf(stderr, "\nautark <cmd> [options]\n");
  fprintf(stderr, "  Execute a given command from checker script.\n");
  fprintf(stderr,
          "\nautark set <key>=<value>\n"
          "  Sets key/value pair as output for check script.\n");
  fprintf(stderr,
          "\nautark dep <file>\n"
          "  Registers a given file as dependency for check script.\n");
  fprintf(stderr,
          "\nautark env <env>\n"
          "  Registers a given environment variable as dependency for check script.\n");

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
  akcheck(chdir(root_dir));

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
      int rc = path_rmdir(g_env.project.cache_dir);
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

void _build(void) {
  struct sctx *x;
  int rc = script_open(AUTARK_SCRIPT, &x);
  if (rc) {
    akfatal(rc, "Failed to open script: %s", AUTARK_SCRIPT);
  }
  script_build(x);
  script_close(&x);
  akinfo("Build success");
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

void autark_run(int argc, const char **argv) {
  akassert(argc > 0 && argv[0]);
  autark_init();

  static const struct option long_options[] = {
    { "cache", 1, 0, 'C' },
    { "clean", 0, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "verbose", 0, 0, 'V' },
    { "quiet", 0, 0, 'q' },
    { "version", 0, 0, 'v' },
    { 0 }
  };

  bool version = false;

  for (int ch; (ch = getopt_long(argc, (void*) argv, "+C:chVvq", long_options, 0)) != -1; ) {
    switch (ch) {
      case 'C':
        g_env.project.cache_dir = pool_strdup(g_env.pool, optarg);
        break;
      case 'V':
        g_env.verbose = 1;
        break;
      case 'q':
        g_env.quiet = 1;
        break;
      case 'c':
        g_env.project.cleanup = true;
        break;
      case 'v':
        version = true;
        break;
      case 'h':
      default:
        _usage(0);
    }
  }

  if (version) {
    printf(VERSION);
    if (g_env.verbose) {
      puts("\nRevision: " REVISION);
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

  const char *home = getenv("AUTARK_HOME");
  if (home) {
    g_env.spawn.extra_env_paths = path_normalize_pool(pool_strdup(g_env.pool, home), g_env.pool);
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
    } else { // Root dir expected
      g_env.project.root_dir = pool_strdup(g_env.pool, arg);
    }
  }

  autark_build_prepare(AUTARK_SCRIPT);
  _build();
}
