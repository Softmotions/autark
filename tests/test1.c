#include "test_utils.h"
#include "log.h"
#include "xstr.h"

int main(void) {
  int rc = 0;
  struct script *p;
  struct xstr *xstr = xstr_create_empty();

  rc = test_script_parse("./data/test1/Autark", &p);
  ASSERT(assert, rc == 0);

  script_print(p, xstr);
  ASSERT(assert, cmp_file_with_xstr("./data/test1/Autark.dump", xstr) == 0);

  script_close(&p);
  xstr_destroy(xstr);

  if (rc) {
    akerror(rc, "Error ", 0);
  }
  return rc ? rc : 0;
assert:
  return 1;
}
