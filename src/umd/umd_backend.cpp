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

    if (runtime) {
      if (!runtime->owner || runtime->create.owner != runtime->owner.get()
          || runtime->create.sType != MWD_STYPE_DEVICE || runtime->create.pNext
          || !mwd_callbacks_valid(runtime->create.callbacks)) return E_INVALIDARG;
      mwd_support support = {MWD_STYPE_SUPPORT, nullptr, 0, 0, 0, 0};
      VkPhysicalDeviceProperties2 properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      properties.pNext = &support;
      backend->instance->vki()->vkGetPhysicalDeviceProperties2(backend->adapter->handle(), &properties);
      if (support.magic != MWD_RUNTIME_MAGIC || support.version != MWD_RUNTIME_ABI_VERSION
          || support.size != sizeof(mwd_callbacks) || support.flags != 1) return DXGI_ERROR_UNSUPPORTED;
      mwd_context_info contextInfo = {};
      const HRESULT ready = runtime->create.callbacks->context(runtime->create.owner, &contextInfo);
      if (ready != S_OK) return FAILED(ready) ? ready : E_FAIL;
      if (std::memcmp(contextInfo.luid, luid.data(), luid.size()) || !contextInfo.generation
          || !contextInfo.context_id || !contextInfo.queue_id) return DXGI_ERROR_DEVICE_REMOVED;
    }
    // The callback owner reaches VkDeviceCreateInfo before Turnip allocates any
    // internal BO. Native runtime creation never falls back to direct KMT.
    backend->device = backend->adapter->createDevice(runtime);
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
