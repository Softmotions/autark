#ifndef SCRIPT_H
#define SCRIPT_H

//#include "basedefs.h"
#include "ulist.h"
#include "xstr.h"
#include "deps.h"
#include "map.h"

#include <stdbool.h>

#define NODE_TYPE_VALUE   0x01U
#define NODE_TYPE_SCRIPT  0x02U
#define NODE_TYPE_BAG     0x04U
#define NODE_TYPE_META    0x08U
#define NODE_TYPE_CHECK   0x20U
#define NODE_TYPE_SOURCES 0x40U
#define NODE_TYPE_SET     0x80U
#define NODE_TYPE_EXEC    0x100U
#define NODE_TYPE_STATIC  0x200U
#define NODE_TYPE_SHARED  0x400U
#define NODE_TYPE_INCLUDE 0x800U
#define NODE_TYPE_IF      0x1000U
#define NODE_TYPE_SUBST   0x2000U
#define NODE_TYPE_RUN     0x4000U

#define NODE_FLG_BOUND    0x01U
#define NODE_FLG_INIT     0x02U
#define NODE_FLG_SETUP    0x04U
#define NODE_FLG_UPDATED  0x08U // Node product updated as result of build
#define NODE_FLG_BUILT    0x10U // Node built
#define NODE_FLG_EXCLUDED 0x20U // Node is excluded from build
#define NODE_FLG_IN_CACHE 0x40U
#define NODE_FLG_IN_SRC   0x80U

#define NODE_FLG_IN_ANY (NODE_FLG_IN_SRC | NODE_FLG_IN_CACHE)

#define node_is_excluded(n__) (((n__)->flags & NODE_FLG_EXCLUDED) != 0)
#define node_is_included(n__) (!node_is_excluded(n__))
#define node_is_init(n__)     (((n__)->flags & NODE_FLG_INIT) != 0)
#define node_is_setup(n__)    (((n__)->flags & NODE_FLG_SETUP) != 0)
#define node_is_built(n__)    (((n__)->flags & NODE_FLG_BUILT) != 0)
#define node_is_value(n__)    ((n__)->type & NODE_TYPE_VALUE)
#define node_is_rule(n__)     !node_is_value(n__)

#define NODE_PRINT_INDENT 2

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
  struct node *root;
  struct ulist nodes;    /// ulist<struct node*>
  struct map  *products; /// Products of nodes  (product name -> node)
};

int script_open(const char *file, struct sctx **out);

void script_setup(struct sctx*);

void script_build(struct sctx*);

void script_close(struct sctx**);

void script_dump(struct sctx*, struct xstr *out);

const char* node_env_get(struct node*, const char *key);

void node_env_set(struct node*, const char *key, const char *val);

struct node* node_by_product(struct node*, const char *prod, char pathbuf[PATH_MAX]);

void node_product_add(struct node*, const char *prod, char pathbuf[PATH_MAX]);

void node_reset(struct node *n);

const char* node_value(struct node *n);

void node_init(struct node *n);

void node_setup(struct node *n);

void node_build(struct node *n);

struct node* node_find_direct_child(struct node *n, int type, const char *val);

struct node* node_find_prev_sibling(struct node *n);

struct node* node_consumes_resolve(struct node *n, void (*on_resolved)(const char *path, void *opq), void *opq);

void node_add_unit_deps(struct deps*);

struct node_resolve {
  const char *path;
  void       *user_data;
  void (*on_init)(struct node_resolve*);
  void (*on_env_value)(struct node_resolve*, const char *key, const char *val);
  void (*on_resolve)(struct node_resolve*);
  const char  *deps_path_tmp;
  const char  *env_path_tmp;
  struct pool *pool;
  unsigned     mode;
  int num_outdated; // Number of outdated dependencies
  int num_deps;     // Number of dependencies
};

#define NODE_RESOLVE_ENV_ALWAYS 0x01U

void node_resolve(struct node_resolve*);

__attribute__((noreturn))
void node_fatal(int rc, struct node *n, const char *fmt, ...);

void node_info(struct node *n, const char *fmt, ...);

int node_error(int rc, struct node *n, const char *fmt, ...);


#endif
