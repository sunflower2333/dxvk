#pragma once

#include "../dxvk/dxvk_instance.h"
#include "../dxvk/dxvk_device.h"
#include "umd_identity.h"
#include "umd_runtime_bridge.h"

namespace dxvk::umd {

struct GpuBackend {
  Rc<DxvkInstance> instance;
  Rc<DxvkAdapter> adapter;
  Rc<DxvkDevice> device;

  // Both embedded renderers use exactly one LUID/Turnip match. A native
  // runtime descriptor always reaches device creation; it cannot fall back
  // to the direct KMT backend used by standalone development probes.
  HRESULT initialize(const AdapterLuid& luid, DxvkInstanceFlags flags,
                     const RuntimeBackend* runtime) noexcept;
};

}
