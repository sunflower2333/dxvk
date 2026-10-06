#include "umd_backend.h"
#include "umd_api.h"
#include "../dxvk/dxvk_instance.h"
#include "../d3d11/d3d11_context_imm.h"

#include <cstring>
#include <new>

namespace dxvk::umd {

HRESULT flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  if (!context) return E_INVALIDARG;
  try {
    return static_cast<D3D11ImmediateContext*>(context)->FlushRuntimeSubmission();
  } catch (...) { return DXGI_ERROR_DEVICE_REMOVED; }
}

HRESULT isStagingResourceBusy(ID3D11DeviceContext* context,
                             ID3D11Resource* resource, BOOL* busy) noexcept {
  if (!busy) return E_POINTER;
  *busy = TRUE;
  if (!context || !resource) return E_INVALIDARG;
  try {
    return static_cast<D3D11ImmediateContext*>(context)->IsStagingResourceBusy(resource, busy);
  } catch (const std::bad_alloc&) {
    return E_OUTOFMEMORY;
  } catch (...) {
    return E_FAIL;
  }
}

HRESULT createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
                     ID3D11Device** device, ID3D11DeviceContext** context,
                     const RuntimeBackend* runtime) noexcept {
  if (!device || !context)
    return E_POINTER;
  *device = nullptr;
  *context = nullptr;
  AdapterLuid requested;
  static_assert(sizeof(luid) == sizeof(requested));
  std::memcpy(requested.data(), &luid, sizeof(luid));
  std::unique_ptr<Backend> backend;
  HRESULT hr = Backend::create(requested, level, backend, runtime);
  if (FAILED(hr))
    return hr;
  *device = backend->d3d.ref();
  *context = backend->context.ref();
  return S_OK;
}

HRESULT Backend::create(const AdapterLuid& luid,
                       D3D_FEATURE_LEVEL level,
                       std::unique_ptr<Backend>& result,
                       const RuntimeBackend* runtime) noexcept {
  result.reset();
  if (luid == AdapterLuid{})
    return E_INVALIDARG;

  try {
    auto backend = std::make_unique<Backend>();
    const HRESULT initialized = backend->initialize(luid, 0, runtime);
    if (FAILED(initialized)) return initialized;
    // The embedded renderer implements DDI operations using D3D11 facilities
    // (notably NO_RASTERIZED_STREAM). Do not mislabel that implementation FL10.
    // Adapter capability admission remains independent and fail-closed.
    level = implementationFeatureLevel(level);
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
