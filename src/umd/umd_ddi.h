#pragma once

#include <d3d10_1.h>
#include <d3d10umddi.h>

// Development harness entry points. They use real WDK DDI types, but the
// subset table is never published to the Windows runtime as full support.
// OpenAdapter registration follows only after the complete required table
// and the runtime-adapter identity contract have been implemented.
extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize();
extern "C" HRESULT APIENTRY VioGpuDxvkCreateDdiTestDevice(
  const LUID* luid, D3D10DDI_HDEVICE device,
  D3D10DDI_HRTCORELAYER runtime,
  const D3D10DDI_CORELAYER_DEVICECALLBACKS* callbacks,
  D3D10DDI_DEVICEFUNCS* functions);
