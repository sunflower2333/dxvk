#pragma once

#include <d3d9.h>
#include <d3dumddi.h>

// A development-only typed D3D9 handshake. Rendering and production
// OpenAdapter remain unavailable until the embedded renderer is complete.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter9ForTest(D3DDDIARG_OPENADAPTER* args);
