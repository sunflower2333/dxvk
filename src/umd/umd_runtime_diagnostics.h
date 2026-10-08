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
  uint32_t allocations = 0, lockedAllocations = 0, activeCalls = 0;
};

struct RuntimeGpuRenderReferenceDiagnostic {
  uint32_t allocationHandle = 0, writeOperation = 0;
  uint32_t allocationIndex = 0, allocationOffset = 0;
  uint32_t patchOffset = 0, relativePatchOffset = 0;
  uint64_t slot = 0, boPresumed = 0;
  uint32_t boPresumedOffset = 0;
  bool boPresumedAvailable = false;
};

class RuntimeGpuDiagnostics {
public:
  enum class Event {
    ContextReady, SubmitEntered, SubmitSucceeded, Failure,
    CloseEntered, AllocationCleanupFinished, DestroyContextEntered,
    DestroyContextFinished, CloseFinished, RenderBefore, RenderAfter
  };
  static constexpr uint32_t RenderReferenceLimit = 8;
  explicit RuntimeGpuDiagnostics(bool enabled = false) : m_enabled(enabled) { }
  bool enabled() const { return m_enabled; }
  bool wantsRenderSnapshot() const {
    return m_enabled && !(m_recorded & (1u << uint32_t(Event::RenderBefore)));
  }
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
    const char* name = "";
    switch (event) {
      case Event::ContextReady: name = "context-ready"; break;
      case Event::SubmitEntered: name = "submit-entered"; break;
      case Event::SubmitSucceeded: name = "submit-succeeded"; break;
      case Event::Failure: name = "failure"; break;
      case Event::CloseEntered: name = "close-entered"; break;
      case Event::AllocationCleanupFinished: name = "allocation-cleanup-finished"; break;
      case Event::DestroyContextEntered: name = "destroy-context-entered"; break;
      case Event::DestroyContextFinished: name = "destroy-context-finished"; break;
      case Event::CloseFinished: name = "close-finished"; break;
      case Event::RenderBefore: name = "render-before"; break;
      case Event::RenderAfter: name = "render-after"; break;
    }
    std::fprintf(output,
      "VIOGPU_RUNTIME_GPU event=%s stage=%s hr=%08x callback=%u callback_hr=%08x"
      " generation=%llu context=%u queue=%u references=%u stream_bytes=%u"
      " locked_references=%u index=%u allocations=%u locked_allocations=%u active_calls=%u\n",
      name, stage, unsigned(uint32_t(status)), unsigned(callback),
      unsigned(uint32_t(callbackStatus)), static_cast<unsigned long long>(info.generation),
      unsigned(info.context), unsigned(info.queue), unsigned(info.references),
      unsigned(info.streamBytes), unsigned(info.lockedReferences), unsigned(info.index),
      unsigned(info.allocations), unsigned(info.lockedAllocations), unsigned(info.activeCalls));
    return true;
  }

  // First callback only, at most eight scalar rows. The caller snapshots these
  // before RenderCb can replace its borrowed command and list buffers.
  bool recordRenderBefore(FILE* output, const RuntimeGpuDiagnosticInfo& info,
                          const RuntimeGpuRenderReferenceDiagnostic* references,
                          uint32_t count) noexcept {
    if (!references || !record(output, Event::RenderBefore, "render-callback", 0, info))
      return false;
    const uint32_t captured = count < RenderReferenceLimit ? count : RenderReferenceLimit;
    std::fprintf(output, "VIOGPU_RUNTIME_GPU_RENDER captured=%u omitted=%u\n",
      unsigned(captured), unsigned(count - captured));
    for (uint32_t i = 0; i < captured; ++i) {
      const auto& reference = references[i];
      std::fprintf(output,
        "VIOGPU_RUNTIME_GPU_RENDER index=%u allocation_handle=%08x write_operation=%u"
        " allocation_index=%u allocation_offset=%u patch_offset=%u relative_patch_offset=%u"
        " slot_u64=%016llx bo_presumed_available=%u bo_presumed_offset=%u bo_presumed=%016llx\n",
        unsigned(i), unsigned(reference.allocationHandle), unsigned(reference.writeOperation),
        unsigned(reference.allocationIndex), unsigned(reference.allocationOffset),
        unsigned(reference.patchOffset), unsigned(reference.relativePatchOffset),
        static_cast<unsigned long long>(reference.slot), unsigned(reference.boPresumedAvailable),
        unsigned(reference.boPresumedOffset), static_cast<unsigned long long>(reference.boPresumed));
    }
    return true;
  }

private:
  bool m_enabled;
  uint32_t m_recorded = 0;
};

}
