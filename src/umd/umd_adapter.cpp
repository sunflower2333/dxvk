#include "umd_adapter.h"
#include <new>

namespace {
struct Adapter {
  std::shared_ptr<const dxvk::umd::AdapterIdentity> identity;
};

bool supported(UINT interfaceVersion, UINT version, UINT flags = 0) {
  // This development table is D3D10.0 only. In particular, do not reinterpret
  // a D3D11 table, promise a pipeline level or ignore a threading restriction.
  return interfaceVersion == D3D10_0_DDI_INTERFACE_VERSION
      && (version >> 16) >= D3D10_0_DDI_BUILD_VERSION && !flags;
}

SIZE_T APIENTRY privateDeviceSize(D3D10DDI_HADAPTER adapter,
    const D3D10DDIARG_CALCPRIVATEDEVICESIZE* args) {
  if (!adapter.pDrvPrivate || !args || !supported(args->Interface, args->Version, args->Flags))
    return 0;
  return VioGpuDxvkPrivateDeviceSize();
}

HRESULT APIENTRY createDevice(D3D10DDI_HADAPTER adapter, D3D10DDIARG_CREATEDEVICE* args) {
  if (!adapter.pDrvPrivate || !args) return E_INVALIDARG;
  if (!supported(args->Interface, args->Version, args->Flags)) return DXGI_ERROR_UNSUPPORTED;
  if (!args->pDeviceFuncs) return E_INVALIDARG;
  *args->pDeviceFuncs = {};
  if (!args->hDrvDevice.pDrvPrivate || !args->hRTDevice.handle || !args->pKTCallbacks
      || !args->hRTCoreLayer.handle || !args->pUMCallbacks || !args->pUMCallbacks->pfnSetErrorCb)
    return E_INVALIDARG;
  try {
    return dxvk::umd::createAdapterDevice(static_cast<Adapter*>(adapter.pDrvPrivate)->identity, args);
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY closeAdapter(D3D10DDI_HADAPTER adapter) {
  if (!adapter.pDrvPrivate) return E_INVALIDARG;
  delete static_cast<Adapter*>(adapter.pDrvPrivate);
  return S_OK;
}
}

extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapterForTest(D3D10DDIARG_OPENADAPTER* args) {
  if (!args) return E_INVALIDARG;
  args->hAdapter = {};
  if (!args->pAdapterFuncs) return E_INVALIDARG;
  *args->pAdapterFuncs = {};
  if (!supported(args->Interface, args->Version)) return DXGI_ERROR_UNSUPPORTED;
  if (!args->pAdapterCallbacks) return E_INVALIDARG;
  LUID luid = {};
  HRESULT hr = VioGpuDxvkQueryRuntimeAdapterLuid(args->hRTAdapter,
    args->pAdapterCallbacks->pfnQueryAdapterInfoCb, &luid);
  if (FAILED(hr)) return hr;
  try {
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = luid;
    identity->runtime = args->hRTAdapter;
    auto adapter = std::make_unique<Adapter>();
    adapter->identity = std::move(identity);
    args->pAdapterFuncs->pfnCalcPrivateDeviceSize = privateDeviceSize;
    args->pAdapterFuncs->pfnCreateDevice = createDevice;
    args->pAdapterFuncs->pfnCloseAdapter = closeAdapter;
    args->hAdapter.pDrvPrivate = adapter.release();
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
