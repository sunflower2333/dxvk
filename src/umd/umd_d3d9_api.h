#pragma once

#include <windows.h>
#include "umd_runtime_bridge.h"

// Development construction/teardown probe only. Its caller must keep the
// supplied callback dispatcher pumping throughout this synchronous call.
// No renderer object, device DDI or presentation capability is published.
extern "C" HRESULT APIENTRY VioGpuDxvkProbeD3D9BackendForTest(const LUID* luid,
    const dxvk::umd::RuntimeBackend* runtime, UINT* swapchainCount);
