#pragma once

#include "umd_ddi.h"
#include <memory>

namespace dxvk::umd {

// Kept alive by every device independently of the adapter handle. Runtime
// handles are opaque and are returned only to their originating callbacks.
struct AdapterIdentity {
  LUID luid = {};
  D3D10DDI_HRTADAPTER runtime = {};
};

HRESULT createAdapterDevice(const std::shared_ptr<const AdapterIdentity>& identity,
  D3D10DDIARG_CREATEDEVICE* args);

}
