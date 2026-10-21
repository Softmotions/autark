#include "test_utils.h"
#include "script.h"
#include "env.h"

static void test_units_env(void) {
  test_init(true);

  struct sctx *sctx = 0;
  IW_TEST_ASSERT_EQUAL_FATAL(script_open("../../tests/data/test9/Autark", &sctx), 0);
  script_build(sctx);

  IW_TEST_ASSERT_EQUAL(g_env.units.num, 4);

  for (int i = 0; i < g_env.units.num; ++i) {
    struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
    if (i == 0) {
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "FOO"));
      IW_TEST_ASSERT_PTR_NOT_NULL(unit_env_get_raw(unit, "L1K1"));
      IW_TEST_ASSERT_PTR_NOT_NULL(unit_env_get_raw(unit, "L1K2"));
      IW_TEST_ASSERT_PTR_NOT_NULL(unit_env_get_raw(unit, "L2K2"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L2K1"));
    } else if (i == 1) {
      IW_TEST_ASSERT_PTR_NOT_NULL(unit_env_get_raw(unit, "FOO"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L1K1"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L1K2"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L2K2"));
      IW_TEST_ASSERT_PTR_NOT_NULL(unit_env_get_raw(unit, "L2K1"));

      const char *v = unit_env_get_raw(unit, "COMPOSITE");
      IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(v);
      IW_TEST_ASSERT_STR_EQUAL(v, "\1one\1two\1three\1four");
    } else if (i == 2) {
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "FOO"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L1K1"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L1K2"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L2K2"));
      IW_TEST_ASSERT_PTR_NULL(unit_env_get_raw(unit, "L2K1"));
    }
  }

  script_close(&sctx);
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_units_env),
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("test9", cases, 0, 0));
}
