#include "test_utils.h"
#include "script.h"
#include "env.h"

int main(void) {
  test_init(true);

  struct sctx *sctx;
  akassert(script_open("../../tests/data/test9/Autark", &sctx) == 0);
  script_build(sctx);

  akassert(g_env.units.num == 3);

  for (int i = 0; i < g_env.units.num; ++i) {
    struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
    if (i == 0) {
      akassert(unit_env_get_raw(unit, "FOO") == 0);
      akassert(unit_env_get_raw(unit, "L1K1") != 0);
      akassert(unit_env_get_raw(unit, "L1K2") != 0);
      akassert(unit_env_get_raw(unit, "L2K2") != 0);
      akassert(unit_env_get_raw(unit, "L2K1") == 0);
    } else if (i == 1) {
      akassert(unit_env_get_raw(unit, "FOO") != 0);
      akassert(unit_env_get_raw(unit, "L1K1") == 0);
      akassert(unit_env_get_raw(unit, "L1K2") == 0);
      akassert(unit_env_get_raw(unit, "L2K2") == 0);
      akassert(unit_env_get_raw(unit, "L2K1") != 0);

      const char *v = unit_env_get_raw(unit, "COMPOSITE");
      akassert(v);
      akassert(strcmp(v, "\1one\1two\1three\1four") == 0);
    } else if (i == 2) {
      akassert(unit_env_get_raw(unit, "FOO") == 0);
      akassert(unit_env_get_raw(unit, "L1K1") == 0);
      akassert(unit_env_get_raw(unit, "L1K2") == 0);
      akassert(unit_env_get_raw(unit, "L2K2") == 0);
      akassert(unit_env_get_raw(unit, "L2K1") == 0);
    }
  }


  script_close(&sctx);
  return 0;
}
