#include "test_utils.h"
#include "script.h"

int main(void) {
  char cwd_prev[PATH_MAX];
  akassert(getcwd(cwd_prev, sizeof(cwd_prev)));

  struct xstr *xstr = xstr_create_empty();
  test_init(true);
  g_env.check.log = xstr;

  struct sctx *sctx;
  akassert(script_open("../../tests/data/test10/Autark", &sctx) == 0);
  script_build(sctx);
  akassert(
    strcmp(
      "Autark:9    init: foo bar\n"
      "Autark:9    init: baz gaz\n"
      "Autark:7    echo: baz\n"
      "Autark:13   echo: JOIN baz gaz last\n"
      "Autark:7    echo: last\n"
      "Autark:13   echo: JOIN baz gaz last\n",
      xstr_ptr(xstr)) == 0);
  xstr_clear(xstr);
  script_dump(sctx, xstr);
  script_close(&sctx);

  chdir(cwd_prev);

  akassert(cmp_file_with_xstr("../../tests/data/test10/Autark.dump", xstr) == 0);
  xstr_destroy(xstr);
  return 0;
}
