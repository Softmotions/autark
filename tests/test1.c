#include "test_utils.h"
#include "xstr.h"

static void test_parse_and_dump(void) {
  struct sctx *p = 0;
  struct xstr *xstr = xstr_create_empty();

  int rc = test_script_parse("../../tests/data/test1/Autark", &p);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  script_dump(p, xstr);
  IW_TEST_ASSERT_EQUAL(cmp_file_with_xstr("../../tests/data/test1/Autark.dump", xstr), 0);

  script_close(&p);
  xstr_destroy(xstr);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_parse_and_dump),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test1", cases, 0, 0));
}
