#include "test_utils.h"
#include "../log.h"
#include "../project.h"

int main(void) {
  int rc = 0;
  struct project *p;

  rc = project_open("./samples/sample1/Autark", &p);
  ASSERT(finish, rc == 0);


  project_close(&p);

finish:
  if (rc) {
    akerror(rc, "Error ", 0);
  }
  return rc ? rc : 0;
}
