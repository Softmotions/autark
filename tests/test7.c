#include "test_utils.h"
#include "script.h"

int main(void) {
  unsetenv("CC");
  unsetenv("CFLAGS");
  unsetenv("LDFLAGS");

  test_init(true);
  struct xstr *xlog = g_env.check.log = xstr_create_empty();
  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx;
  int rc = script_open("./data/test7/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);


  xstr_destroy(g_env.check.log);
}
