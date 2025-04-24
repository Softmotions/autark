#ifndef ENV_H
#define ENV_H

#include "pool.h"
#include "map.h"
#include "ulist.h"

#include <stdbool.h>

#define AUTARK_CACHE  "autark-cache"
#define AUTARK_SCRIPT "Autark"

#define AUTARK_ROOT_DIR  "AUTARK_ROOT_DIR"  // Project root directory
#define AUTARK_CACHE_DIR "AUTARK_CACHE_DIR" // Project cache directory
#define AUTARK_UNIT      "AUTARK_UNIT"      // Path relative to AUTARK_ROOT_DIR of build process unit executed
                                            // currently.
#define AUTARK_VERBOSE "AUTARK_VERBOSE"     // Autark verbose env key

#define UNIT_FLG_SRC_CWD 0x01U // Set project source dir as unit CWD
#define UNIT_FLG_NO_CWD  0x02U // Do not change CWD for unit

/// Current execution unit.
struct unit {
  struct map  *env;         // Environment associated with unit.
  const char  *rel_path;    // Path to unit relative to the project root and project cache.
  const char  *basename;    // Basename of unit path.
  const char  *dir;         // Absolute path to unit dir.
  const char  *source_path; // Unit source path.
  const char  *cache_path;  // Absolute path to the unit in cache dir.
  const char  *cache_dir;   // Absolute path to the cache directory where unit file is located.
  struct pool *pool;        // Pool used to allocate unit
  unsigned     flags;       // Unit flags
  void *impl;
};

/// Global env
struct env {
  const char  *cwd;
  const char  *program;
  struct pool *pool;
  int verbose;
  int quiet;
  struct {
    const char *root_dir;  // Project root source dir.
    const char *cache_dir; // Project artifacts cache dir.
    bool clean;            // Clean project cache before build
  } project;
  struct {
    const char *extra_env_paths; // Extra PATH environment for any program spawn
  } spawn;
  struct ulist units_stack; // Stack of nested units
  struct ulist units;       // All created units.
};

extern struct env g_env;

struct unit* unit_create(const char *unit_path, unsigned flags, struct pool *pool);

void unit_push(struct unit*);

struct unit* unit_pop(void);

struct unit* unit_peek(void);

void unit_ch_dir(struct unit*);

void unit_ch_cache_dir(struct unit*);

void unit_ch_src_dir(struct unit*);

void unit_env_set(struct unit*, const char *key, const char *val);

const char* unit_env_get(struct unit*, const char *key);

void unit_env_remove(struct unit*, const char *key);

#endif
