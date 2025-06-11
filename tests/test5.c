#include "test_utils.h"
#include "script.h"

int main(void) {
  test_init(true);

  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx;
  int rc = script_open("../../tests/data/test5/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(access("autark-cache/.autark/check1.sh.deps", F_OK) == 0);
  akassert(access("autark-cache/.autark/check1.sh.env", F_OK) == 0);
  akassert(access("autark-cache/.autark/test-file.txt", F_OK) == 0);

  struct akpath_stat st[6];
  akassert(path_stat("autark-cache/.autark/test-file.txt", &st[0]) == 0);
  akassert(path_stat("autark-cache/run1-product1.txt", &st[2]) == 0);
  akassert(path_stat("autark-cache/run2-product1.txt", &st[4]) == 0);

  // Now do the second run
  chdir(cwd_prev);
  akassert(script_open("../../tests/data/test5/Autark", &sctx) == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(path_stat("autark-cache/.autark/test-file.txt", &st[1]) == 0);
  akassert(path_stat("autark-cache/run1-product1.txt", &st[3]) == 0);
  akassert(path_stat("autark-cache/run2-product1.txt", &st[5]) == 0);
  akassert(st[0].mtime == st[1].mtime);
  akassert(st[2].mtime == st[3].mtime);
  akassert(st[4].mtime == st[5].mtime);

  struct value v = utils_file_as_buf("autark-cache/run1-product2.txt", 1024);
  akassert(v.buf);
  akassert(strcmp(v.buf, "VAL2\n") == 0);
  value_destroy(&v);

  v = utils_file_as_buf("autark-cache/run2-product2.txt", 1024);
  akassert(v.buf);
  akassert(strcmp(v.buf, "VAL2\n") == 0);
  value_destroy(&v);

  chdir(cwd_prev);
  return 0;
}
