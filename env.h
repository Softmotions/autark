#ifndef ENV_H
#define ENV_H

#include "pool.h"

#define AUTARK_ROOT_DIR  "AUTARK_ROOT_DIR"  // Project root directory
#define AUTARK_CACHE_DIR "AUTARK_CACHE_DIR" // Project cache directory

struct env {
  const char  *cmd;
  const char  *program;
  struct pool *pool;
  int verbose;
  int quiet;
  struct {
    const char *root_dir;  // Project root source dir
    const char *cache_dir; // Project artifacts cache dirs
  } project;
};

extern struct env g_env;

#endif
