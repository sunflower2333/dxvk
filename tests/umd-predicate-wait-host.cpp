// SPDX-License-Identifier: MIT
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include "umd-predicate-wait-controls.h"

int main() {
  unsigned checks = 0;
  predicateWaitControls([&](bool condition) {
    ++checks;
    if (!condition) {
      std::fprintf(stderr, "native predicate wait policy check %u failed\n", checks);
      std::exit(1);
    }
  }, dxvk::umd::PredicateWaitCodes{0, 1, -2147467259},
    -2005270523, -2005270521, -2147024882);
  std::printf("native predicate wait policy PASS checks=%u pending_yields=20004 hardware_admission=0\n", checks);
}
