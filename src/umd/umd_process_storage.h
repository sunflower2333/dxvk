#pragma once

namespace dxvk::umd {

// DllMain sets this only for DLL_PROCESS_DETACH with a non-null reserved
// argument, before the CRT runs global destructors. Other threads have already
// terminated; they may have left locks held and cannot execute cleanup work.
inline bool processTerminating = false;

// Keep registry-owned graphs intact during process termination. Skipping only
// an outer cleanup guard would still let the registry's destructor release
// shared owners and enter runtime callbacks or worker waits. No allocation,
// lock or reference-count operation is needed to abandon this inline storage.
// Ordinary FreeLibrary and explicit DDI destruction retain normal ownership.
template<typename T>
class ProcessStorage final {
public:
  ProcessStorage() : m_value() { }
  ~ProcessStorage() { if (!processTerminating) m_value.~T(); }
  ProcessStorage(const ProcessStorage&) = delete;
  ProcessStorage& operator=(const ProcessStorage&) = delete;
  T& get() noexcept { return m_value; }
private:
  union { T m_value; };
};

}
