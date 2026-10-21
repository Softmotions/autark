#include "test_utils.h"
#include "script.h"
#include "env.h"

static char cwd_start[PATH_MAX];

static int suite_init(void) {
  return getcwd(cwd_start, sizeof(cwd_start)) ? 0 : 1;
}

static void test_caller_option(void) {
  chdir(cwd_start);
  test_reinit(true);

  // Simulate a command-line build option set before script_build(),
  // exactly like `./build.sh -DBUILD_TYPE=Debug` does.
  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test12/Autark", &sctx), 0);
  unit_env_set_val(unit_root(), "BUILD_TYPE", "Debug");
  script_build(sctx);

  struct unit *root = unit_root();
  const char *v = unit_env_get_raw(root, "BUILD_TYPE");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v);
  IW_TEST_ASSERT_STR_EQUAL(v, "Debug");

  // A later reference to the same option must see the caller value too.
  v = unit_env_get_raw(root, "RESULT");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v);
  IW_TEST_ASSERT_STR_EQUAL(v, "Debug");

  script_close(&sctx);
  chdir(cwd_start);
}

static void test_default_option(void) {
  chdir(cwd_start);
  test_reinit(true);

  // Without a caller-provided value, the inline default must apply.
  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test12/Autark", &sctx), 0);
  script_build(sctx);

  struct unit *root = unit_root();
  const char *v = unit_env_get_raw(root, "BUILD_TYPE");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v);
  IW_TEST_ASSERT_STR_EQUAL(v, "Release");

  v = unit_env_get_raw(root, "RESULT");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v);
  IW_TEST_ASSERT_STR_EQUAL(v, "Release");

  script_close(&sctx);
  chdir(cwd_start);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_caller_option),
  IW_TEST_CASE(test_default_option),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test12", cases, suite_init, 0));
}
