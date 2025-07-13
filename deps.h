#ifndef DEPS_H
#define DEPS_H

#ifndef _AMALGAMATE_
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#endif

#define DEPS_TYPE_FILE          102 // f
#define DEPS_TYPE_FILE_OUTDATED 111 // x
#define DEPS_TYPE_NODE_VALUE    118 // v
#define DEPS_TYPE_ENV           101 // e
#define DEPS_TYPE_ALIAS         97  // a
#define DEPS_TYPE_OUTDATED      120 // o

#define DEPS_OPEN_TRUNCATE 0x01U
#define DEPS_OPEN_READONLY 0x02U

#define DEPS_BUF_SZ 262144

struct deps {
  char    type;
  char    flags;
  int     num_registered;   /// Number of deps registerd in the current session
  int64_t serial;
  const char *resource;
  const char *alias;
  FILE       *file;
  char buf[DEPS_BUF_SZ];
};

int deps_open(const char *path, int omode, struct deps *init);

bool deps_cur_next(struct deps*);

bool deps_cur_is_outdated(struct deps*);

int deps_add(struct deps*, char type, char flags, const char *resource, int64_t serial);

int deps_add_alias(struct deps*, char flags, const char *resource, const char *alias);

int deps_add_env(struct deps *d, char flags, const char *key, const char *value);

int deps_add_sys_env(struct deps *d, char flags, const char *key, const char *value);

void deps_close(struct deps*);

void deps_prune_all(const char *path);

#endif
