#pragma once

#include "../d3d11/d3d11_device.h"
#include "umd_identity.h"

namespace dxvk::umd {

struct Backend {
  Rc<DxvkInstance> instance;
  Rc<DxvkAdapter> adapter;
  Rc<DxvkDevice> device;
  Com<ID3D11Device> d3d;
  Com<ID3D11DeviceContext> context;

  // Uses the supplied identity only. No DXGI factories, public D3D device
  // creation, adapter-index fallback, WARP or llvmpipe fallback.
  static HRESULT create(const AdapterLuid& luid,
                        D3D_FEATURE_LEVEL level,
                        std::unique_ptr<Backend>& result) noexcept;
};

}
