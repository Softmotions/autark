#include "test_utils.h"
#include "script.h"

#define _PCACHE "../../tests/data/test11/autark-cache"

static void test_install_source_deps(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct pool *pool = pool_create_empty();
  test_init(true);

  const char *cache_dir = path_normalize_cwd_pool(_PCACHE, 0, pool);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(cache_dir);
  const char *install_dir = pool_printf(pool, "%s", cache_dir);
  const char *overlay_dir = pool_printf(pool, "%s" AUTARK_CACHE_OVERLAY_DIR, cache_dir);

  g_env.install.prefix_dir = install_dir;
  g_env.project.cache_overlay_dir = overlay_dir;
  g_env.install.enabled = true;
  g_env.install.flags |= INSTALL_FLG_SRC_WITH_DEPS;

  struct sctx *sctx = 0;
  int rc = script_open("../../tests/data/test11/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  script_build(sctx);
  script_close(&sctx);
  chdir(cwd_prev);

  IW_TEST_ASSERT_TRUE(path_is_exist(_PCACHE "/include/libext/libext.h"));
  IW_TEST_ASSERT_TRUE(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/Autark"));
  IW_TEST_ASSERT_TRUE(
    path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/extern_libext/Autark"));
  IW_TEST_ASSERT_TRUE(
    path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/extern_libext/.autark/env.sh"));
  IW_TEST_ASSERT_TRUE(
    path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/.overlay/" AUTARK_FETCHED_REG_DIST));

  fprintf(stderr, "\n\n");
  test_reinit(false);
  g_env.install.enabled = false;
  g_env.install.flags = 0;

  rc = script_open(_PCACHE "/shared/test11/test11-source-1.0.0/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  chdir(cwd_prev);
  IW_TEST_ASSERT_TRUE(path_is_exist(_PCACHE "/shared/test11/test11-source-1.0.0/autark-cache/main"));

  fprintf(stderr, "\n\n");
  test_reinit(false);

  rc = script_open(_PCACHE "/shared/test11/test11-source-1.0.0/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  chdir(cwd_prev);
  pool_destroy(pool);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_install_source_deps),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test11", cases, 0, 0));
}
