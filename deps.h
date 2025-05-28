#ifndef DEPS_H
#define DEPS_H

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

#define DEPS_TYPE_FILE       102
#define DEPS_TYPE_NODE_VALUE 118
#define DEPS_TYPE_OUTDATED   111

#define DEPS_OPEN_TRUNCATE 0x01U
#define DEPS_OPEN_READONLY 0x02U

#define DEPS_BUF_SZ 4096

struct deps {
  char    type;
  char    flags;
  int     num_registered;   /// Number of deps registerd in the current session
  int64_t serial;
  const char *resource;
  FILE       *file;
  char buf[DEPS_BUF_SZ];
};

int deps_open(const char *path, int omode, struct deps *init);

bool deps_cur_next(struct deps*);

bool deps_cur_is_outdated(struct deps*);

int deps_add(struct deps*, char type, char flags, const char *file);

void deps_close(struct deps*);

void deps_prune_all(const char *path);

#endif
