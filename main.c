#include "log.h"
#include "autark.h"

int main(int argc, const char **argv) {
  int rc = autark_run(argc, argv);
  if (rc) {
    akerror(rc, "Build failed", 0);
  } else {
    akinfo("Build success");
  }
  return 0;
}
