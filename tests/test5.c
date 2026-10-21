#include "test_utils.h"
#include "script.h"

static void test_incremental_build(void) {
  test_init(true);

  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx = 0;
  int rc = script_open("../../tests/data/test5/Autark", &sctx);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  script_build(sctx);
  script_close(&sctx);

  IW_TEST_ASSERT_EQUAL(access("autark-cache/.autark/.2.deps", F_OK), 0);
  IW_TEST_ASSERT_EQUAL(access("autark-cache/.autark/.2.env", F_OK), 0);
  IW_TEST_ASSERT_EQUAL(access("autark-cache/.autark/test-file.txt", F_OK), 0);

  struct akpath_stat st[6];
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/.autark/test-file.txt", &st[0]), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/run1-product1.txt", &st[2]), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/run2-product1.txt", &st[4]), 0);

  // Now do the second run
  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test5/Autark", &sctx), 0);
  script_build(sctx);
  script_close(&sctx);

  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/.autark/test-file.txt", &st[1]), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/run1-product1.txt", &st[3]), 0);
  IW_TEST_ASSERT_EQUAL_FATAL(path_stat("autark-cache/run2-product1.txt", &st[5]), 0);
  IW_TEST_ASSERT_EQUAL(st[0].mtime, st[1].mtime);
  IW_TEST_ASSERT_EQUAL(st[2].mtime, st[3].mtime);
  IW_TEST_ASSERT_EQUAL(st[4].mtime, st[5].mtime);

  struct value v = utils_file_as_buf("autark-cache/run1-product2.txt", 1024);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v.buf);
  IW_TEST_ASSERT_STR_EQUAL(v.buf, "VAL2\n");
  value_destroy(&v);

  v = utils_file_as_buf("autark-cache/run2-product2.txt", 1024);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v.buf);
  IW_TEST_ASSERT_STR_EQUAL(v.buf, "VAL2\n");
  value_destroy(&v);

  chdir(cwd_prev);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_incremental_build),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test5", cases, 0, 0));
}
