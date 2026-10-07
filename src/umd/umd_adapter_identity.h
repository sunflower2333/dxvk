#pragma once

#include "umd_runtime_query.h"
#include "umd_legacy_api.h"
#include <memory>
#include <atomic>

namespace dxvk::umd {

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
  // Optional lifetime gate for adapters whose opaque handles can expire
  // during a callback. Device-only cleanup must still release its backing.
  std::shared_ptr<const std::atomic<bool>> live;
  bool available() const { return !live || live->load(); }
};

}
