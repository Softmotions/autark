#ifndef UTILS_H
#define UTILS_H

#include "basedefs.h"
#include <sys/types.h>

static inline int utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}

struct value utils_file_as_buf(const char *path, ssize_t buflen_max);


#endif
