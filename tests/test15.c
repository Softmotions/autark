// Tests the `install` rule permissions policy.
//
// The test project creates files/directories with restrictive modes (umask 077)
// and installs them twice: with the default policy and with the `exact` mode.
// The default policy must make files readable by everyone and executables
// executable by everyone (write and special bits are preserved), while `exact`
// must preserve the source mode as is.

#include "test_utils.h"
#include "script.h"

#include <sys/stat.h>

static char cwd_start[PATH_MAX];

static int suite_init(void) {
  return getcwd(cwd_start, sizeof(cwd_start)) ? 0 : 1;
}

static mode_t file_mode(const char *path) {
  struct stat st;
  IW_TEST_ASSERT_EQUAL_FATAL(stat(path, &st), 0);
  return st.st_mode & 07777;
}

static void test_install_permissions(void) {
  chdir(cwd_start);
  test_reinit(true);

  struct pool *pool = pool_create_empty();
  const char *install_dir = path_normalize_cwd_pool("../../tests/data/test15/autark-cache", 0, pool);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(install_dir);
  path_rm_dir_recursive(install_dir);
  IW_TEST_ASSERT_EQUAL_FATAL(path_mkdirs(install_dir), 0);

  g_env.install.enabled = true;
  g_env.install.prefix_dir = install_dir;
  g_env.install.data_dir = "share";

  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test15/Autark", &sctx), 0);
  script_build(sctx);
  script_close(&sctx);

  // Default mode: readable by everyone, executable by everyone if executable,
  // other source permission bits and write bits are preserved.
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/plain.txt", install_dir)), 0644);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/exec.bin", install_dir)), 0755);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/write.bin", install_dir)), 0664);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/data", install_dir)), 0755);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/data/sub", install_dir)), 0755);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/data/sub/nested.txt", install_dir)), 0644);

  // Exact mode: source permissions are preserved as is.
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/exact/plain.txt", install_dir)), 0600);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/exact/exec.bin", install_dir)), 0700);
  IW_TEST_ASSERT_EQUAL(file_mode(pool_printf(pool, "%s/share/exact/write.bin", install_dir)), 0660);

  chdir(cwd_start);
  pool_destroy(pool);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_install_permissions),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test15", cases, suite_init, 0));
}
