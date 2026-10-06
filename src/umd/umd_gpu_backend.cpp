#include "umd_gpu_backend.h"
#include "umd_runtime_validation.h"
#include <dxgi.h>
#include <new>

namespace dxvk::umd {

HRESULT GpuBackend::initialize(const AdapterLuid& luid, DxvkInstanceFlags flags,
                              const RuntimeBackend* runtime) noexcept {
  if (luid == AdapterLuid{}) return E_INVALIDARG;
  if (runtime && !validRuntimeBackend(*runtime)) return E_INVALIDARG;

  try {
    RuntimeBackendSnapshot retained(runtime);
    const RuntimeBackend* bridge = retained.get();

    GpuBackend candidate;
    candidate.instance = new DxvkInstance(flags);
    for (uint32_t i = 0; i < candidate.instance->adapterCount(); i++) {
      auto adapter = candidate.instance->enumAdapters(i);
      const auto info = adapter->info();
      AdapterLuid actual;
      std::memcpy(actual.data(), info.deviceLuid, actual.size());
      if (!matchesAdapter(luid, info.luidIsValid, actual,
                          VK_DRIVER_ID_MESA_TURNIP, info.driverId)) continue;
      if (candidate.adapter) return DXGI_ERROR_UNSUPPORTED;
      candidate.adapter = adapter;
    }
    if (!candidate.adapter) return DXGI_ERROR_NOT_FOUND;

    if (runtime) {
      mwd_support support = {MWD_STYPE_SUPPORT, nullptr, 0, 0, 0, 0};
      VkPhysicalDeviceProperties2 properties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      properties.pNext = &support;
      candidate.instance->vki()->vkGetPhysicalDeviceProperties2(candidate.adapter->handle(), &properties);
      if (!supportsRuntimeBackend(support)) return DXGI_ERROR_UNSUPPORTED;
      mwd_context_info info = {};
      const HRESULT ready = bridge->create.callbacks->context(bridge->create.owner, &info);
      if (ready != S_OK) return FAILED(ready) ? ready : E_FAIL;
      if (!matchesRuntimeContext(luid, info)) return DXGI_ERROR_DEVICE_REMOVED;
    }

    candidate.device = candidate.adapter->createDevice(bridge);
    *this = std::move(candidate);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const DxvkError& e) {
      Logger::err(str::format("VIOGPU UMD GPU backend: ", e.message()));
      return DXGI_ERROR_UNSUPPORTED;
    } catch (...) { return E_FAIL; }
}

}
