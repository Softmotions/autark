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

  struct akpath_stat st[2];
  akassert(access("autark-cache/hello", X_OK) == 0);
  akassert(path_stat("autark-cache/hello", &st[0]) == 0);

  struct value val = utils_file_as_buf("autark-cache/libhello/hello.h", 0x100000);
  akassert(val.error == 0);
  akassert(cmp_file_with_buf("hello.h.dump", val.buf, val.len) == 0);
  value_destroy(&val);

  akassert(
    strcmp(
      "Autark:14  env.sh: resolved outdated outdated=0\n"
      "libhello/Autark:2   libhello.sh: resolved outdated outdated=0\n"
      "libhello/Autark:29  configure: resolved outdated outdated=0\n"
      "Autark:46     cc: resolved outdated outdated=0\n"
      "Autark:46     cc: build src=../main.c obj=main.o\n"
      "Autark:46     cc: build src=../aux.c obj=aux.o\n"
      "libhello/Autark:10     cc: resolved outdated outdated=0\n"
      "libhello/Autark:10     cc: build src=../../libhello/hello.c obj=hello.o\n"
      "libhello/Autark:19    run: resolved outdated outdated=0\n"
      "libhello/Autark:19    run: ar\n"
      "Autark:58    run: resolved outdated outdated=0\n"
      "Autark:58    run: cc\n",
      xstr_ptr(xlog)) == 0
    );

  //---

  fprintf(stderr, "\n\n");
  chdir(cwd_prev);
  test_reinit(false);
  g_env.check.log = xstr_clear(xlog);
  rc = script_open("./data/test7/Autark", &sctx);
  akassert(rc == 0);
  script_build(sctx);
  script_close(&sctx);
  akassert(xstr_size(xlog) == 0);


  xstr_destroy(xlog);
}
