#include "test_utils.h"
#include "script.h"

#include <unistd.h>
#include <getopt.h>

int main(void) {
  test_init();
  g_env.project.clean = true;

  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx;
  int rc = script_open("./data/test5/Autark", &sctx);
  ASSERT(assert, rc == 0);
  script_build(sctx);

  // TODO: Checks

  script_close(&sctx);
  chdir(cwd_prev);
  return 0;

assert:
  return 1;
}
