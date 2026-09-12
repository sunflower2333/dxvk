#pragma once

#include "umd_ddi.h"
#include "umd_runtime_identity.h"
#include <memory>

namespace dxvk::umd {

// Kept alive by every device independently of the adapter handle. Runtime
// handles are opaque and are returned only to their originating callbacks.
struct AdapterIdentity {
  LUID luid = {};
  D3D10DDI_HRTADAPTER runtime = {};
  PFND3DDDI_QUERYADAPTERINFOCB query = nullptr;
  uint64_t generation = 0;
  uint64_t capabilities = 0;
};

HRESULT queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
  PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result);

HRESULT createAdapterDevice(const std::shared_ptr<const AdapterIdentity>& identity,
  D3D10DDIARG_CREATEDEVICE* args);

}
