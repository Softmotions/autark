#include "test_utils.h"
#include "log.h"
#include "xstr.h"
#include "autark.h"

int main(void) {
  int rc = 0;
  struct sctx *p;
  struct xstr *xstr = xstr_create_empty();
  autark_init();

  rc = test_script_parse("./data/test2/Autark", &p);
  ASSERT(assert, rc == 0);

  script_dump(p, xstr);
  ASSERT(assert, cmp_file_with_xstr("./data/test2/Autark.dump", xstr) == 0);

  script_close(&p);
  xstr_destroy(xstr);

  if (rc) {
    akerror(rc, "Error ", 0);
  }
  return rc ? rc : 0;
assert:
  return 1;
}

