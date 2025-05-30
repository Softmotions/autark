#include "test_utils.h"
#include "script.h"

int main(void) {
  test_init(true);
  struct xstr *xlog = g_env.check.log = xstr_create_empty();
  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct sctx *sctx;
  int rc = script_open("./data/test6/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(
    strcmp(
      "Autark:2   env.sh: resolved outdated outdated=0\n"
      "Autark:36     cc: resolved outdated outdated=0\n"
      "Autark:36     cc build src=../main.c obj=main.o\n"
      "Autark:36     cc build src=../hello.c obj=hello.o\n"
      "Autark:42    run: resolved outdated outdated=0\n"
      "Autark:42    run: gcc\n",
      xstr_ptr(xlog)) == 0
    );
  xstr_clear(xlog);

  fprintf(stderr, "\n\n");

  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xlog;
  rc = script_open("./data/test6/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  akassert(xstr_size(xlog) == 0);
  script_close(&sctx);



  xstr_destroy(g_env.check.log);
}
