#pragma once

#include "umd_identity.h"
#include "umd_runtime_bridge.h"
#include <cstring>

namespace dxvk::umd {

inline bool validRuntimeBackend(const RuntimeBackend& runtime) {
  return runtime.owner && runtime.create.owner == runtime.owner.get()
    && runtime.create.sType == MWD_STYPE_DEVICE && !runtime.create.pNext
    && mwd_callbacks_valid(runtime.create.callbacks);
}

inline bool supportsRuntimeBackend(const mwd_support& support) {
  return support.magic == MWD_RUNTIME_MAGIC
    && support.version == MWD_RUNTIME_ABI_VERSION
    && support.size == sizeof(mwd_callbacks) && support.flags == 1;
}

inline bool matchesRuntimeContext(const AdapterLuid& luid, const mwd_context_info& info) {
  return luid != AdapterLuid{} && !std::memcmp(info.luid, luid.data(), luid.size())
    && info.generation && info.context_id && info.queue_id;
}

// The descriptor and callback table may be changed by a reentrant callback.
// Keep their values and owner stable until the ICD has copied the table.
class RuntimeBackendSnapshot {
public:
  explicit RuntimeBackendSnapshot(const RuntimeBackend* runtime) : m_supplied(runtime != nullptr) {
    if (runtime) {
      m_runtime = *runtime;
      m_callbacks = *runtime->create.callbacks;
      m_runtime.create.callbacks = &m_callbacks;
    }
  }
  RuntimeBackendSnapshot(const RuntimeBackendSnapshot&) = delete;
  RuntimeBackendSnapshot& operator=(const RuntimeBackendSnapshot&) = delete;
  const RuntimeBackend* get() const { return m_supplied ? &m_runtime : nullptr; }

private:
  RuntimeBackend m_runtime;
  mwd_callbacks m_callbacks = {};
  bool m_supplied;
};

}
