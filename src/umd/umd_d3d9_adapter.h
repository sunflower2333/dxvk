#pragma once

#include <d3d9.h>
#include <d3dumddi.h>
#include "umd_adapter_identity.h"
#include <memory>

namespace dxvk::umd {
HRESULT createAdapterDevice9(const std::shared_ptr<const AdapterIdentity>& identity,
                            D3DDDIARG_CREATEDEVICE* args);
}

// Development-only typed adapter/device lifecycle. Rendering caps are zero
// and production OpenAdapter remains absent until runtime admission is ready.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter9ForTest(D3DDDIARG_OPENADAPTER* args);
