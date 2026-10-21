#include "test_utils.h"
#include "env.h"
#include "paths.h"

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>

void on_command_set(int argc, const char **argv);

static void test_set_command(void) {
  g_env.verbose = true;

  char path[PATH_MAX], path2[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(path, sizeof(path) - 1));
  setenv(AUTARK_ROOT_DIR_ENV, path, 1);

  snprintf(path2, sizeof(path2), "%s/%s", path, AUTARK_CACHE);
  IW_TEST_ASSERT_EQUAL_FATAL(path_rm_cache(path2), 0);

  setenv(AUTARK_CACHE_DIR_ENV, path2, 1);
  setenv(AUTARK_UNIT_ENV, "test3", 1);

  const char *argv[] = { "test3", "set", "foo=bar" };
  int argc = sizeof(argv) / sizeof(argv[0]);
  ++optind;

  on_command_set(argc, argv);
  unit_pop();

  struct value v = utils_file_as_buf(AUTARK_CACHE "/test3.env.tmp", 1024);
  IW_TEST_ASSERT_EQUAL_FATAL(v.error, 0);
  IW_TEST_ASSERT_STR_EQUAL(v.buf, "foo=bar\n");
  value_destroy(&v);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_set_command),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test3", cases, 0, 0));
}
