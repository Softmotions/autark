#include "autark.h"

int main(int argc, char **argv) {
  int rc = autark_run(argc, argv);
  return rc ? 1 : 0;
}
