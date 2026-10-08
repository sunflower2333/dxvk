#pragma once

// Diagnostic-only Vista device-table boundary, using the original typed ABI.
// Optional unsupported entries stay null. Construction/state/resources and
// teardown retain the core's original functions; GPU workload entry points
// cannot forward while Microsoft's factory builds its enumeration device.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <cstddef>
#include <cstdint>

namespace dxvk::test {
template<auto Reject>
struct EnumerationDevice8 {
  template<typename Function> struct Entry;
  template<typename... Arguments>
  struct Entry<HRESULT (APIENTRY*)(HANDLE, Arguments...)> {
    static HRESULT APIENTRY call(HANDLE handle, Arguments...) {
      return Reject(handle);
    }
  };

  static uint32_t publish(D3DDDI_DEVICEFUNCS& table) {
    uint32_t mask = 0;
#define BLOCK(member, bit) \
    static_assert(offsetof(D3DDDI_DEVICEFUNCS, member) < \
      offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME)); \
    if (table.member) { \
      table.member = Entry<decltype(table.member)>::call; mask |= 1u << bit; \
    }
    BLOCK(pfnDrawPrimitive, 0)
    BLOCK(pfnDrawIndexedPrimitive, 1)
    BLOCK(pfnDrawRectPatch, 2)
    BLOCK(pfnDrawTriPatch, 3)
    BLOCK(pfnDrawPrimitive2, 4)
    BLOCK(pfnDrawIndexedPrimitive2, 5)
    BLOCK(pfnVolBlt, 6)
    BLOCK(pfnBufBlt, 7)
    BLOCK(pfnTexBlt, 8)
    BLOCK(pfnClear, 9)
    BLOCK(pfnBlt, 10)
    BLOCK(pfnColorFill, 11)
    BLOCK(pfnDepthFill, 12)
    BLOCK(pfnGenerateMipSubLevels, 13)
    BLOCK(pfnPresent, 14)
#undef BLOCK
    return mask;
  }
};
}
