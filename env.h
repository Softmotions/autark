#ifndef ENV_H
#define ENV_H

#include "pool.h"

#define AUTARK_ROOT_DIR  "AUTARK_ROOT_DIR"  // Project root directory
#define AUTARK_CACHE_DIR "AUTARK_CACHE_DIR" // Project cache directory
#define AUTARK_UNIT      "AUTARK_UNIT"      // Path relative to AUTARK_ROOT_DIR of build process unit executed
                                            // currently.
#define AUTARK_VERBOSE "AUTARK_VERBOSE"     // Autark verbose env key

struct env {
  const char  *cwd;
  const char  *program;
  struct pool *pool;
  int verbose;
  int quiet;
  struct {
    const char *root_dir;   // Project root source dir.
    const char *cache_dir;  // Project artifacts cache dir.
  } project;
  struct {
    const char *path;       // Relative path of current unit.
    const char *cache_path; // absolute path to unit in cache dir.
    const char *cache_dir;   // Cache directory where unit file is located.
  } unit;
};

extern struct env g_env;

#endif
