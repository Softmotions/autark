#include "test_utils.h"
#include "script.h"

static void test_echo_and_dump(void) {
  char cwd_prev[PATH_MAX];
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct xstr *xstr = xstr_create_empty();
  test_init(true);
  g_env.check.log = xstr;

  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test10/Autark", &sctx), 0);
  script_build(sctx);

  IW_TEST_ASSERT_STR_EQUAL(
    xstr_ptr(xstr),
    "Autark:9    init: foo bar\n"
    "Autark:9    init: baz gaz\n"
    "Autark:7    echo: baz\n"
    "Autark:13   echo: JOIN foo bar baz\n"
    "Autark:7    echo: last\n"
    "Autark:13   echo: JOIN baz gaz last\n"
    );

  xstr_clear(xstr);
  script_dump(sctx, xstr);
  script_close(&sctx);

  chdir(cwd_prev);
  IW_TEST_ASSERT_EQUAL(cmp_file_with_xstr("../../tests/data/test10/Autark.dump", xstr), 0);
  xstr_destroy(xstr);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_echo_and_dump),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test10", cases, 0, 0));
}
