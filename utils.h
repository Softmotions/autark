#ifndef UTILS_H
#define UTILS_H

#include "basedefs.h"
#include "xstr.h"
#include "pool.h"
#include "ulist.h"

#include <limits.h>

static inline int utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}

static inline void utils_chars_replace(char *buf, char c, char r) {
  for ( ; *buf; ++buf) {
    if (*buf == c) {
      *buf = r;
    }
  }
}

long int utils_strtol(const char *v, int base, int *rcp);

long long utils_strtoll(const char *v, int base, int *rcp);

struct value utils_file_as_buf(const char *path, ssize_t buflen_max);

int utils_exec_path(char buf[PATH_MAX]);

int utils_copy_file(const char *src, const char *dst);

int utils_rename_file(const char *src, const char *dst);

static inline bool utils_is_list_value(const char *val) {
  return (val && *val == '\1');
}

void utils_split_values_add(const char *v, struct xstr *xstr);

char** utils_list_value_to_clist(const char *val, struct pool*);

AK_ALLOC char* utils_list_value_from_ulist(const struct ulist *list);

#endif
