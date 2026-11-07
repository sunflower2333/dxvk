#include "umd_primary.h"

#if defined(__clang__) && defined(__MINGW32__) && defined(__i386__)
// LLVM's i386 GNU backend cannot emit the SEH registration tables. The small
// C ABI boundary is compiled for Windows SEH; the actual typed accesses stay
// in this translation unit. The indirect call must remain opaque to its SEH
// caller, so both translation units are built without LTO.
extern "C" int dxvkUmdGuardPrimaryAccess(
    void (*access)(void*, const void*), void* destination, const void* source);
namespace {
void copyPrimary(void* destination, const void* source) {
  *static_cast<DXGI_DDI_PRIMARY_DESC*>(destination) =
    *static_cast<const DXGI_DDI_PRIMARY_DESC*>(source);
}
void copyPrimaryFlags(void* destination, const void* source) {
  static_cast<DXGI_DDI_PRIMARY_DESC*>(destination)->DriverFlags = *static_cast<const UINT*>(source);
}
}
#endif

namespace dxvk::umd {
// Keep asynchronous SEH local to these runtime-owned memory accesses. Clang
// otherwise treats plain loads/stores as non-trapping and removes __except,
// even with volatile or optimization disabled. The build enables SEH for
// this file without changing the C++ exception policy of the rendering code.
__declspec(noinline) bool readPrimary(const DXGI_DDI_PRIMARY_DESC* source,
    DXGI_DDI_PRIMARY_DESC& output) noexcept {
#if defined(__clang__) && defined(__MINGW32__) && defined(__i386__)
  return dxvkUmdGuardPrimaryAccess(copyPrimary, &output, source) != 0;
#else
  __try { output = *source; return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}
__declspec(noinline) bool writePrimaryFlags(DXGI_DDI_PRIMARY_DESC* output, UINT flags) noexcept {
#if defined(__clang__) && defined(__MINGW32__) && defined(__i386__)
  return dxvkUmdGuardPrimaryAccess(copyPrimaryFlags, output, &flags) != 0;
#else
  __try { output->DriverFlags = flags; return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}
}
