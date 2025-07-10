#ifndef SCRIPT_H
#define SCRIPT_H

#ifndef _AMALGAMATE_
#include "ulist.h"
#include "xstr.h"
#include "deps.h"
#include "map.h"

#include <stdbool.h>
#endif

// value types
#define NODE_TYPE_VALUE    0x01U
#define NODE_TYPE_SUBST    0x02U
#define NODE_TYPE_SET      0x04U
#define NODE_TYPE_JOIN     0x08U
#define NODE_TYPE_BASENAME 0x10U
#define NODE_TYPE_DIR      0x20U
#define NODE_TYPE_FIND     0x40U
// eof value types
#define NODE_TYPE_SCRIPT     0x100U
#define NODE_TYPE_BAG        0x200U
#define NODE_TYPE_META       0x400U
#define NODE_TYPE_CHECK      0x800U
#define NODE_TYPE_INCLUDE    0x1000U
#define NODE_TYPE_IF         0x2000U
#define NODE_TYPE_RUN        0x4000U
#define NODE_TYPE_CC         0x8000U
#define NODE_TYPE_CONFIGURE  0x10000U
#define NODE_TYPE_FOREACH    0x20000U
#define NODE_TYPE_IN_SOURCES 0x40000U
#define NODE_TYPE_OPTION     0x80000U
#define NODE_TYPE_ERROR      0x100000U
#define NODE_TYPE_ECHO       0x200000U
#define NODE_TYPE_INSTALL    0x400000U

#define NODE_FLG_BOUND    0x01U
#define NODE_FLG_INIT     0x02U
#define NODE_FLG_SETUP    0x04U
#define NODE_FLG_UPDATED  0x08U // Node product updated as result of build
#define NODE_FLG_BUILT    0x10U // Node built
#define NODE_FLG_IN_CACHE 0x40U
#define NODE_FLG_IN_SRC   0x80U
#define NODE_FLG_NO_CWD   0x100U

#define NODE_FLG_IN_ANY (NODE_FLG_IN_SRC | NODE_FLG_IN_CACHE | NODE_FLG_NO_CWD)

#define node_is_init(n__)         (((n__)->flags & NODE_FLG_INIT) != 0)
#define node_is_setup(n__)        (((n__)->flags & NODE_FLG_SETUP) != 0)
#define node_is_built(n__)        (((n__)->flags & NODE_FLG_BUILT) != 0)
#define node_is_value(n__)        ((n__)->type == NODE_TYPE_VALUE)
#define node_is_can_be_value(n__) ((n__)->type >= NODE_TYPE_VALUE && (n__)->type <= NODE_TYPE_FIND)

#define node_is_rule(n__) !node_is_value(n__)

#define NODE_PRINT_INDENT 2

struct node_foreach {
  char    *name;
  char    *value;
  char    *items;
  unsigned access_cnt;
};

struct node {
  unsigned type;
  unsigned flags;
  unsigned flags_owner; /// Extra flaghs set by owner node
  unsigned index;       /// Own index in env::nodes
  unsigned lnum;        /// Node line number

  const char *name;   /// Internal node name
  const char *vfile;  /// Node virtual file
  const char *value;  /// Key or value

  struct node *child;
  struct node *next;
  struct node *parent;

  // Recursive set
  struct {
    struct node *n;
    bool active;
  } recur_next;

  struct sctx *ctx;
  struct unit *unit;

  const char* (*value_get)(struct node*);
  void (*init)(struct node*);
  void (*setup)(struct node*);
  void (*build)(struct node*);
  void (*dispose)(struct node*);

  void *impl;
};

struct sctx {
  struct node *root;     /// Project root script node (Autark)
  struct ulist nodes;    /// ulist<struct node*>
  struct map  *products; /// Products of nodes  (product name -> node)
};

int script_open(const char *file, struct sctx **out);

int script_include(struct node *parent, const char *file, struct node **out);

void script_build(struct sctx*);

void script_close(struct sctx**);

void script_dump(struct sctx*, struct xstr *out);

const char* node_env_get(struct node*, const char *key);

void node_env_set(struct node*, const char *key, const char *val);

void node_env_set_node(struct node*, const char *key);

struct node* node_by_product(struct node*, const char *prod, char pathbuf[PATH_MAX]);

struct node* node_by_product_raw(struct node*, const char *prod);

void node_product_add(struct node*, const char *prod, char pathbuf[PATH_MAX]);

void node_product_add_raw(struct node*, const char *prod);

void node_reset(struct node *n);

const char* node_value(struct node *n);

void node_module_setup(struct node *n, unsigned flags);

void node_init(struct node *n);

void node_setup(struct node *n);

void node_build(struct node *n);

struct node* node_find_direct_child(struct node *n, int type, const char *val);

struct node* node_find_prev_sibling(struct node *n);

struct  node* node_find_parent_of_type(struct node *n, int type);

struct node_foreach* node_find_parent_foreach(struct node *n);

bool node_is_value_may_be_dep_saved(struct node *n);

struct node* node_consumes_resolve(
  struct node *n,
  struct node *npaths,
  struct ulist *paths,
  void (*on_resolved)(const char *path, void *opq),
  void *opq);

void node_add_unit_deps(struct deps*);

struct node_resolve {
  struct node *n;
  const char  *path;
  void *user_data;
  void  (*on_init)(struct node_resolve*);
  void  (*on_env_value)(struct node_resolve*, const char *key, const char *val);
  void  (*on_resolve)(struct node_resolve*);
  const char  *deps_path_tmp;
  const char  *env_path_tmp;
  struct ulist resolve_outdated; // struct resolve_outdated
  struct ulist node_val_deps;    // struct node*
  struct pool *pool;
  unsigned     mode;
  int  num_deps;    // Number of dependencies
  bool force_outdated;
};

struct resolve_outdated {
  char  type;
  char  flags;
  char *path;
};

#define NODE_RESOLVE_ENV_ALWAYS 0x01U

void node_resolve(struct node_resolve*);

__attribute__((noreturn))
void node_fatal(int rc, struct node *n, const char *fmt, ...);

void node_info(struct node *n, const char *fmt, ...);

void node_warn(struct node *n, const char *fmt, ...);

int node_error(int rc, struct node *n, const char *fmt, ...);


#endif
