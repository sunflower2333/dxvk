// SPDX-License-Identifier: MIT
// C calling conventions agree between i386 Windows GNU and MSVC targets.
// Keep this indirect call opaque (no LTO): Clang can otherwise remove the SEH
// scope around ordinary memory accesses. No C++ object crosses this boundary.
__declspec(noinline) int dxvkUmdGuardPrimaryAccess(
    void (*access)(void*, const void*), void* destination, const void* source) {
  __try {
    access(destination, source);
    return 1;
  } __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
    return 0;
  }
}
