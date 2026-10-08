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
// Driver-only FVF fog capability from SDK d3dhal.h (FOGINFVF). This is
// deliberately distinct from public D3DPMISCCAPS_FOGANDSPECULARALPHA.
inline constexpr DWORD d3d9DdiFogInFvf = 0x00002000;
static_assert(d3d9DdiFogInFvf != D3DPMISCCAPS_FOGANDSPECULARALPHA);
inline constexpr size_t d3d9DeviceFunctionBytes =
  offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME);
static_assert(d3d9DeviceFunctionBytes == 99 * sizeof(void*));

HRESULT createAdapterDevice9(const std::shared_ptr<const AdapterIdentity>& identity,
                            D3DDDIARG_CREATEDEVICE* args);
}

// Original Microsoft DX8/DX9 runtime entry; only the implemented legacy
// profile is advertised. Modern OpenAdapter10/10_2 admission is independent.
extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args);

// The explicit test entry shares the same typed adapter/device implementation.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter9ForTest(D3DDDIARG_OPENADAPTER* args);
