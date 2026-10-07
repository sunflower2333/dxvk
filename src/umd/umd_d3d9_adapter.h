#pragma once

#include <d3d9.h>
#include <d3dumddi.h>
#include "umd_adapter_identity.h"
#include <cstddef>
#include <memory>

namespace dxvk::umd {
// These entrypoints implement the original D3D9 DDI. Current SDK headers are
// a superset used for local storage; their default version also requires newer
// resource, blit and synchronization DDIs that this bridge does not implement.
inline constexpr UINT d3d9DriverVersion = D3D_UMD_INTERFACE_VERSION_VISTA;
inline constexpr size_t d3d9DeviceFunctionBytes =
  offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME);
static_assert(d3d9DeviceFunctionBytes == 99 * sizeof(void*));

HRESULT createAdapterDevice9(const std::shared_ptr<const AdapterIdentity>& identity,
                            D3DDDIARG_CREATEDEVICE* args);
}

// Development-only typed adapter/device lifecycle with the implemented caps.
// Production OpenAdapter remains absent until runtime admission is ready.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter9ForTest(D3DDDIARG_OPENADAPTER* args);
