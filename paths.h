#ifndef PATHS_H
#define PATHS_H

#include "pool.h"
#include <stdint.h>

enum akpath_access {
  AKPATH_READ  = 0x01,
  AKPATH_WRITE = 0x02,
  AKPATH_EXEC  = 0x04,
};

int path_access(const char *path, enum akpath_access access);

enum akpath_ftype {
  AKPATH_NOT_EXISTS,
  AKPATH_TYPE_FILE,
  AKPATH_TYPE_DIR,
  AKPATH_LINK,
  AKPATH_OTHER,
};

struct akpath_stat {
  uint64_t size;
  uint64_t atime;
  uint64_t ctime;
  uint64_t mtime;
  enum akpath_ftype ftype;
};

int path_is_absolute(const char *path);

const char* path_to_real(const char *path, struct pool *pool);

int path_mkdirs(const char *path);

int path_stat(const char *path, struct akpath_stat *stat);

int path_statfd(int fd, struct akpath_stat *stat);

int path_is_accesible(const char *path, enum akpath_access a);

int path_is_accesible_read(const char *path);

int path_is_accesible_write(const char *path);

int path_is_accesible_exec(const char *path);

int path_is_dir(const char *path);

int path_is_file(const char *path);

int path_is_exist(const char *path);

#endif
