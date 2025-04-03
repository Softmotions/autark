#ifndef SCRIPT_H
#define SCRIPT_H

//#include "basedefs.h"
#include "pool.h"
#include "ulist.h"
#include "xstr.h"


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

#define NODE_FLG_SETTLED  0x01U
#define NODE_FLG_RESOLVED 0x02U

#define node_is_value(n__) ((n__)->type & NODE_TYPE_VALUE)
#define node_is_rule(n__)  !node_is_value(n__)

#define NODE_PRINT_INDENT 2

struct node {
  unsigned type;
  unsigned flags;
  unsigned index;  ///< Own index in project::nodes

  const char *value;  ///< Key or value
  const char *path;

  struct node   *child;
  struct node   *next;
  struct node   *parent;
  struct script *project;

  int  (*settle)(struct node*);
  int  (*build)(struct node*, int *updated);
  void (*dispose)(struct node*);

  void *impl;
};

struct script {
  struct pool *pool;
  struct node *root;
  struct ulist nodes; // ulist<struct node*>
};

int script_open(const char *script_path, struct script **out);

int script_build(struct script*);

void script_close(struct script**);

void script_print(struct script*, struct xstr *out);

#endif
