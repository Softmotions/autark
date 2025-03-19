#ifndef SCRIPT_H
#define SCRIPT_H

#include "basedefs.h"
#include "pool.h"

#define NODE_TYPE_ROOT     0x01U
#define NODE_TYPE_VALUE    0x02U
#define NODE_TYPE_META     0x04U
#define NODE_TYPE_CONSUMES 0x08U
#define NODE_TYPE_CHECK    0x10U
#define NODE_TYPE_SOURCES  0x20U
#define NODE_TYPE_FLAGS    0x40U
#define NODE_TYPE_EXEC     0x80U
#define NODE_TYPE_STATIC   0x100U
#define NODE_TYPE_SHARED   0x200U
#define NODE_TYPE_INCLUDE  0x400U

#define node_is_value(n__) ((n__)->type & NODE_TYPE_VALUE)
#define node_is_rule(n__)  !node_is_value(n__)

struct node {
  unsigned    type;
  unsigned    flags;
  const char *value;  ///< Key or value

  struct node *child;
  struct node *next;
  struct node *parent;
  struct pool *pool;

  void *impl;
  int   (*attach)(struct node*);
  int   (*init)(struct node*);
  int   (*run)(struct node*);
};

int script_from_buf(const struct value *buf, struct node **out);

int script_from_file(const char *path, struct node **out);

#endif
