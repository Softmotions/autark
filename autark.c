#include "env.h"
#include "utils.h"
#include "log.h"
#include "paths.h"
#include "script.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

struct env g_env = {
  .units_stack = { .usize = sizeof(struct unit*) },
  .units = { .usize = sizeof(struct unit*) }
};

static void _unit_destroy(struct unit *unit) {
  map_destroy(unit->env);
}

struct unit* unit_create(const char *unit_path) {
  akassert(unit_path && !path_is_absolute(unit_path));
  char path[PATH_MAX];
  struct pool *pool = g_env.pool;
  struct unit *unit = pool_calloc(g_env.pool, sizeof(*unit));
  unit->env = map_create_str(map_kv_free);
  unit->path_rel = pool_strdup(pool, unit_path);
  unit->basename = path_basename((char*) unit->path_rel);

  snprintf(path, sizeof(path), "%s/%s", g_env.project.root_dir, unit_path);
  unit->dir = path_normalize(path, pool);
  path_dirname((char*) unit->dir);

  snprintf(path, sizeof(path), "%s/%s", g_env.project.cache_dir, unit_path);
  unit->cache_path = path_normalize(path, pool);

  strncpy(path, unit->cache_path, sizeof(path));
  path[sizeof(path) - 1] = '\0';
  path_dirname(path);
  unit->cache_dir = pool_strdup(pool, path);

  int rc = path_mkdirs(unit->cache_dir);
  if (rc) {
    akfatal(rc, "Failed to create directory: %s", path);
  }
  ulist_push(&g_env.units, &unit);
  return unit;
}

void unit_push(struct unit *unit) {
  akassert(unit);
  ulist_push(&g_env.units_stack, &unit);
}

struct unit* unit_pop(void) {
  akassert(g_env.units_stack.num > 0);
  struct unit *unit = *(struct unit**) ulist_get(&g_env.units_stack, g_env.units_stack.num - 1);
  ulist_pop(&g_env.units_stack);
  return unit;
}

struct unit* unit_peek(void) {
  if (g_env.units_stack.num == 0) {
    return 0;
  }
  struct unit *unit = *(struct unit**) ulist_get(&g_env.units_stack, g_env.units_stack.num - 1);
  return unit;
}

void unit_ch_cache_dir(struct unit *unit) {
  akassert(unit);
  akcheck(chdir(unit->cache_dir));
}

void unit_ch_dir(struct unit *unit) {
  akassert(unit);
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
          "    -h, --help                  Prints usage help.\n");
  fprintf(stderr,
          "\nautark [sources_dir] [options]\n"
          "  Build project in given sources dir.\n");
  fprintf(stderr,
          "    -C, --cache=<>              Project cache/build dir. Default: ./autark-cache\n");
  fprintf(stderr, "\nautark <cmd> [options]\n");
  fprintf(stderr, "  Execute a given command from checker script.\n");
  fprintf(stderr,
          "\nautark set <key> <value>\n"
          "  Sets key/value pair as output for check script.\n");
  fprintf(stderr,
          "\nautark dep <file>\n"
          "  Marks a given file as dependency for a check script.\n");

  fprintf(stderr, "\n");
  return AK_ERROR_INVALID_ARGS;
}

static int _usage(const char *err, ...) {
  va_list ap;
  va_start(ap, err);
  int rc = _usage_va(err, ap);
  va_end(ap);
  return rc;
}

static void _project_env_define(void) {
  akassert(g_env.project.root_dir);
  if (!g_env.project.cache_dir) {
    g_env.project.cache_dir = "./autark-cache";
  }

  int rc = path_mkdirs(g_env.project.cache_dir);
  if (rc) {
    akfatal(rc, "Failed to create directory: %s", g_env.project.cache_dir);
  }
  if (!path_is_dir(g_env.project.cache_dir) || !path_is_accesible_read(g_env.project.cache_dir)) {
    akfatal(AK_ERROR_FAIL, "Failed to access build CACHE directory: %s", g_env.project.cache_dir);
  }

  g_env.project.cache_dir = path_normalize(g_env.project.cache_dir, g_env.pool);
  if (!g_env.project.cache_dir) {
    akfatal(errno, "Failed to resolve project CACHE dir: %s", g_env.project.cache_dir);
  }

  const char *root_dir = g_env.project.root_dir;
  if (!path_is_dir(root_dir)) {
    akfatal(AK_ERROR_FAIL, "%s is not a directory", root_dir);
  }

  root_dir = path_normalize(g_env.project.root_dir, g_env.pool);
  if (!root_dir) {
    akfatal(errno, "Failed to resolve project ROOT dir: %s", g_env.project.root_dir);
  }
  g_env.project.root_dir = root_dir;
  akcheck(chdir(root_dir));

  setenv(AUTARK_ROOT_DIR, g_env.project.root_dir, 1);
  setenv(AUTARK_CACHE_DIR, g_env.project.cache_dir, 1);
  setenv(AUTARK_UNIT, "Autark", 1);
  if (g_env.verbose) {
    setenv(AUTARK_VERBOSE, "1", 1);
    akinfo(
      "AUTARK_ROOT_DIR:  %s\n"
      "AUTARK_CACHE_DIR: %s\n"
      "AUTARK_UNIT:      Autark\n",
      g_env.project.root_dir,
      g_env.project.cache_dir);
  }
}

