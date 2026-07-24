#include "test_utils.h"
#include "script.h"

int main(void) {
  test_init(true);
  g_env.distr.flags |= DISTR_FLG_WITH_DEPS | DISTR_FLG_PACK;


  return 0;
}
