#include <cstdio>

// Records which host C++ build flags were in effect (checked by the test suite)
int main() {
#ifdef HOST_TOOL_CXX
  std::puts("host-cxxflags=yes");
#else
  std::puts("host-cxxflags=no");
#endif
  return 0;
}
