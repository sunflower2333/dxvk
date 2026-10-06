#pragma once

#include "../d3d11/d3d11_device.h"
#include "umd_gpu_backend.h"

namespace dxvk::umd {

struct Backend : GpuBackend {
  Com<ID3D11Device> d3d;
  Com<ID3D11DeviceContext> context;

  // Uses the supplied identity only. No DXGI factories, public D3D device
  // creation, adapter-index fallback, WARP or llvmpipe fallback.
  static HRESULT create(const AdapterLuid& luid,
                        D3D_FEATURE_LEVEL level,
                        std::unique_ptr<Backend>& result,
                        const RuntimeBackend* runtime = nullptr) noexcept;
};

}
