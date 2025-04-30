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
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(access("autark-cache/.autark/check1.sh.deps", R_OK) == 0);
  akassert(access("autark-cache/.autark/check1.sh.env", R_OK) == 0);
  akassert(access("autark-cache/.autark/test-file.txt", R_OK) == 0);

  struct akpath_stat st, st2;
  akassert(path_stat("autark-cache/.autark/test-file.txt", &st) == 0);

  // Now do the second run
  akassert(script_open("./data/test5/Autark", &sctx) == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(path_stat("autark-cache/.autark/test-file.txt", &st2) == 0);
  akassert(st.mtime == st2.mtime);

  chdir(cwd_prev);
  return 0;
}
