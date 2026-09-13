#include "umd_contract.h"

uint32_t dxvk::umd::runtimeMissingD3D10Requirements() noexcept {
  // Missing callbacks: GeometryShaderWithStreamOutput size/create, SoSetTargets,
  // DrawAuto, SetPredication, OpenedResource size/OpenResource, SetTextFilterSize,
  // ResetPrimitiveID and SetVertexPipelineOutput. The last two are version
  // dependent. Non-null callbacks also reject texture dimensions, primary and
  // shared ownership, several shader semantics and multiple render targets.
  // Keep all native feature levels unadvertised until these contracts, not
  // just their function pointers, have been implemented and validated.
  // Ordinary worker dispatch and Flush submission are implemented, but this
  // integration still needs actual native runtime/Turnip proof. Controlled
  // WARP fixture success alone must not remove this admission requirement.
  return StreamOutput | Predication | OpenedResources | CompleteResources
    | CompleteShaderSemantics | MultipleRenderTargets | PrimaryAndDxgi
    | LegacyPipelineCallbacks | RuntimeThreading;
}
