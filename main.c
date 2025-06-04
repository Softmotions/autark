#ifndef _AMALGAMATE_
#include "autark.h"
#endif

int main(int argc, const char **argv) {
  autark_init();
  autark_run(argc, argv);
  autark_dispose();
  return 0;
}
