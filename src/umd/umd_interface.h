#pragma once

#include "umd_ddi.h"

namespace dxvk::umd {

enum class NativeInterface { Unsupported, D3D10, D3D10_1, D3D11 };

inline NativeInterface nativeInterface(UINT interfaceVersion) noexcept {
  switch (interfaceVersion) {
    case D3D10_0_DDI_INTERFACE_VERSION: return NativeInterface::D3D10;
    case D3D10_1_DDI_INTERFACE_VERSION: return NativeInterface::D3D10_1;
    case D3D11_0_DDI_INTERFACE_VERSION: return NativeInterface::D3D11;
    default: return NativeInterface::Unsupported;
  }
}

inline bool supportedNativeInterface(UINT interfaceVersion, UINT version, UINT flags = 0) noexcept {
  UINT build = 0;
  switch (nativeInterface(interfaceVersion)) {
    case NativeInterface::D3D10: build = D3D10_0_DDI_BUILD_VERSION; break;
    case NativeInterface::D3D10_1: build = D3D10_1_DDI_BUILD_VERSION; break;
    case NativeInterface::D3D11: build = D3D11_0_DDI_BUILD_VERSION; break;
    default: return false;
  }
  if ((version >> 16) < build) return false;
  if (nativeInterface(interfaceVersion) != NativeInterface::D3D11) return flags == 0;
  // SINGLETHREADED describes the caller; it does not require free-threaded
  // device access. DISABLE_EXTRA_THREAD_CREATION cannot be honored by the
  // embedded renderer and therefore remains rejected.
  const UINT allowed = D3D11DDI_CREATEDEVICE_FLAG_SINGLETHREADED
    | D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_MASK;
  if (flags & ~allowed) return false;
  switch (D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(flags)) {
    case D3D11DDI_3DPIPELINELEVEL_10_0:
    case D3D11DDI_3DPIPELINELEVEL_10_1:
    case D3D11DDI_3DPIPELINELEVEL_11_0: return true;
    default: return false;
  }
}

inline D3D_FEATURE_LEVEL nativeFeatureLevel(UINT interfaceVersion, UINT flags = 0) noexcept {
  switch (nativeInterface(interfaceVersion)) {
    case NativeInterface::D3D10: return D3D_FEATURE_LEVEL_10_0;
    case NativeInterface::D3D10_1: return D3D_FEATURE_LEVEL_10_1;
    case NativeInterface::D3D11:
      switch (D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(flags)) {
        case D3D11DDI_3DPIPELINELEVEL_10_0: return D3D_FEATURE_LEVEL_10_0;
        case D3D11DDI_3DPIPELINELEVEL_10_1: return D3D_FEATURE_LEVEL_10_1;
        case D3D11DDI_3DPIPELINELEVEL_11_0: return D3D_FEATURE_LEVEL_11_0;
        default: break;
      }
      break;
    default: break;
  }
  return static_cast<D3D_FEATURE_LEVEL>(0);
}

inline bool nativeDxgiUses1_1(UINT interfaceVersion, UINT version) noexcept {
  // DXGI's table revision is carried in the lower runtime-version bits, even
  // within one exact D3D device interface. The WDK macro defines its bounds.
  return IS_DXGI1_1_BASE_FUNCTIONS(interfaceVersion, version) != FALSE;
}

}
