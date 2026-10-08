#include "umd-runtime-diagnostics.h"

int main() {
  const unsigned checks = runtimeDiagnosticsControls();
  std::printf("PASS %u runtime diagnostic output controls hardware_admission=0\n", checks);
  return 0;
}
