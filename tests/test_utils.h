#pragma once

#include "basedefs.h"
#include "utils.h"
#include "xstr.h"
#include "script.h"
#include "autark.h"
#include "env.h"
#include "paths.h"
#include "log.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define ASSERT(label__, expr__)                                       \
        if (!(expr__)) {                                              \
          fputs(__FILE__ ":" Q(__LINE__) " " Q(expr__) "\n", stderr); \
          goto label__;                                               \
        }

int test_script_parse(const char *script_path, struct sctx **out);

static inline int cmp_file_with_buf(const char *path, const char *buf, size_t sz) {
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
  int ret = memcmp(val.buf, buf, sz);
  value_destroy(&val);
  return ret;
}

static inline int cmp_file_with_xstr(const char *path, struct xstr *xstr) {
  return cmp_file_with_buf(path, xstr_ptr(xstr), xstr_size(xstr));
}

static inline void test_init(bool cleanup) {
  g_env.verbose = true;
  g_env.project.cleanup = cleanup;
  autark_init();
  g_env.spawn.extra_env_paths = path_normalize_pool("./..", g_env.pool);
}

static inline void test_reinit(bool cleanup) {
  autark_dispose();
  test_init(cleanup);
}

