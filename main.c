#include "autark.h"

int main(int argc, char **argv) {
  autark_init();
  autark_run(argc, argv);
  autark_dispose();
  return 0;
}
