#include "test_utils.h"
#include "env.h"
#include "paths.h"

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>

void on_command_set(int argc, const char **argv);

int main(void) {
  g_env.verbose = true;

  char path[PATH_MAX], path2[PATH_MAX];
  getcwd(path, PATH_MAX - 1);
  setenv(AUTARK_ROOT_DIR, path, 1);

  snprintf(path2, sizeof(path2), "%s/%s", path, AUTARK_CACHE);
  ASSERT(assert, path_rmdir(path2) == 0);

  setenv(AUTARK_CACHE_DIR, path2, 1);
  setenv(AUTARK_UNIT, "test3", 1);

  const char *argv[] = { "test3", "set", "foo", "bar" };
  int argc = sizeof(argv) / sizeof(argv[0]);
  ++optind;

  on_command_set(argc, argv);
  unit_pop();

  struct value v = utils_file_as_buf(AUTARK_CACHE "/test3.env.tmp", 1024);
  ASSERT(assert, v.error == 0);
  ASSERT(assert, strcmp("foo=bar\n", v.buf) == 0);
  value_destroy(&v);

  return 0;
assert:
  return 1;
}
