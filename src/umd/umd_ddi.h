#pragma once

#include <d3d10_1.h>
#include <d3d10umddi.h>

// Native entrypoints use the WDK ABI. Negotiation advertises no incomplete
// interface or feature level; these exports do not imply INF registration.
extern "C" HRESULT APIENTRY OpenAdapter10(D3D10DDIARG_OPENADAPTER* args);
extern "C" HRESULT APIENTRY OpenAdapter10_2(D3D10DDIARG_OPENADAPTER* args);

// Explicit development helpers can exercise the partial table independently
// of native capability admission. They are not ordinary runtime activation.
extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize();
// Uses the real WDK OpenAdapter/CreateDevice structures and callback ABI.
// Explicit harness name: no OpenAdapter10 export or system registration.
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapterForTest(D3D10DDIARG_OPENADAPTER* args);
// Consumer of the optional identity trailer. KMDs without the paired producer
// fail closed; this entry point does not publish OpenAdapter to the runtime.
extern "C" HRESULT APIENTRY VioGpuDxvkQueryRuntimeAdapterLuid(
  D3D10DDI_HRTADAPTER runtime, PFND3DDDI_QUERYADAPTERINFOCB query, LUID* luid);
extern "C" HRESULT APIENTRY VioGpuDxvkCreateDdiTestDevice(
  const LUID* luid, D3D10DDI_HDEVICE device,
  D3D10DDI_HRTCORELAYER runtime,
  const D3D10DDI_CORELAYER_DEVICECALLBACKS* callbacks,
  D3D10DDI_DEVICEFUNCS* functions);
