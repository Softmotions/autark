#ifndef SCRIPT_H
#define SCRIPT_H

//#include "basedefs.h"
#include "pool.h"
#include "ulist.h"
#include "xstr.h"
#include "map.h"

#define NODE_TYPE_VALUE    0x01U
#define NODE_TYPE_SCRIPT   0x02U
#define NODE_TYPE_BAG      0x04U
#define NODE_TYPE_META     0x08U
#define NODE_TYPE_CONSUMES 0x10U
#define NODE_TYPE_CHECK    0x20U
#define NODE_TYPE_SOURCES  0x40U
#define NODE_TYPE_FLAGS    0x80U
#define NODE_TYPE_EXEC     0x100U
#define NODE_TYPE_STATIC   0x200U
#define NODE_TYPE_SHARED   0x400U
#define NODE_TYPE_INCLUDE  0x800U
#define NODE_TYPE_IF       0x1000U
#define NODE_TYPE_SUBST    0x2000U

#define NODE_FLG_BOUND    0x01U
#define NODE_FLG_RESOLVED 0x02U // Node built
#define NODE_FLG_UPDATED  0x04U // Node product updated as result of build
#define NODE_FLG_BUILT    0x08U // Node built
#define NODE_FLG_EXCLUDED 0x10U // Node is excluded from build

#define node_is_value(n__) ((n__)->type & NODE_TYPE_VALUE)
#define node_is_rule(n__)  !node_is_value(n__)

#define NODE_PRINT_INDENT 2

struct node {
  unsigned type;
  unsigned flags;
  unsigned index;     // Own index in env::nodes
  unsigned pos;       // Node position

  const char *value;  ///< Key or value

  struct node *child;
  struct node *next;
  struct node *parent;

  struct xenv *env;
  struct map  *props;

  int  (*resolve)(struct node*);
  int  (*build)(struct node*);
  void (*dispose)(struct node*);

  void *impl;
};

struct xenv {
  struct pool *pool;
  struct node *root;
  struct ulist nodes;    // ulist<struct node*>
  struct ulist contexts; // ulist<struct node*> nodes with type == NODE_TYPE_SCRIPT
};

int script_open(const char *file, struct xenv **out);

int script_resolve(struct xenv*);

int script_build(struct xenv*);

void script_close(struct xenv**);

void script_dump(struct xenv*, struct xstr *out);

int node_build(struct node *n);

const char* node_prop_get(struct node*, const char *key);

void node_prop_set(struct node*, const char *key, const char *val);

void node_fatal(int rc, struct node *n, const char *fmt, ...);

int node_error(int rc, struct node *n, const char *fmt, ...);

#endif
