#include "test_utils.h"
#include "script.h"

int main(void) {
  test_init(false);

  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx;
  int rc = script_open("./data/test6/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);
  chdir(cwd_prev);

}

