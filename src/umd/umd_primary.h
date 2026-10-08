#pragma once
#include "umd_ddi.h"
#include "umd_primary_policy.h"

namespace dxvk::umd {
static_assert(DXGI_DDI_PRIMARY_OPTIONAL == 1 && DXGI_DDI_PRIMARY_NONPREROTATED == 2
  && DXGI_DDI_PRIMARY_STEREO == 4 && DXGI_DDI_PRIMARY_INDIRECT == 8);
static_assert(DXGI_DDI_PRIMARY_DRIVER_FLAG_NO_SCANOUT == 1);
static_assert(DXGI_DDI_MODE_ROTATION_IDENTITY == 1
  && DXGI_DDI_MODE_SCANLINE_ORDER_PROGRESSIVE == 1 && DXGI_DDI_MODE_SCALING_CENTERED == 2);
static_assert(DXGI_FORMAT_R8G8B8A8_UNORM == 28 && DXGI_FORMAT_B8G8R8A8_UNORM == 87
  && DXGI_FORMAT_B8G8R8X8_UNORM == 88 && DXGI_FORMAT_R8G8B8A8_UNORM_SRGB == 29
  && DXGI_FORMAT_B8G8R8A8_UNORM_SRGB == 91 && DXGI_FORMAT_B8G8R8X8_UNORM_SRGB == 93);

// This pointer is a separate runtime-owned input/output ABI. Never retain it
// after creation, and contain a retired or guarded page without publishing.
inline bool readPrimary(const DXGI_DDI_PRIMARY_DESC* source, DXGI_DDI_PRIMARY_DESC& output) noexcept {
  __try { output = *source; return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline bool writePrimaryFlags(DXGI_DDI_PRIMARY_DESC* output, UINT flags) noexcept {
  __try { output->DriverFlags = flags; return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline HRESULT primaryResourcePlan(const D3D10DDIARG_CREATERESOURCE& args,
    const DXGI_DDI_PRIMARY_DESC& desc, PrimaryPlan& plan) {
  // The documented runtime sets pPrimaryDesc only with BIND_PRESENT.
  if (!(args.BindFlags & D3D10_DDI_BIND_PRESENT)) return E_INVALIDARG;
  // An optional primary can remain a copy-style shared buffer. It uses the
  // existing one-allocation owner and reports NO_SCANOUT; a real shared
  // primary still needs an unambiguous primary/staging-pair open ABI.
  const bool sharedCopy = args.MiscFlags == D3D10_DDI_RESOURCE_MISC_SHARED
    && (desc.Flags & DXGI_DDI_PRIMARY_OPTIONAL)
    && (args.Format == DXGI_FORMAT_R8G8B8A8_UNORM || args.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D || args.MipLevels != 1
      || args.ArraySize != 1 || args.Usage != D3D10_DDI_USAGE_DEFAULT || args.MapFlags
      || (args.MiscFlags && !sharedCopy) || args.SampleDesc.Count != 1 || args.SampleDesc.Quality
      || !(args.BindFlags & D3D10_DDI_BIND_RENDER_TARGET)) return DXGI_DDI_ERR_UNSUPPORTED;
  const auto& shape = args.pMipInfoList[0];
  if (shape.TexelDepth != 1 || shape.PhysicalDepth != 1 || shape.TexelWidth != shape.PhysicalWidth
      || shape.TexelHeight != shape.PhysicalHeight) return E_INVALIDARG;
  const auto& mode = desc.ModeDesc;
  PrimaryShape input{desc.Flags, desc.VidPnSourceId, shape.TexelWidth, shape.TexelHeight,
    UINT(args.Format), mode.Width, mode.Height, UINT(mode.Format),
    mode.RefreshRate.Numerator, mode.RefreshRate.Denominator,
    UINT(mode.ScanlineOrdering), UINT(mode.Rotation), UINT(mode.Scaling)};
  switch (primaryPlan(input, plan)) {
    case PrimaryStatus::Valid: return S_OK;
    case PrimaryStatus::Invalid: return E_INVALIDARG;
    default: return DXGI_DDI_ERR_UNSUPPORTED;
  }
}
}
