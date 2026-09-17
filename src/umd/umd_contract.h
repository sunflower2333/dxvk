#pragma once

#include "umd_ddi.h"
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
};

// This is immutable production capability policy, not an environment switch.
// A non-null partial DDI table alone cannot establish a D3D feature level.
uint32_t runtimeMissingD3D10Requirements() noexcept;

inline bool runtimeSupportsD3D10() noexcept {
  return runtimeMissingD3D10Requirements() == 0;
}

}
