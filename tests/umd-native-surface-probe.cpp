#include "umd-kmt-compute-transport.h"
#include "umd-kmt-runtime-paging.h"
#include "../src/umd/umd_runtime_gpu.h"
#include "../src/umd/umd_native_surface.h"
#include "umd-native-vulkan-image.h"
#include <cerrno>
#include <cstdlib>
#include <memory>

HRESULT exerciseNativeD3D(const LUID&, uint32_t, uint32_t, uint32_t,
  dxvk::umd::RuntimeBackend, const mwd_allocation&, const mwd_allocation&, uint32_t, bool);

// Bounded real-KMD allocation/import/retirement probe with optional GPU pixel
// verification. Native surfaces are never CPU mapped or published to desktop.
using NativeSurface = dxvk::umd::NativeSurfaceInfo;
using ResourceShare = dxvk::umd::NativeResourceShare;

class KmtNativeSurface final {
public:
  explicit KmtNativeSurface(KmtComputeTransport& transport) : m_transport(transport) { }
  ~KmtNativeSurface() {
    const HRESULT hr = close();
    if (hr != S_OK) std::printf("NATIVE_SURFACE_DESTRUCTOR hr=%08lx\n", (unsigned long)hr);
  }
  HRESULT open(uint32_t width, uint32_t height, bool used = true, bool runtimeWrapper = false, bool primary = false) {
    if (m_surface.key || m_resource || !m_transport.m_device) return E_INVALIDARG;
    if (runtimeWrapper) return openRuntime(width, height, used, primary);
    HRESULT hr = dxvk::umd::openKernelIdentity(m_transport.m_identity, m_source);
    if (hr == S_OK) hr = dxvk::umd::NativeSurfaceLease::create(m_transport.m_identity,
      m_source, width, height, 0x34325241, m_lease);
    if (m_lease) m_surface = m_lease->info();
    std::printf("NATIVE_SURFACE_ALLOCATE hr=%08lx key=%llu bytes=%llu generation=%llu"
      " resource=%u width=%u height=%u stride=%u modifier=%llu offset=%llu planes=%u layout=%u\n",
      (unsigned long)hr, (unsigned long long)m_surface.key, (unsigned long long)m_surface.size,
      (unsigned long long)m_surface.generation, m_surface.resource, m_surface.width,
      m_surface.height, m_surface.stride, (unsigned long long)m_surface.modifier,
      (unsigned long long)m_surface.offset, m_surface.planes, m_surface.layoutFlags);
    if (hr != S_OK) return hr;
    if (!m_lease) return E_FAIL;
    dxvk::umd::AllocationInfo info;
    info.flags = 0; info.size = m_surface.size; info.width = width; info.height = height;
    info.pitch = m_surface.stride; info.format = 1;
    ResourceShare share; share.key = m_surface.key; share.stride = m_surface.stride;
    D3DDDI_ALLOCATIONINFO allocation = {};
    allocation.pPrivateDriverData = &info; allocation.PrivateDriverDataSize = sizeof(info);
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = m_transport.m_device; request.Flags.CreateResource = 1;
    request.NumAllocations = 1; request.pAllocationInfo = &allocation;
    request.pPrivateDriverData = &share; request.PrivateDriverDataSize = sizeof(share);
    try { m_transport.m_nativeAllocations.reserve(m_transport.m_nativeAllocations.size() + 1); }
    catch (...) { return E_OUTOFMEMORY; }
    hr = KmtComputeTransport::result(D3DKMTCreateAllocation(&request));
    // Capture all returned ownership before validating a callback result.
    m_resource = request.hResource; m_allocation = allocation.hAllocation;
    std::printf("NATIVE_SURFACE_WRAPPER hr=%08lx resource=%u allocation=%u\n",
      (unsigned long)hr, m_resource, m_allocation);
    if (hr != S_OK) return hr;
    if (!m_resource || !m_allocation || m_transport.allocation(m_allocation)
        || m_transport.nativeAllocation(m_allocation)) return E_FAIL;
    m_transport.m_nativeAllocations.push_back({m_allocation, m_surface.key, m_surface.size, m_surface.generation, used});
    return S_OK;
  }
  HRESULT close() {
    if (m_memory) {
      const HRESULT released = m_backing ? m_backing->release() : S_OK;
      if (released != S_OK) return released;
      const HRESULT closed = m_memory->closeDeviceAllocations();
      if (closed != S_OK) return closed;
      m_backing.reset(); m_memory.reset(); m_surface.key = 0;
      for (auto& entry : s_wrappers) if (entry == this) entry = nullptr;
    }
    if (m_resource || m_allocation) {
      const HRESULT evicted = m_transport.evict(m_allocation);
      if (evicted != S_OK) return evicted;
      D3DKMT_DESTROYALLOCATION request = {}; request.hDevice = m_transport.m_device;
      if (m_resource) request.hResource = m_resource;
      else { request.AllocationCount = 1; request.phAllocationList = &m_allocation; }
      const HRESULT hr = KmtComputeTransport::result(D3DKMTDestroyAllocation(&request));
      std::printf("NATIVE_SURFACE_DESTROY_WRAPPER hr=%08lx\n", (unsigned long)hr);
      if (hr != S_OK) return hr;
      auto& borrowed = m_transport.m_nativeAllocations;
      borrowed.erase(std::remove_if(borrowed.begin(), borrowed.end(),
        [&](const auto& entry) { return entry.handle == m_allocation; }), borrowed.end());
      m_resource = m_allocation = 0;
    }
    if (m_lease) {
      const HRESULT hr = m_lease->close();
      std::printf("NATIVE_SURFACE_FREE hr=%08lx key=%llu production_lease=1\n", (unsigned long)hr,
        (unsigned long long)m_surface.key);
      if (hr != S_OK) return hr;
      m_lease.reset(); m_surface.key = 0;
    }
    return S_OK;
  }
  std::shared_ptr<dxvk::umd::AdapterIdentity> identity(const D3DDDI_ADAPTERCALLBACKS& cb, bool runtimePaging = false) {
    auto value = std::make_shared<dxvk::umd::AdapterIdentity>();
    value->luid = m_transport.m_luid; value->runtime = m_transport.runtimeAdapter().handle;
    value->query = cb.pfnQueryAdapterInfoCb;
    value->generation = m_transport.m_identity.generation;
    value->capabilities = m_transport.m_identity.capabilities;
    // The optional mode uses the production explicit-residency path. Legacy
    // modes retain the original Render-time residency baseline.
    if (runtimePaging) value->continuity = m_source;
    return value;
  }
  uint32_t handle() const { return m_allocation; }
  const NativeSurface& surface() const { return m_surface; }
private:
  static KmtNativeSurface* wrapper(HANDLE cookie, HANDLE resource) {
    for (auto entry : s_wrappers) if (entry && resource == entry
        && KmtComputeTransport::owner(cookie, false) == &entry->m_transport) return entry;
    return nullptr;
  }
  static HRESULT APIENTRY allocateWrapper(HANDLE cookie, D3DDDICB_ALLOCATE* args) {
    auto s = args ? wrapper(cookie, args->hResource) : nullptr;
    if (!s || args->NumAllocations != 1 || !args->pAllocationInfo || args->hKMResource
        || !args->pPrivateDriverData || args->PrivateDriverDataSize != sizeof(ResourceShare)) return E_INVALIDARG;
    const auto& share = *static_cast<const ResourceShare*>(args->pPrivateDriverData);
    const auto& slot = args->pAllocationInfo[0];
    if (!slot.pPrivateDriverData || slot.PrivateDriverDataSize != sizeof(dxvk::umd::AllocationInfo)) return E_INVALIDARG;
    const auto& info = *static_cast<const dxvk::umd::AllocationInfo*>(slot.pPrivateDriverData);
    if (!dxvk::umd::validNativeResourceShare(share, info.width)
        || info.flags != unsigned(s->m_primary) || slot.Flags.Value != unsigned(s->m_primary)
        || info.refreshNumerator != (s->m_primary ? 165u : 0u)
        || info.refreshDenominator != unsigned(s->m_primary)) return E_INVALIDARG;
    auto& transport = s->m_transport;
    transport.m_nativeAllocations.reserve(transport.m_nativeAllocations.size() + 1);
    D3DKMT_CREATEALLOCATION request = {}; request.hDevice = transport.m_device;
    request.Flags.CreateResource = 1; request.NumAllocations = 1; request.pAllocationInfo = args->pAllocationInfo;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const HRESULT hr = KmtComputeTransport::result(D3DKMTCreateAllocation(&request));
    s->m_resource = request.hResource; s->m_allocation = slot.hAllocation;
    args->hKMResource = request.hResource;
    std::printf("NATIVE_RUNTIME_ALLOCATE hr=%08lx resource=%u allocation=%u count=1 cpu_staging=0 primary=%u\n",
      (unsigned long)hr, s->m_resource, s->m_allocation, unsigned(s->m_primary));
    if (hr != S_OK) return hr;
    if (!s->m_resource || !s->m_allocation || transport.nativeAllocation(s->m_allocation)
        || transport.allocation(s->m_allocation)) return E_FAIL;
    transport.m_nativeAllocations.push_back({s->m_allocation, share.key, info.size,
      transport.m_identity.generation, s->m_used});
    return S_OK;
  }
  static HRESULT APIENTRY deallocateWrapper(HANDLE cookie, const D3DDDICB_DEALLOCATE* args) {
    auto s = args ? wrapper(cookie, args->hResource) : nullptr;
    if (!s || args->NumAllocations || args->HandleList || !s->m_resource) return E_INVALIDARG;
    const HRESULT evicted = s->m_transport.evict(s->m_allocation);
    if (evicted != S_OK) return evicted;
    D3DKMT_DESTROYALLOCATION request = {}; request.hDevice = s->m_transport.m_device;
    request.hResource = s->m_resource;
    const HRESULT hr = KmtComputeTransport::result(D3DKMTDestroyAllocation(&request));
    std::printf("NATIVE_RUNTIME_DEALLOCATE hr=%08lx resource=%u allocation=%u\n",
      (unsigned long)hr, s->m_resource, s->m_allocation);
    if (hr != S_OK) return hr;
    auto& borrowed = s->m_transport.m_nativeAllocations;
    borrowed.erase(std::remove_if(borrowed.begin(), borrowed.end(),
      [&](const auto& entry) { return entry.handle == s->m_allocation; }), borrowed.end());
    s->m_resource = s->m_allocation = 0;
    return S_OK;
  }
  static HRESULT APIENTRY unusedPresent(HANDLE, DXGIDDICB_PRESENT*) { return E_NOTIMPL; }
  static HRESULT APIENTRY unusedSetDisplayMode(HANDLE, D3DDDICB_SETDISPLAYMODE*) {
    // Allocation-only fixture: an unexpected mode switch must fail closed.
    return E_NOTIMPL;
  }
  HRESULT openRuntime(uint32_t width, uint32_t height, bool used, bool primary) {
    HRESULT hr = dxvk::umd::openKernelIdentity(m_transport.m_identity, m_source);
    if (hr != S_OK) return hr;
    D3DDDI_ADAPTERCALLBACKS adapter = {}; D3DDDI_DEVICECALLBACKS callbacks = {};
    D3D11DDI_CORELAYER_DEVICECALLBACKS core = {};
    m_transport.callbacks(adapter, callbacks, core);
    auto bound = identity(adapter); bound->continuity = m_source;
    callbacks.pfnAllocateCb = allocateWrapper; callbacks.pfnDeallocateCb = deallocateWrapper;
    callbacks.pfnSetDisplayModeCb = unusedSetDisplayMode;
    m_dxgi.pfnPresentCb = unusedPresent;
    auto slot = std::find(s_wrappers.begin(), s_wrappers.end(), nullptr);
    if (slot == s_wrappers.end()) return E_OUTOFMEMORY;
    *slot = this; m_used = used; m_primary = primary;
    m_memory = std::make_unique<dxvk::umd::RuntimeMemory>();
    m_backing = std::make_shared<dxvk::umd::RuntimeAllocation>();
    m_memory->initialize(m_transport.runtimeDevice().handle, callbacks, &m_dxgi, bound);
    hr = m_memory->allocateNative(*m_backing, this, width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
      primary, primary ? 165 : 0, primary ? 1 : 0);
    if (hr != S_OK) return hr;
    if (!m_backing->nativeSurface()) return E_FAIL;
    m_surface = *m_backing->nativeSurface();
    std::printf("NATIVE_RUNTIME_SURFACE key=%llu bytes=%llu stride=%u generation=%llu\n",
      (unsigned long long)m_surface.key, (unsigned long long)m_surface.size, m_surface.stride,
      (unsigned long long)m_surface.generation);
    return S_OK;
  }
  inline static std::array<KmtNativeSurface*, 2> s_wrappers = {};
  KmtComputeTransport& m_transport;
  NativeSurface m_surface;
  std::shared_ptr<const dxvk::umd::AdapterIdentitySource> m_source;
  std::shared_ptr<dxvk::umd::NativeSurfaceLease> m_lease;
  std::unique_ptr<dxvk::umd::RuntimeMemory> m_memory;
  std::shared_ptr<dxvk::umd::RuntimeAllocation> m_backing;
  DXGI_DDI_BASE_CALLBACKS m_dxgi = {};
  bool m_used = true;
  bool m_primary = false;
  D3DKMT_HANDLE m_resource = 0, m_allocation = 0;
};

