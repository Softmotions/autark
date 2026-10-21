#include "test_utils.h"
#include "script.h"

static void test_dependency_tracking(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  test_init(true);
  struct xstr *xlog = g_env.check.log = xstr_create_empty();
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx = 0;
  int rc = script_open("../../tests/data/test6/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  struct akpath_stat st[2];
  IW_TEST_ASSERT_EQUAL(access("autark-cache/hello", X_OK), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/hello", &st[0]), 0);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:2   env.sh: resolved outdated outdated=0\n"
    "Autark:36     cc: resolved outdated outdated=0\n"
    "Autark:36     cc: build src=../main.c obj=main.o\n"
    "Autark:36     cc: build src=../hello.c obj=hello.o\n"
    "Autark:42    run: resolved outdated outdated=0\n"
    "Autark:42    run: cc\n"
    "Autark:42    run: /bin/sh\n"
    );

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("../../tests/data/test6/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);
  IW_TEST_ASSERT_EQUAL(xstr_size(xlog), 0);

  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/hello", &st[1]), 0);
  IW_TEST_ASSERT_EQUAL(st[0].mtime, st[1].mtime);

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL_FATAL(system("touch ../../tests/data/test6/hello.c"), 0);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("../../tests/data/test6/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:36     cc: outdated hello.c t=f f=s\n"
    "Autark:36     cc: resolved outdated outdated=1\n"
    "Autark:36     cc: build src=../hello.c obj=hello.o\n"
    "Autark:42    run: outdated hello.o t=f f= \n"
    "Autark:42    run: resolved outdated outdated=1\n"
    "Autark:42    run: cc\n"
    "Autark:42    run: /bin/sh\n"
    );

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL_FATAL(system("touch ../../tests/data/test6/hello.h"), 0);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("../../tests/data/test6/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:36     cc: outdated main.c t=a f=s\n"
    "Autark:36     cc: outdated hello.c t=a f=s\n"
    "Autark:36     cc: resolved outdated outdated=2\n"
    "Autark:36     cc: build src=../main.c obj=main.o\n"
    "Autark:36     cc: build src=../hello.c obj=hello.o\n"
    "Autark:42    run: outdated main.o t=f f= \n"
    "Autark:42    run: outdated hello.o t=f f= \n"
    "Autark:42    run: resolved outdated outdated=2\n"
    "Autark:42    run: cc\n"
    "Autark:42    run: /bin/sh\n"
    );

  fprintf(stderr, "\n\n");
  setenv("CFLAGS", "-O2", 1);
  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL_FATAL(system("touch ../../tests/data/test6/hello.h"), 0);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("../../tests/data/test6/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xlog),
    "Autark:36     cc: outdated \1-DBUILD_TYPE=Debug\1-DDEBUG=1\1-O0\1-g t=v f= \n"
    "Autark:36     cc: outdated main.c t=a f=s\n"
    "Autark:36     cc: outdated hello.c t=a f=s\n"
    "Autark:36     cc: resolved outdated outdated=3\n"
    "Autark:36     cc: build src=../main.c obj=main.o\n"
    "Autark:36     cc: build src=../hello.c obj=hello.o\n"
    "Autark:42    run: outdated main.o t=f f= \n"
    "Autark:42    run: outdated hello.o t=f f= \n"
    "Autark:42    run: resolved outdated outdated=2\n"
    "Autark:42    run: cc\n"
    "Autark:42    run: /bin/sh\n"
    );

  xstr_destroy(g_env.check.log);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_dependency_tracking),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test6", cases, 0, 0));
}
