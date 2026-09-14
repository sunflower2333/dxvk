#pragma once

#include <atomic>
#include <cstdint>

namespace dxvk {

/**
 * Lightweight opt-in counters for DroidVM submit-path investigation.
 *
 * This intentionally does not affect Vulkan submission behavior. Call sites
 * can increment counters around existing queue submission boundaries without
 * adding per-draw logging overhead.
 */
struct DroidVmPerfStats {
  std::atomic<uint64_t> submitCount { 0 };
  std::atomic<uint64_t> commandBufferCount { 0 };
  std::atomic<uint64_t> waitSemaphoreCount { 0 };
  std::atomic<uint64_t> signalSemaphoreCount { 0 };

  void reset() {
    submitCount.store(0, std::memory_order_relaxed);
    commandBufferCount.store(0, std::memory_order_relaxed);
    waitSemaphoreCount.store(0, std::memory_order_relaxed);
    signalSemaphoreCount.store(0, std::memory_order_relaxed);
  }
};

}
