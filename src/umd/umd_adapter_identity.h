#pragma once

#include "umd_runtime_query.h"
#include "umd_legacy_api.h"
#include "umd_primary_policy.h"
#include <memory>
#include <atomic>

namespace dxvk::umd {

// The D3D10+ runtime's private-info callback is an initialization service.
// Continuity checks use a separately owned kernel adapter for the same LUID.
// Devices retain this source until their final allocation/context cleanup.
class AdapterIdentitySource {
public:
  virtual ~AdapterIdentitySource() = default;
  virtual HRESULT query(RuntimeIdentity& result, RuntimeQueryStage stage) const = 0;
  // Kernel driver model, independent of the negotiated D3D API/DDI revision.
  virtual bool explicitResidency() const { return false; }
  // Adapter-only Native AHB transactions never use a runtime device cookie
  // or substitute a KMT handle for one. The kernel source owns this adapter.
  virtual HRESULT nativeEscape(void*, UINT) const { return DXGI_ERROR_UNSUPPORTED; }
  // Query the available modes of this exact adapter/source. An optional
  // primary keeps NO_SCANOUT unless this returns exact success.
  virtual HRESULT supportsPrimaryMode(const PrimaryShape&) const { return DXGI_ERROR_UNSUPPORTED; }
};

HRESULT openKernelIdentity(const RuntimeIdentity& initial,
  std::shared_ptr<const AdapterIdentitySource>& source);

// Shared by D3D9 and D3D10 devices; the runtime handle is opaque and is
// returned only to the query and escape callbacks which supplied it.
struct AdapterIdentity {
  // Immutable API identity for the legacy8/9 DDI. Default9 preserves existing
  // direct-core callers; D3D10+ uses its own interface contract and ignores it.
  explicit AdapterIdentity(LegacyD3DApi api = LegacyD3DApi::D3D9) : legacyApi(api) { }
  const LegacyD3DApi legacyApi;
  LUID luid = {};
  HANDLE runtime = nullptr;
  PFND3DDDI_QUERYADAPTERINFOCB query = nullptr;
  uint64_t generation = 0;
  uint64_t capabilities = 0;
  std::shared_ptr<const AdapterIdentitySource> continuity;
  // Optional lifetime gate for adapters whose opaque handles can expire
  // during a callback. Device-only cleanup must still release its backing.
  std::shared_ptr<const std::atomic<bool>> live;
  bool available() const { return !live || live->load(); }
  bool explicitResidency() const { return continuity && continuity->explicitResidency(); }
  HRESULT queryCurrent(RuntimeIdentity& result,
      RuntimeQueryStage stage = RuntimeQueryStage::Continuity) const {
    return continuity ? continuity->query(result, stage)
      : queryRuntimeIdentity(runtime, query, result, stage);
  }
};

inline void copyResidencyCallbacks(const AdapterIdentity& identity,
    const D3DDDI_DEVICECALLBACKS& source, D3DDDI_DEVICECALLBACKS& target) {
  // WDDM2 runtime kernel callbacks are also required by historical D3D8/9/11
  // devices on a WDDM2 physical engine. Never inspect this tail on WDDM1 or
  // standalone historical-ABI callers. Query the exact adapter, not the OS
  // version or another adapter discovered through enumeration.
  if (!identity.explicitResidency()) return;
  target.pfnMakeResidentCb = source.pfnMakeResidentCb;
  target.pfnEvictCb = source.pfnEvictCb;
  target.pfnCreatePagingQueueCb = source.pfnCreatePagingQueueCb;
  target.pfnDestroyPagingQueueCb = source.pfnDestroyPagingQueueCb;
  target.pfnWaitForSynchronizationObjectFromCpuCb = source.pfnWaitForSynchronizationObjectFromCpuCb;
  target.pfnWaitForSynchronizationObjectFromGpuCb = source.pfnWaitForSynchronizationObjectFromGpuCb;
  target.pfnSignalSynchronizationObject2Cb = source.pfnSignalSynchronizationObject2Cb;
}

}
