#pragma once

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "utils.h"
#include "xstr.h"
#include "script.h"

#define _QSTR(x__) #x__
#define _Q(x__)    _QSTR(x__)

#define ASSERT(label__, expr__)                                         \
        if (!(expr__)) {                                                \
          fputs(__FILE__ ":" _Q(__LINE__) " " _Q(expr__) "\n", stderr); \
          goto label__;                                                 \
        }

int test_script_parse(const char *script_path, struct env **out);

static inline int cmp_file_with_xstr(const char *path, struct xstr *xstr) {
  size_t sz = xstr_size(xstr);
  struct value val = utils_file_as_buf(path, sz + 1);
  if (val.error) {
    return -1;
  }
  if (val.len > sz) {
    value_destroy(&val);
    return 1;
  } else if (val.len < sz) {
    value_destroy(&val);
    return -1;
  }
  int ret = memcmp(val.buf, xstr_ptr(xstr), sz);
  value_destroy(&val);
  return ret;
}
