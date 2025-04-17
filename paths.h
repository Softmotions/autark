#ifndef PATHS_H
#define PATHS_H

#include "pool.h"
#include "basedefs.h"

#include <limits.h>

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

bool path_is_absolute(const char *path);

/// Uses stdlib `realpath` implementation.
/// NOTE: Path argument must exists on file system.
/// Foo other cases see: path_normalize()
const char* path_real(const char *path, struct pool *pool);

/// Normalize a path buffer to an absolute path without resolving symlinks.
/// It operates purely on the path string, and works for non-existing paths.
const char* path_normalize(const char *path, struct pool *pool);

int path_mkdirs(const char *path);

int path_rmdir(const char *path);

int path_stat(const char *path, struct akpath_stat *stat);

int path_statfd(int fd, struct akpath_stat *stat);

bool path_is_accesible(const char *path, enum akpath_access a);

bool path_is_accesible_read(const char *path);

bool path_is_accesible_write(const char *path);

bool path_is_accesible_exec(const char *path);

bool path_is_dir(const char *path);

bool path_is_file(const char *path);

bool path_is_exist(const char *path);

AK_ALLOC char* path_relativize(const char *from, const char *to);

// Modifies its argument
char* path_dirname(char *path);

// Modifies its argument
char* path_basename(char *path);


#endif
