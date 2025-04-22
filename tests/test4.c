#include "test_utils.h"
#include "env.h"
#include "paths.h"

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>

void on_command_dep(int argc, const char **argv);

int main(void) {
  g_env.verbose = true;

  char path[PATH_MAX], path2[PATH_MAX];
  getcwd(path, PATH_MAX - 1);
  setenv(AUTARK_ROOT_DIR, path, 1);

  snprintf(path2, sizeof(path2), "%s/%s", path, AUTARK_CACHE);
  ASSERT(assert, path_rmdir(path2) == 0);

  setenv(AUTARK_CACHE_DIR, path2, 1);
  setenv(AUTARK_UNIT, "test4", 1);

  const char *argv[] = { "test4", "dep", "./data/test3/dep1.txt" };
  int argc = sizeof(argv) / sizeof(argv[0]);
  ++optind;

  on_command_dep(argc, argv);
  unit_pop();

  snprintf(path, sizeof(path), "%s/%s", path2, "test4.deps");
  ASSERT(assert, access(path, R_OK) == 0);


  return 0;
assert:
  return 1;
}
