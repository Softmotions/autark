#include "test_utils.h"
#include "script.h"

static void test_incremental_rebuild(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  test_init(true);
  struct xstr *xlog = g_env.check.log = xstr_create_empty();
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test8/Autark", &sctx), 0);
  script_build(sctx);
  script_close(&sctx);
  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:14     cc: resolved outdated outdated=0\n"
    "Autark:14     cc: build src=../test8_1.c obj=test8_1.o\n"
    "Autark:14     cc: build src=../test8_2.c obj=test8_2.o\n"
    "Autark:14     cc: build src=../test8_3.c obj=test8_3.o\n"
    "Autark:14     cc: build src=../test8_4.c obj=test8_4.o\n"
    "Autark:23    run: resolved outdated outdated=0\n"
    "Autark:23    run: cc\n"
    "Autark:23    run: /bin/sh\n"
    "Autark:23    run: cc\n"
    "Autark:23    run: /bin/sh\n"
    "Autark:23    run: cc\n"
    "Autark:23    run: /bin/sh\n"
    "Autark:23    run: cc\n"
    "Autark:23    run: /bin/sh\n"
    );

  struct value v = utils_file_as_buf("./autark-cache/tests.log", 1024 * 1024);
  IW_TEST_ASSERT_EQUAL_FATAL(v.error, 0);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v.buf);
  IW_TEST_ASSERT_STR_EQUAL(
    v.buf,
    "test8_1\n"
    "test8_2\n"
    "test8_3\n"
    "test8_4\n"
    );
  value_destroy(&v);

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test8/Autark", &sctx), 0);
  script_build(sctx);
  script_close(&sctx);
  IW_TEST_ASSERT_EQUAL(xstr_size(xlog), 0);

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL_FATAL(system("touch ../../tests/data/test8/test8_2.c"), 0);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test8/Autark", &sctx), 0);
  script_build(sctx);
  script_close(&sctx);
  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:14     cc: outdated test8_2.c t=f f=s\n"
    "Autark:14     cc: resolved outdated outdated=1\n"
    "Autark:14     cc: build src=../test8_2.c obj=test8_2.o\n"
    "Autark:23    run: outdated test8_2.o t=f f=f\n"
    "Autark:23    run: resolved outdated outdated=1\n"
    "Autark:23    run: cc\n"
    "Autark:23    run: /bin/sh\n"
    );

  v = utils_file_as_buf("./autark-cache/tests.log", 1024 * 1024);
  IW_TEST_ASSERT_EQUAL_FATAL(v.error, 0);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v.buf);
  IW_TEST_ASSERT_STR_EQUAL(
    v.buf,
    "test8_1\n"
    "test8_2\n"
    "test8_3\n"
    "test8_4\n"
    "test8_2\n"
    );
  value_destroy(&v);

  chdir(cwd_prev);
  xstr_destroy(xlog);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_incremental_rebuild),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test8", cases, 0, 0));
}
