#include "umd_contract.h"

uint32_t dxvk::umd::runtimeMissingD3D10Requirements() noexcept {
  // The mandatory D3D10 device table is now complete: every slot the WDK
  // declares for a hardware UMD is non-null. CalcPrivateOpenedResourceSize,
  // OpenResource and SetTextFilterSize were the last three. ResetPrimitiveID
  // and SetVertexPipelineOutput are not version dependent as previously
  // recorded here: d3d10umddi.h declares them only under D3D10PSGP, so a
  // hardware UMD has no such fields to fill and LegacyPipelineCallbacks is
  // genuinely retired. A complete table is a prerequisite for admission, not
  // admission itself: OpenResource still fails, and the capabilities below
  // are still missing, so no native feature level may be advertised.
  // Non-null callbacks also reject texture dimensions, primary and shared
  // ownership and several shader semantics. The float MRT slice is
  // implemented, but full MRT formats/semantics and target validation remain
  // admission requirements; do not clear MultipleRenderTargets from fixtures.
  // Keep all native feature levels unadvertised until these contracts, not
  // just their function pointers, have been implemented and validated.
  // Ordinary worker dispatch and Flush submission are implemented, but this
  // integration still needs actual native runtime/Turnip proof. Controlled
  // WARP fixture success alone must not remove this admission requirement.
  // Occlusion predication has a synchronous correctness fallback, but the
  // full predicate contract still needs real Turnip proof. GS stream output,
  // SO statistics/overflow and DrawAuto have a bounded implementation, but
  // null-GS passthrough, full shader semantics and target proof remain gaps.
  return StreamOutput | Predication | OpenedResources | CompleteResources
    | CompleteShaderSemantics | MultipleRenderTargets | PrimaryAndDxgi
    | RuntimeThreading;
}
