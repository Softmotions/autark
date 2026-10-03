#include "test_utils.h"
#include "script.h"
#include "env.h"

int main(void) {
  test_init(true);

  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  // Simulate a command-line build option set before script_build(),
  // exactly like `./build.sh -DBUILD_TYPE=Debug` does.
  struct sctx *sctx;
  akassert(script_open("../../tests/data/test12/Autark", &sctx) == 0);
  unit_env_set_val(unit_root(), "BUILD_TYPE", "Debug");
  script_build(sctx);

  struct unit *root = unit_root();
  const char *v = unit_env_get_raw(root, "BUILD_TYPE");
  akassert(v);
  akassert(strcmp(v, "Debug") == 0);

  // A later reference to the same option must see the caller value too.
  v = unit_env_get_raw(root, "RESULT");
  akassert(v);
  akassert(strcmp(v, "Debug") == 0);

  script_close(&sctx);

  // Without a caller-provided value, the inline default must apply.
  chdir(cwd_prev);
  test_reinit(true);
  akassert(script_open("../../tests/data/test12/Autark", &sctx) == 0);
  script_build(sctx);
  root = unit_root();
  v = unit_env_get_raw(root, "BUILD_TYPE");
  akassert(v);
  akassert(strcmp(v, "Release") == 0);
  v = unit_env_get_raw(root, "RESULT");
  akassert(v);
  akassert(strcmp(v, "Release") == 0);
  script_close(&sctx);

  chdir(cwd_prev);
  return 0;
}
