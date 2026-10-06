#pragma once

#include "umd_gpu_backend.h"
#include "../d3d9/d3d9_interface.h"
#include "../d3d9/d3d9_device.h"

namespace dxvk::umd {

struct D3D9Backend : GpuBackend {
  Com<D3D9InterfaceEx> parent;
  Com<D3D9DeviceEx> d3d;

  // The caller pumps the original runtime's callbacks during construction,
  // rendering and synchronous destruction. Runtime ownership is mandatory.
  static HRESULT create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                        std::unique_ptr<D3D9Backend>& result) noexcept;
};

}
