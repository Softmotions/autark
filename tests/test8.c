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
  akassert(script_open("./data/test8/Autark", &sctx) == 0);
  script_build(sctx);
  script_close(&sctx);

  akassert(
    strcmp(
      "Autark:19     cc: resolved outdated outdated=0\n"
      "Autark:19     cc: build src=../test8_1.c obj=test8_1.o\n"
      "Autark:19     cc: build src=../test8_2.c obj=test8_2.o\n"
      "Autark:19     cc: build src=../test8_3.c obj=test8_3.o\n"
      "Autark:19     cc: build src=../test8_4.c obj=test8_4.o\n"
      "Autark:28    run: resolved outdated outdated=0\n"
      "Autark:28    run: cc\n"
      "Autark:28    run: sh\n"
      "Autark:28    run: cc\n"
      "Autark:28    run: sh\n"
      "Autark:28    run: cc\n"
      "Autark:28    run: sh\n"
      "Autark:28    run: cc\n"
      "Autark:28    run: sh\n",
      xstr_ptr(xlog)) == 0
    );

  struct value v = utils_file_as_buf("./autark-cache/tests.log", 1024 * 1024);
  akassert(!v.error);
  akassert(
    strcmp(
      "test8_1\n"
      "test8_2\n"
      "test8_3\n"
      "test8_4\n",
      v.buf) == 0
    );
  value_destroy(&v);

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  akassert(script_open("./data/test8/Autark", &sctx) == 0);
  script_build(sctx);
  script_close(&sctx);
  akassert(xstr_size(xlog) == 0);

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  akassert(system("touch ./data/test8/test8_2.c") == 0);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  akassert(script_open("./data/test8/Autark", &sctx) == 0);
  script_build(sctx);
  script_close(&sctx);
  akassert(
    strcmp(
      "Autark:19     cc: outdated test8_2.c t=f f=s\n"
      "Autark:19     cc: resolved outdated outdated=1\n"
      "Autark:19     cc: build src=../test8_2.c obj=test8_2.o\n"
      "Autark:28    run: outdated test8_2.o t=f f=f\n"
      "Autark:28    run: resolved outdated outdated=1\n"
      "Autark:28    run: cc\n"
      "Autark:28    run: sh\n",
      xstr_ptr(xlog)) == 0
    );

  v = utils_file_as_buf("./autark-cache/tests.log", 1024 * 1024);
  akassert(!v.error);
  akassert(
    strcmp(
      "test8_1\n"
      "test8_2\n"
      "test8_3\n"
      "test8_4\n"
      "test8_2\n",
      v.buf) == 0
    );
  value_destroy(&v);

  xstr_destroy(xlog);
}
