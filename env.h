#ifndef ENV_H
#define ENV_H

#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "pool.h"
#include "map.h"
#include "ulist.h"

#include <stdbool.h>
#endif

#define AUTARK_CACHE           "autark-cache"
#define AUTARK_SCRIPT          "Autark"
#define INSTALL_PREFIX_DEFAULT "/usr/local"

#define AUTARK_ROOT_DIR  "AUTARK_ROOT_DIR"  // Project root directory
#define AUTARK_CACHE_DIR "AUTARK_CACHE_DIR" // Project cache directory
#define AUTARK_UNIT      "AUTARK_UNIT"      // Path relative to AUTARK_ROOT_DIR of build process unit executed
                                            // currently.
#define AUTARK_VERBOSE "AUTARK_VERBOSE"     // Autark verbose env key

#define UNIT_FLG_ROOT    0x01U // Project root unit
#define UNIT_FLG_SRC_CWD 0x02U // Set project source dir as unit CWD
#define UNIT_FLG_NO_CWD  0x04U // Do not change CWD for unit

struct unit_env_item {
  const char  *val;
  struct node *n;
};

/// Current execution unit.
struct unit {
  struct map  *env;         // Environment associated with unit. struct unit_env_item.
  const char  *rel_path;    // Path to unit relative to the project root and project cache.
  const char  *basename;    // Basename of unit path.
  const char  *dir;         // Absolute path to unit dir.
  const char  *source_path; // Absolute unit source path.
  const char  *cache_path;  // Absolute path to the unit in cache dir.
  const char  *cache_dir;   // Absolute path to the cache directory where unit file is located.
  struct pool *pool;        // Pool used to allocate unit
  unsigned     flags;       // Unit flags
  struct node *n;           // Node this unit associated. May be zero.
  void *impl;               // Arbirary implementation data
};

/// Unit context in units stack.
struct unit_ctx {
  struct unit *unit;
  unsigned     flags;
};

/// Global env
struct env {
  const char  *cwd;
  struct pool *pool;
  int verbose;
  struct {
    const char *root_dir;           // Project root source dir.
    const char *cache_dir;          // Project artifacts cache dir.
    bool cleanup;                   // Clean project cache before build
    bool prepared;                  // Autark build prepared
    bool options;                   // Ask option values
  } project;
  struct {
    const char *prefix_dir;  // Install prefix dir.
    const char *bin_dir;     // Path to the bin dir relative to prefix.
    const char *lib_dir;     // Path to lib dir relative to prefix.
    const char *include_dir; // Path to include headers dir relative to prefix.
    const char *pkgconf_dir; // Path to pkgconfig dir.
  } install;
  struct {
    const char *extra_env_paths; // Extra PATH environment for any program spawn
  } spawn;
  struct map  *map_path_to_unit; // Path to unit mapping
  struct ulist stack_units;      // Stack of nested unit contexts (struct unit_ctx)
  struct ulist units;            // All created units.
  struct {
    struct xstr *log;
  } check;
};

extern struct env g_env;

void on_unit_pool_destroy(struct pool*);

struct unit* unit_create(const char *unit_path, unsigned flags, struct pool *pool);

struct unit* unit_for_path(const char *path);

void unit_push(struct unit*, struct node*);

struct unit* unit_pop(void);

struct unit* unit_peek(void);

struct unit_ctx unit_peek_ctx(void);

struct  unit* unit_root(void);

void unit_ch_dir(struct unit_ctx*, char *prevcwd);

void unit_ch_cache_dir(struct unit*, char *prevcwd);

void unit_ch_src_dir(struct unit*, char *prevcwd);

void unit_env_set_val(struct unit*, const char *key, const char *val);

void unit_env_set_node(struct unit*, const char *key, struct node *n);

const char* unit_env_get(struct unit*, const char *key);

const char* unit_env_get_raw(struct unit *u, const char *key);

void unit_env_remove(struct unit*, const char *key);

#endif
