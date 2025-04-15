#ifndef UTILS_H
#define UTILS_H

#include "basedefs.h"
#include <limits.h>

static inline int utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}

struct value utils_file_as_buf(const char *path, ssize_t buflen_max);

int utils_exec_path(char buf[PATH_MAX]);

int utils_copy_file(const char *src, const char *dst);

int utils_rename_file(const char *src, const char *dst);

#endif
