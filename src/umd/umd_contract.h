#pragma once

#include "umd_ddi.h"
#include "umd_interface.h"
#include <cstdint>

namespace dxvk::umd {

enum RuntimeGap : uint32_t {
  StreamOutput = 1 << 0,
  Predication = 1 << 1,
  OpenedResources = 1 << 2,
  CompleteResources = 1 << 3,
  CompleteShaderSemantics = 1 << 4,
  MultipleRenderTargets = 1 << 5,
  PrimaryAndDxgi = 1 << 6,
  // Retired: the mandatory table is complete and ResetPrimitiveID /
  // SetVertexPipelineOutput exist only under D3D10PSGP, never in a
  // hardware UMD. The value stays reserved so a stale mask is obvious.
  LegacyPipelineCallbacks = 1 << 7,
  RuntimeThreading = 1 << 8,
  D3D10_1Semantics = 1 << 9,
  D3D11ImmediateSemantics = 1 << 10,
  D3D11ComputeAndUav = 1 << 11,
  D3D11Tessellation = 1 << 12,
  D3D11ShaderInterfaces = 1 << 13,
};

// This is immutable production capability policy, not an environment switch.
// A non-null partial DDI table alone cannot establish a D3D feature level.
uint32_t runtimeMissingD3D10Requirements() noexcept;

inline bool runtimeSupportsD3D10() noexcept {
  return runtimeMissingD3D10Requirements() == 0;
}

// A complete older interface does not admit a newer ABI or shader/resource
// profile. These immutable additional requirements stay closed until full
// native semantics and ordinary runtime/Turnip acceptance are proven.
inline uint32_t runtimeMissingD3D10_1Requirements() noexcept {
  return runtimeMissingD3D10Requirements() | D3D10_1Semantics;
}

inline uint32_t runtimeMissingD3D11Requirements(D3D_FEATURE_LEVEL level) noexcept {
  switch (level) {
    case D3D_FEATURE_LEVEL_10_0:
      return runtimeMissingD3D10Requirements() | D3D11ImmediateSemantics;
    case D3D_FEATURE_LEVEL_10_1:
      return runtimeMissingD3D10_1Requirements() | D3D11ImmediateSemantics;
    case D3D_FEATURE_LEVEL_11_0:
      return runtimeMissingD3D10_1Requirements() | D3D11ImmediateSemantics
        | D3D11ComputeAndUav | D3D11Tessellation | D3D11ShaderInterfaces;
    default: return UINT32_MAX;
  }
}

inline bool runtimeSupportsNativeInterface(UINT interfaceVersion, UINT flags = 0) noexcept {
  switch (nativeInterface(interfaceVersion)) {
    case NativeInterface::D3D10: return flags == 0 && runtimeSupportsD3D10();
    case NativeInterface::D3D10_1: return flags == 0 && runtimeMissingD3D10_1Requirements() == 0;
    case NativeInterface::D3D11:
      return supportedNativeInterface(interfaceVersion, D3D11_0_DDI_BUILD_VERSION << 16, flags)
        && runtimeMissingD3D11Requirements(nativeFeatureLevel(interfaceVersion, flags)) == 0;
    default: return false;
  }
}

}
