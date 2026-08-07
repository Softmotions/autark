#include "libext/libext.h"

int main(void) {
  int r = callext();
  return r == 42 ? 0 : r;
}
