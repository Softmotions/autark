#ifndef UTILS_H
#define UTILS_H

static inline int utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}

#endif
