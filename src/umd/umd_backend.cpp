#include "umd_backend.h"
#include "../dxvk/dxvk_instance.h"

#include <cstring>
#include <new>

namespace dxvk::umd {

HRESULT Backend::create(const AdapterLuid& luid,
                       D3D_FEATURE_LEVEL level,
                       std::unique_ptr<Backend>& result) noexcept {
  result.reset();
  if (luid == AdapterLuid{})
    return E_INVALIDARG;

  try {
    auto backend = std::make_unique<Backend>();
    backend->instance = new DxvkInstance(0);

    // Check every candidate and reject ambiguous identities. Driver ID is
    // checked independently: a matching LUID on a software ICD is not proof
    // of the requested Turnip device.
    for (uint32_t i = 0; i < backend->instance->adapterCount(); i++) {
      auto adapter = backend->instance->enumAdapters(i);
      const auto info = adapter->info();
      AdapterLuid candidate;
      std::memcpy(candidate.data(), info.deviceLuid, candidate.size());
      if (!matchesAdapter(luid, info.luidIsValid, candidate,
                          VK_DRIVER_ID_MESA_TURNIP, info.driverId))
        continue;
      if (backend->adapter)
        return DXGI_ERROR_UNSUPPORTED;
      backend->adapter = adapter;
    }
    if (!backend->adapter)
      return DXGI_ERROR_NOT_FOUND;

    backend->device = backend->adapter->createDevice();
    if (level > D3D11Device::GetMaxFeatureLevel(*backend->device))
      return DXGI_ERROR_UNSUPPORTED;

    Com<D3D11DXGIDevice> container = new D3D11DXGIDevice(
      nullptr, nullptr, nullptr, backend->instance, backend->adapter,
      backend->device, level, 0);
    HRESULT hr = container->QueryInterface(__uuidof(ID3D11Device),
      reinterpret_cast<void**>(&backend->d3d));
    if (FAILED(hr))
      return hr;
    backend->d3d->GetImmediateContext(&backend->context);
    result = std::move(backend);
    return S_OK;
  } catch (const std::bad_alloc&) {
    return E_OUTOFMEMORY;
  } catch (const DxvkError& e) {
    Logger::err(str::format("VIOGPU UMD backend: ", e.message()));
    return DXGI_ERROR_UNSUPPORTED;
  } catch (...) {
    return E_FAIL;
  }
}

}