static bool number(const char* input, uint32_t& output, int base) {
  if (!input || !*input || *input == '-' || *input == '+') return false;
  char* end = nullptr; errno = 0;
  const unsigned long long value = std::strtoull(input, &end, base);
  if (errno || !end || *end || value > UINT32_MAX) return false;
  output = uint32_t(value); return true;
}

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  uint32_t low = 0, high = 0, width = 0, height = 0;
  const bool runtimePaging = argc == 6 && !std::strcmp(argv[5], "--runtime-primary-paging");
  const bool primary = runtimePaging || (argc == 6 && !std::strcmp(argv[5], "--runtime-primary"));
  const bool runtimeWrapper = primary || (argc == 6 && !std::strcmp(argv[5], "--runtime-rotate"));
  const bool rotate = runtimeWrapper || (argc == 6 && !std::strcmp(argv[5], "--rotate"));
  const bool embedded = rotate || (argc == 6 && !std::strcmp(argv[5], "--embedded"));
  const bool pixels = embedded || (argc == 7 && !std::strcmp(argv[5], "--pixels"));
  const char* candidate = argc == 7 && (pixels || !std::strcmp(argv[5], "--modifier")) ? argv[6] : nullptr;
  const bool layout = candidate || (argc == 6 && !std::strcmp(argv[5], "--layout"));
  if ((argc != 5 && !layout && !embedded) || !number(argv[1], low, 16) || !number(argv[2], high, 16) || !(low || high)
      || !number(argv[3], width, 10) || !number(argv[4], height, 10)
      || !width || !height || width > 4096 || height > 4096) {
    std::fprintf(stderr, "usage: probe LUID_LOW_HEX LUID_HIGH_HEX WIDTH HEIGHT\n"); return 2;
  }
  KmtComputeTransport transport;
  KmtRuntimePaging paging(transport);
  const LUID luid = {low, LONG(high)};
  HRESULT hr = transport.open(luid);
  std::printf("NATIVE_SURFACE_ADAPTER hr=%08lx luid=%08x:%08x\n", (unsigned long)hr, high, low);
  auto surface = std::make_shared<KmtNativeSurface>(transport);
  if (hr == S_OK) hr = surface->open(width, height, true, runtimeWrapper, primary);
  auto unused = std::make_shared<KmtNativeSurface>(transport);
  if (hr == S_OK && embedded) hr = unused->open(rotate ? width : 64, rotate ? height : 64, rotate, runtimeWrapper, primary);
  D3DDDI_ADAPTERCALLBACKS adapter = {};
  D3DDDI_DEVICECALLBACKS callbacks = {};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core = {};
  transport.callbacks(adapter, callbacks, core);
  if (hr == S_OK && runtimePaging) {
    // Initial binding really uses the runtime adapter callback; subsequent
    // continuity checks use the independently owned kernel adapter.
    dxvk::umd::RuntimeIdentity current;
    hr = dxvk::umd::queryRuntimeIdentity(transport.runtimeAdapter().handle,
      adapter.pfnQueryAdapterInfoCb, current);
    if (hr == S_OK) hr = paging.callbacks(callbacks);
  }
  std::shared_ptr<dxvk::umd::RuntimeGpu> gpu;
  auto service = std::make_shared<dxvk::umd::RuntimeService>(true);
  dxvk::umd::RuntimeService::Scope caller(service.get());
  mwd_allocation imported = {};
  mwd_allocation unusedImport = {};
  mwd_allocation reopenedImport = {};
  std::shared_ptr<std::shared_ptr<KmtNativeSurface>> reopenedOwner;
  if (hr == S_OK) {
    gpu = dxvk::umd::RuntimeGpu::create(transport.runtimeDevice().handle, callbacks,
      surface->identity(adapter, runtimePaging), service);
    hr = gpu->importNativeSurface(surface->handle(), surface->surface().key,
      surface->surface().size, surface, &imported);
    std::printf("NATIVE_SURFACE_IMPORT hr=%08lx token=%p handle=%u address=%llu size=%llu flags=%u\n",
      (unsigned long)hr, imported.token, imported.handle, (unsigned long long)imported.address,
      (unsigned long long)imported.size, imported.flags);
    if (hr == S_OK && (!imported.token || imported.handle != surface->handle()
        || imported.size != surface->surface().size || !imported.address || imported.address % 4096
        || imported.generation != surface->surface().generation || imported.flags != 20)) hr = E_FAIL;
    if (hr == S_OK && runtimeWrapper) {
      reopenedOwner = std::make_shared<std::shared_ptr<KmtNativeSurface>>(surface);
      const auto before = transport.counts.escapes;
      hr = gpu->importNativeSurface(surface->handle(), surface->surface().key,
        surface->surface().size, reopenedOwner, &reopenedImport);
      if (hr == S_OK && (reopenedImport.token != imported.token
          || reopenedImport.address != imported.address || transport.counts.escapes != before)) hr = E_FAIL;
      if (hr == S_OK) hr = gpu->revokeNativeSurface(surface);
      std::printf("NATIVE_SURFACE_REOPEN hr=%08lx canonical_token=%p first_registration_revoked=1 same_kmt_handle=1\n",
        (unsigned long)hr, reopenedImport.token);
    }
    if (hr == S_OK && embedded) {
      hr = gpu->importNativeSurface(unused->handle(), unused->surface().key,
        unused->surface().size, unused, &unusedImport);
      std::printf("NATIVE_SURFACE_UNUSED_IMPORT hr=%08lx token=%p key=%llu\n", (unsigned long)hr,
        unusedImport.token, (unsigned long long)unused->surface().key);
    }
    if (hr == S_OK && layout) {
      NativeVulkanImage image;
      hr = image.layout(*gpu, luid, width, height, surface->surface().stride,
        surface->surface().size, imported, candidate);
      if (hr == S_OK && pixels) hr = image.exercise(width, height);
    }
    // Keep all runtime callbacks on the transport's original thread while
    // the backend records/submits on its normal workers. Local backend COM
    // owners are destroyed inside this pump as well, before it returns.
    if (hr == S_OK && embedded) hr = service->run([&] {
      return exerciseNativeD3D(luid, width, height, surface->surface().stride,
        gpu->backend(), imported, unusedImport, unused->surface().stride, rotate);
    });
    if (unusedImport.token) {
      auto bridge = gpu->backend();
      const HRESULT released = bridge.create.callbacks->release(bridge.create.owner, unusedImport.token);
      std::printf("NATIVE_SURFACE_UNUSED_RELEASE hr=%08lx\n", (unsigned long)released);
      if (hr == S_OK) hr = released;
    }
    if (imported.token) {
      auto bridge = gpu->backend();
      const HRESULT released = bridge.create.callbacks->release(bridge.create.owner, imported.token);
      std::printf("NATIVE_SURFACE_RELEASE hr=%08lx\n", (unsigned long)released);
      if (hr == S_OK) hr = released;
    }
    if (reopenedImport.token) {
      auto bridge = gpu->backend();
      const HRESULT released = bridge.create.callbacks->release(bridge.create.owner, reopenedImport.token);
      std::printf("NATIVE_SURFACE_REOPEN_RELEASE hr=%08lx\n", (unsigned long)released);
      if (hr == S_OK) hr = released;
    }
  }
  auto keep = [&](HRESULT status) { if (hr == S_OK && status != S_OK) hr = status; };
  if (gpu) keep(gpu->close());
  gpu.reset();
  reopenedOwner.reset();
  const bool leaseRetired = surface.use_count() == 1 && unused.use_count() == 1;
  if (!leaseRetired) keep(E_FAIL);
  if (leaseRetired) keep(surface->close());
  if (leaseRetired) keep(unused->close());
  surface.reset();
  unused.reset();
  if (runtimePaging) {
    paging.print();
    if (!paging.balanced()) keep(E_FAIL);
    keep(paging.close());
  }
  const auto& c = transport.counts;
  if (hr == S_OK && (!c.queries || c.contexts != 1 || c.contextCloses != 1
      || c.escapes < 3 || c.allocations != c.deallocations || c.locks != c.unlocks
      || (!layout && !embedded && (c.escapes != 3 || c.allocations || c.locks))
      || (!pixels && (c.renders || c.residents || c.evictions))
      || (pixels && ((!embedded && (c.renders != 2 || c.nativeReads != 2 || c.nativeWrites != 2))
          || (embedded && (c.renders < 2 || c.nativeReads < 2 || c.nativeWrites < 2))
          || !c.residents || c.residents != c.evictions))
      || c.wrongThreads || c.badCookies
      || c.coreErrors || c.malformedOutputs)) keep(E_FAIL);
  keep(transport.close());
  transport.printCounts();
  std::printf("NATIVE_SURFACE_LIFECYCLE %s hr=%08lx borrowed_lease_retired=%u"
    " gpu_submission=%u pixel_validation=%u present=0\n", hr == S_OK ? "PASS" : "FAIL",
    (unsigned long)hr, unsigned(leaseRetired), c.renders, unsigned(pixels && hr == S_OK));
  return hr == S_OK ? 0 : 1;
}
