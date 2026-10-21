#include "test_utils.h"
#include "script.h"
#include "pool.h"

static char cwd_start[PATH_MAX];

static int suite_init(void) {
  return getcwd(cwd_start, sizeof(cwd_start)) ? 0 : 1;
}

static void test_build_and_rebuild(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  chdir(cwd_start);
  test_init(true);
  struct xstr *xlog = g_env.check.log = xstr_create_empty();
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx = 0;
  int rc = script_open("../../tests/data/test7/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  script_build(sctx);
  script_close(&sctx);

  struct akpath_stat st[2];
  IW_TEST_ASSERT_EQUAL(access("autark-cache/hello", X_OK), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/hello", &st[0]), 0);

  struct value val = utils_file_as_buf("autark-cache/libhello/hello.h", 0x100000);
  IW_TEST_ASSERT_EQUAL_FATAL(val.error, 0);
  IW_TEST_ASSERT_EQUAL(cmp_file_with_buf("hello.h.dump", val.buf, val.len), 0);
  value_destroy(&val);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:14  env.sh: resolved outdated outdated=0\n"
    "libhello/Autark:2   libhello.sh: resolved outdated outdated=0\n"
    "libhello/Autark:29  configure: resolved outdated outdated=0\n"
    "Autark:46     cc: resolved outdated outdated=0\n"
    "Autark:46     cc: build src=../main.c obj=main.o\n"
    "Autark:46     cc: build src=../aux.c obj=aux.o\n"
    "libhello/Autark:10     cc: resolved outdated outdated=0\n"
    "libhello/Autark:10     cc: build src=../../libhello/hello.c obj=hello.o\n"
    "libhello/Autark:19    run: resolved outdated outdated=0\n"
    "libhello/Autark:19    run: ar\n"
    "Autark:58    run: resolved outdated outdated=0\n"
    "Autark:58    run: cc\n"
    );

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("../../tests/data/test7/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);
  IW_TEST_ASSERT_EQUAL(xstr_size(xlog), 0);

  chdir(cwd_start);
  xstr_destroy(xlog);
}

static void test_install(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  chdir(cwd_start);
  test_reinit(true);

  struct pool *pool = pool_create_empty();
  const char *install_dir = path_normalize_cwd_pool("../../tests/data/test7/autark-cache", 0, pool);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(install_dir);
  g_env.install.enabled = true;
  g_env.install.prefix_dir = install_dir;
  g_env.install.lib_dir = "lib";

  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test7/Autark", &sctx), 0);
  script_build(sctx);
  IW_TEST_ASSERT_TRUE(path_is_exist(pool_printf(pool, "%s/lib/libhello.a", g_env.project.cache_dir)));
  IW_TEST_ASSERT_TRUE(path_is_exist(pool_printf(pool, "%s/include/libhello/hello.h", g_env.project.cache_dir)));
  script_close(&sctx);

  chdir(cwd_start);
  pool_destroy(pool);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_build_and_rebuild),
  IW_TEST_CASE(test_install),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test7", cases, suite_init, 0));
}
