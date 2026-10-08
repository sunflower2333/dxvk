#pragma once

#include <cstdint>
#include <cstdio>

namespace dxvk::umd {

// Only scalar snapshots enter this optional trace. It never reads a borrowed
// request, changes an HRESULT, or calls the runtime from its output path.
struct RuntimeGpuDiagnosticInfo {
  uint64_t generation = 0;
  uint32_t context = 0, queue = 0, references = 0, streamBytes = 0;
  uint32_t lockedReferences = 0, index = UINT32_MAX;
};

class RuntimeGpuDiagnostics {
public:
  enum class Event { ContextReady, SubmitEntered, SubmitSucceeded, Failure };
  explicit RuntimeGpuDiagnostics(bool enabled = false) : m_enabled(enabled) { }
  bool enabled() const { return m_enabled; }
  static bool enabledFlag(const char* flag) {
    return flag && flag[0] == '1' && !flag[1];
  }

  // RuntimeGpu serializes this state with its existing callback mutex.
  // Retain one record of each kind per owner, even when failure causes a
  // completion worker to poll repeatedly during cleanup.
  bool record(FILE* output, Event event, const char* stage, int32_t status,
              const RuntimeGpuDiagnosticInfo& info, bool callback = false,
              int32_t callbackStatus = 0) noexcept {
    const uint32_t bit = 1u << uint32_t(event);
    if (!m_enabled || !output || (m_recorded & bit)) return false;
    m_recorded |= bit;
    const char* name = event == Event::Failure ? "failure"
      : event == Event::ContextReady ? "context-ready"
      : event == Event::SubmitEntered ? "submit-entered" : "submit-succeeded";
    std::fprintf(output,
      "VIOGPU_RUNTIME_GPU event=%s stage=%s hr=%08x callback=%u callback_hr=%08x"
      " generation=%llu context=%u queue=%u references=%u stream_bytes=%u"
      " locked_references=%u index=%u\n",
      name, stage, unsigned(uint32_t(status)), unsigned(callback),
      unsigned(uint32_t(callbackStatus)), static_cast<unsigned long long>(info.generation),
      unsigned(info.context), unsigned(info.queue), unsigned(info.references),
      unsigned(info.streamBytes), unsigned(info.lockedReferences), unsigned(info.index));
    return true;
  }

private:
  bool m_enabled;
  uint32_t m_recorded = 0;
};

}
