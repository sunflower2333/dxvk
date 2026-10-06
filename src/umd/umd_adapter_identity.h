#pragma once

#include "umd_runtime_query.h"
#include <memory>
#include <atomic>

namespace dxvk::umd {

// Shared by D3D9 and D3D10 devices; the runtime handle is opaque and is
// returned only to the query and escape callbacks which supplied it.
struct AdapterIdentity {
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
