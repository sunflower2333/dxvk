#include "umd_d3d9_backend.h"
#include "umd_d3d9_api.h"
#include "umd_gpu_backend.h"
#include "umd_runtime_validation.h"
#include "../d3d9/d3d9_interface.h"
#include "../d3d9/d3d9_device.h"
#include <dxgi.h>
#include <new>

namespace dxvk::umd {

struct D3D9Backend::State : GpuBackend {
  Com<D3D9InterfaceEx> parent;
  Com<D3D9DeviceEx> d3d;
};

D3D9Backend::D3D9Backend() : m_state(std::make_unique<State>()) { }
D3D9Backend::~D3D9Backend() = default;
IDirect3DDevice9Ex* D3D9Backend::device() const noexcept { return m_state->d3d.ptr(); }
HRESULT D3D9Backend::flush() noexcept {
  try { return m_state->d3d->FlushRuntimeSubmission(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  catch (...) { return D3DERR_DEVICELOST; }
}

static HRESULT d3d9Error(HRESULT hr) {
  switch (hr) {
    case DXGI_ERROR_UNSUPPORTED:
    case DXGI_ERROR_NOT_FOUND: return D3DERR_NOTAVAILABLE;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET: return D3DERR_DEVICELOST;
    default: return hr;
  }
}

HRESULT D3D9Backend::create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                          std::unique_ptr<D3D9Backend>& result) noexcept {
  result.reset();
  if (luid == AdapterLuid{} || !runtime || !validRuntimeBackend(*runtime))
    return E_INVALIDARG;
  try {
    auto backend = std::make_unique<D3D9Backend>();
    auto& state = *backend->m_state;
    HRESULT hr = state.initialize(luid, DxvkInstanceFlag::ClientApiIsD3D9, runtime);
    if (FAILED(hr)) return d3d9Error(hr);
    state.parent = new D3D9InterfaceEx(state.instance, state.adapter);
    state.d3d = new D3D9DeviceEx(state.parent.ptr(), state.parent->GetAdapter(0),
      D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING
      | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, state.device);
    hr = state.d3d->InitializeNativeOffscreen();
    if (FAILED(hr)) return hr;
    result = std::move(backend);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const DxvkError& e) {
      Logger::err(str::format("VIOGPU UMD D3D9 backend: ", e.message()));
      return D3DERR_NOTAVAILABLE;
    } catch (...) { return E_FAIL; }
}

}

extern "C" HRESULT APIENTRY VioGpuDxvkProbeD3D9BackendForTest(const LUID* luid,
    const dxvk::umd::RuntimeBackend* runtime, UINT* swapchainCount) {
  if (!luid || !swapchainCount) return E_INVALIDARG;
  dxvk::umd::AdapterLuid requested;
  static_assert(sizeof(requested) == sizeof(*luid));
  std::memcpy(requested.data(), luid, requested.size());
  std::unique_ptr<dxvk::umd::D3D9Backend> backend;
  const HRESULT hr = dxvk::umd::D3D9Backend::create(requested, runtime, backend);
  if (FAILED(hr)) return hr;
  const UINT count = backend->device()->GetNumberOfSwapChains();
  backend.reset();
  *swapchainCount = count;
  return S_OK;
}
