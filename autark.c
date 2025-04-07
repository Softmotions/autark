#include "env.h"
#include "utils.h"
#include "log.h"
#include "paths.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

struct env g_env = { 0 };

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
          "    -C, --cache=<>              Project cache/build dir. Default: ./build-cache\n");
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
    g_env.project.cache_dir = "./build-cache";
  }

  akcheck(path_mkdirs(g_env.project.cache_dir));
  if (!path_is_dir(g_env.project.cache_dir) || !path_is_accesible_read(g_env.project.cache_dir)) {
    akfatal(AK_ERROR_FAIL, "Failed to access build CACHE directory: %s", g_env.project.cache_dir);
  }

  if (!path_is_absolute(g_env.project.cache_dir)) {
    g_env.project.cache_dir = path_to_real(g_env.project.cache_dir, g_env.pool);
    if (!g_env.project.cache_dir) {
      akfatal(errno, "Failed to resolve project CACHE dir: %s", g_env.project.cache_dir);
    }
  }

  const char *root_dir = g_env.project.root_dir;
  if (!path_is_dir(root_dir)) {
    akfatal(AK_ERROR_FAIL, "%s is not a directory", root_dir);
  }

  if (!path_is_absolute(root_dir)) {
    root_dir = path_to_real(g_env.project.root_dir, g_env.pool);
    if (!root_dir) {
      akfatal(errno, "Failed to resolve project ROOT dir: %s", g_env.project.root_dir);
    }
    g_env.project.root_dir = root_dir;
  }

  setenv(AUTARK_ROOT_DIR, g_env.project.root_dir, 1);
  setenv(AUTARK_CACHE_DIR, g_env.project.cache_dir, 1);
}

static int _project_env_detect(void) {
  int rc = 0;
  // TODO:

  return rc;
}

static int _on_command_set(int argc, char* const *argv) {
  int rc = 0;

  return rc;
}

static int _on_command_dep(int argc, char* const *argv) {
  int rc = 0;
  const char *file = 0;
  if (optind >= argc) {
    return _usage("Missing required dependency option");
  }
  file = argv[optind];
  // TODO


  return rc;
}

int autark_run(int argc, char* const *argv) {
  int rc = 0;
  struct pool *pool = pool_create_empty();
  akassert(argc > 0 && argv[0]);

  {
    char buf[PATH_MAX], rpath[PATH_MAX];
    if (utils_exec_path(buf)) {
      strncpy(buf, argv[0], sizeof(buf));
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
    } else if (strcmp(arg, "dep") == _on_command_dep(argc, argv)) {
      rc = _on_command_dep(argc, argv);
      goto finish;
    } else { // Root dir expected
      g_env.project.root_dir = pool_strdup(pool, arg);
    }
  } else {
    char buf[PATH_MAX];
    if (!getcwd(buf, PATH_MAX)) {
      rc = errno;
      goto finish;
    }
    g_env.project.root_dir = pool_strdup(pool, buf);
  }

  // Main build routine
  _project_env_define();

  // TODO: Run build command


finish:
  pool_destroy(pool);
  return rc;
}