static void _project_env_read(void) {
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

  struct unit *unit = unit_create(val);
  unit_push(unit);
  unit_ch_dir(unit);
}

static int _on_command_set(int argc, char* const *argv) {
  _project_env_read();
  const char *key;
  const char *val = "";
  if (optind >= argc) {
    return _usage("Missing <key> argument");
  }
  key = argv[optind++];
  if (optind < argc) {
    val = argv[optind];
  }
  if (g_env.verbose) {
    akinfo("set %s=%s\n", key, val);
  }

  struct unit *unit = unit_peek();
  unit_ch_cache_dir(unit);

  const char *env_path = pool_printf(g_env.pool, "%s.%s", unit->basename, ".env.tmp");
  FILE *f = fopen(env_path, "a+");
  if (!f) {
    akfatal(errno, "Failed to open file: %s", env_path);
  }
  fprintf(f, "%s=%s\n", key, val);
  fclose(f);
  return 0;
}

static void _on_command_dep_impl(const char *file) {
  if (g_env.verbose) {
    akinfo("dep %s", file);
  }
  unsigned ts = 0;
  if (strcmp(file, "-") != 0) {
    struct akpath_stat stat = { 0 };
    if (!path_stat(file, &stat)) {
      ts = stat.mtime / 1000U;
    }
  }

  struct unit *unit = unit_peek();
  unit_ch_cache_dir(unit);

  const char *deps_path = pool_printf(g_env.pool, "%s.%s", unit->basename, ".deps");
  FILE *f = fopen(deps_path, "a+");
  if (!f) {
    akfatal(errno, "Failed to open file: %s", deps_path);
  }
  fprintf(f, "%s:%u\n", file, ts);
  fclose(f);
}

static int _on_command_dep(int argc, char* const *argv) {
  _project_env_read();
  if (optind >= argc) {
    return _usage("Missing required dependency option");
  }
  _on_command_dep_impl(argv[optind]);
  return 0;
}

int _build(void) {
  int rc = 0;
  struct sctx *x;
  struct unit *unit = unit_peek();
  unit_ch_dir(unit);

  RCC(rc, finish, script_open(unit->basename, &x));
  script_build(x);
  script_close(&x);
  if (rc) {
    akerror(rc, "Build failed", 0);
  } else {
    akinfo("Build success");
  }
finish:
  return rc;
}

int autark_run(int argc, char **argv) {
  int rc = 0;
  struct pool *pool = pool_create_empty();
  g_env.pool = pool;
  akassert(argc > 0 && argv[0]);

  {
    char buf[PATH_MAX], rpath[PATH_MAX];
    if (!getcwd(buf, PATH_MAX)) {
      rc = errno;
      goto finish;
    }
    g_env.cwd = pool_strdup(pool, buf);
    if (utils_exec_path(buf)) {
      strncpy(buf, argv[0], sizeof(buf));
      buf[sizeof(buf) - 1] = '\0';
      realpath(buf, rpath);
      g_env.program = pool_strdup(pool, rpath);
    } else {
      g_env.program = pool_strdup(pool, buf);
    }
  }

  static const struct option long_options[] = {
    { "cache", 1, 0, 'C' },
    { "help", 0, 0, 'h' },
    { "verbose", 0, 0, 'V' },
    { "quiet", 0, 0, 'q' },
    { 0 }
  };

  for (int ch; (ch = getopt_long(argc, argv, "+C:hVq", long_options, 0)) != -1; ) {
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
      case 'h':
      default:
        _usage(0);
        goto finish;
    }
  }

  if (optind < argc) {
    const char *arg = argv[optind];
    ++optind;
    if (strcmp(arg, "set") == 0) {
      rc = _on_command_set(argc, argv);
      goto finish;
    } else if (strcmp(arg, "dep") == 0) {
      rc = _on_command_dep(argc, argv);
      goto finish;
    } else { // Root dir expected
      g_env.project.root_dir = pool_strdup(pool, arg);
    }
  } else {
    g_env.project.root_dir = g_env.cwd;
  }

  // Main build routine
  _project_env_define();

  if (strcmp(g_env.cwd, g_env.project.root_dir) != 0) {
    akcheck(chdir(g_env.project.root_dir));
    g_env.cwd = g_env.project.root_dir;
  }

  rc = _build();

finish:
  for (int i = 0; i < g_env.units.num; ++i) {
    struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
    _unit_destroy(unit);
  }
  ulist_destroy_keep(&g_env.units);
  ulist_destroy_keep(&g_env.units_stack);
  pool_destroy(pool);
  return rc;
}
