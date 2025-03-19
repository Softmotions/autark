#ifndef ENV_H
#define ENV_H

#include "pool.h"

struct env {
  const char  *cmd;
  struct pool *pool;
};

extern struct env g_env;

#endif
