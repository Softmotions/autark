#ifndef DEPS_H
#define DEPS_H

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

#define DEPS_TYPE_FILE 102
#define DEPS_TYPE_OUTDATED 111
#define DEPS_BUF_SZ    4096

struct deps {
  int       type;
  long long serial;
  const char *resource;
  FILE       *file;
  char buf[DEPS_BUF_SZ];
};

int deps_open(const char *path, bool truncate, struct deps *init);

bool deps_next(struct deps*);

bool deps_is_outdated(struct deps*);

int deps_register(struct deps*, int type, const char *file);

void deps_close(struct deps*);

void deps_remove(const char *path);

#endif
