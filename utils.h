#ifndef UTILS_H
#define UTILS_H

#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "xstr.h"

#include <limits.h>
#include <string.h>
#endif

static inline bool utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}

static inline void utils_chars_replace(char *buf, char c, char r) {
  for ( ; *buf; ++buf) {
    if (*buf == c) {
      *buf = r;
    }
  }
}

static inline long utils_lround(double x) {
  if (x >= 0.0) {
    x += 0.5;
  } else {
    x -= 0.5;
  }
  if (x > (double) LONG_MAX) {
    return LONG_MAX;
  } else if (x < (double) LONG_MIN) {
    return LONG_MIN;
  }
  return (long) x;
}

static inline int utils_toupper_ascii(int c) {
  if (c >= 'a' && c <= 'z') {
    return c - ('a' - 'A');     // or: c - 32
  }
  return c;
}

static inline char* utils_strncpy(char *dst, const char *src, size_t dst_sz) {
  if (dst_sz > 1) {
    size_t len = strnlen(src, dst_sz - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
  } else if (dst_sz) {
    dst[0] = '\0';
  }
  return dst;
}

static inline char* utils_strnncpy(char *dst, const char *src, size_t src_len, size_t dst_sz) {
  if (dst_sz > 1 && src_len) {
    size_t len = MIN(src_len, dst_sz - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
  } else if (dst_sz) {
    dst[0] = '\0';
  }
  return dst;
}

static inline bool utils_startswith(const char *str, const char *prefix) {
  if (!str || !prefix) {
    return false;
  }
  size_t str_len = strlen(str);
  size_t prefix_len = strlen(prefix);
  if (prefix_len > str_len) {
    return false;
  }
  return strncmp(str, prefix, prefix_len) == 0;
}

static inline bool utils_endswith(const char *str, const char *suffix) {
  if (!str || !suffix) {
    return false;
  }
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > str_len) {
    return false;
  }
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

long int utils_strtol(const char *v, int base, int *rcp);

long long utils_strtoll(const char *v, int base, int *rcp);

struct value utils_file_as_buf(const char *path, ssize_t buflen_max);

int utils_exec_path(char buf[PATH_MAX]);

int utils_copy_file(const char *src, const char *dst);

int utils_rename_file(const char *src, const char *dst);

void utils_split_values_add(const char *v, struct xstr *xstr);


//----------------------- Vlist

struct vlist_iter {
  const char *item;
  size_t      len;
};

static inline bool is_vlist(const char *val) {
  return (val && *val == '\1');
}

static inline void vlist_iter_init(const char *vlist, struct vlist_iter *iter) {
  iter->item = vlist;
  iter->len = 0;
}

static inline bool vlist_iter_next(struct vlist_iter *iter) {
  iter->item += iter->len;
  while (*iter->item == '\1') {
    iter->item++;
  }
  if (*iter->item == '\0') {
    return false;
  }
  const char *p = iter->item;
  while (*p != '\0' && *p != '\1') {
    ++p;
  }
  iter->len = p - iter->item;
  return true;
}

#endif
