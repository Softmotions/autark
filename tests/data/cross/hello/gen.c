#include <stdio.h>

// The generated source records which host build flags were in effect, so the
// test suite can verify the host-cc/host-cxx compilation flags.
int main(void) {
#ifdef HOST_TOOL_BUILD
  puts("/* host-tool-flags=host-cflags */");
#endif
#ifdef HOST_TOOL_CXX
  puts("/* host-tool-flags=host-cxxflags */");
#endif
  puts("int generated_value(void);");
  puts("int generated_value(void) { return 42; }");
  return 0;
}
