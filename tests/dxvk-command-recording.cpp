// Actual recording and inline CS bodies; controlled Vulkan return values.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "../src/util/util_likely.h"
#include "../src/util/rc/util_rc_ptr.h"
#include "../src/util/util_flags.h"

#define CACHE_LINE_SIZE 64
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error(#x); } while (0)
namespace dxvk {
using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;
using condition_variable = std::condition_variable;
using high_resolution_clock = std::chrono::steady_clock;
using thread = std::thread;
template<typename T> T align(T value, T alignment) { return (value+alignment-1)&~(alignment-1); }
struct RcObject {
  std::atomic<unsigned> refs{0};
  unsigned incRef() { return ++refs; }
  unsigned decRef() { return --refs; }
};
struct DxvkError : std::runtime_error {
  using std::runtime_error::runtime_error;
  std::string message() const { return what(); }
};
struct Logger { static void err(const std::string&) {} };
namespace env { void setThreadName(const char*) {} }
enum class DxvkStatCounter { CsChunkCount, CsSyncCount, CsSyncTicks, CsIdleTicks, CmdBarrierCount };
enum class DxvkDebugFlag { Capture };
enum class DxvkCmdBuffer { ExecBuffer, InitBuffer, InitBarriers, SdmaBuffer, SdmaBarriers, Count };
namespace vk {
VkDebugUtilsLabelEXT makeLabel(uint32_t, const char* name) {
  VkDebugUtilsLabelEXT result{}; result.pLabelName = name; return result;
}
}
struct VulkanPeer {
  std::string fail;
  VkResult error = VK_ERROR_DEVICE_LOST;
  unsigned created = 0, destroyed = 0, allocated = 0, begun = 0, ended = 0, reset = 0, barriers = 0;
  VkDevice device() { return reinterpret_cast<VkDevice>(1); }
  VkResult vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool* p) {
    if (fail == "create") return error;
    ++created; *p = reinterpret_cast<VkCommandPool>(2); return VK_SUCCESS;
  }
  void vkDestroyCommandPool(VkDevice, VkCommandPool p, const void*) { CHECK(p); ++destroyed; }
  VkResult vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* p) {
    if (fail == (info->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? "allocate" : "secondary-allocate")) return error;
    *p = reinterpret_cast<VkCommandBuffer>(0x100 + ++allocated); return VK_SUCCESS;
  }
  VkResult vkBeginCommandBuffer(VkCommandBuffer p, const VkCommandBufferBeginInfo* info) {
    CHECK(p);
    if (fail == (info->pInheritanceInfo ? "secondary-begin" : "begin")) return error;
    ++begun; return VK_SUCCESS;
  }
  VkResult vkEndCommandBuffer(VkCommandBuffer p) {
    CHECK(p); if (fail == "end") return error;
    ++ended; return VK_SUCCESS;
  }
  VkResult vkResetCommandPool(VkDevice, VkCommandPool p, unsigned) {
    CHECK(p); if (fail == "reset") return error;
    ++reset; return VK_SUCCESS;
  }
  void vkCmdPipelineBarrier2(VkCommandBuffer p, const VkDependencyInfo*) { CHECK(p); ++barriers; }
  void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer, const VkDebugUtilsLabelEXT*) {}
  void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer) {}
};
struct DxvkDevice : RcObject {
  VulkanPeer vk;
  struct Submission {
    std::atomic<VkResult> m_lastError{VK_SUCCESS};
    bool m_inline = true;
    std::recursive_mutex m_inlineMutex;
    std::mutex m_mutex;
    std::condition_variable_any m_inlineCond;
    std::condition_variable m_submitCond, m_finishCond;
    unsigned synchronizations = 0;
    VkResult getLastError() const { return m_lastError.load(); }
    void synchronize() { ++synchronizations; }
#include "queue_error.h"
  } m_submissionQueue;
  struct DebugFlags { bool test(DxvkDebugFlag) const { return false; } };
  DebugFlags debugFlags() const { return {}; }
  VulkanPeer* vkd() { return &vk; }
  VulkanPeer* vki() { return &vk; }
  bool disableExtraThreads() const { return true; }
  bool canUseDescriptorHeap() const { return false; }
  bool canUseDescriptorBuffer() const { return false; }
  void addStatCtr(DxvkStatCounter, uint64_t) {}
#include "device.h"
};
#include "pool.h"
struct DxvkCommandSubmissionInfo {
  std::array<VkCommandBuffer, uint32_t(DxvkCmdBuffer::Count)> cmdBuffers{};
  bool execCommands = false;
};
struct PresenterSync {};
struct DxvkCommandList : RcObject {
  DxvkDevice* m_device;
  VulkanPeer *m_vkd, *m_vki;
  Rc<DxvkCommandPool> m_graphicsPool, m_transferPool;
  DxvkCommandSubmissionInfo m_cmd;
  bool m_checkpoints = false;
  struct Stats { void addCtr(DxvkStatCounter, unsigned) {} void reset() {} } m_statCounters;
  bool m_descriptorHeapInvalidated = false;
  std::vector<std::shared_ptr<void>> m_objectTracker;
  struct Signals { void reset() {} } m_signalTracker;
  struct DescriptorPool : RcObject { void notifyCompletion(uint64_t) {} };
  Rc<DescriptorPool> m_descriptorPool;
  uint64_t m_trackingId = 0;
  struct Pipeline { void releasePipeline() {} };
  std::vector<Pipeline*> m_pipelines;
  std::vector<unsigned> m_waitSemaphores, m_signalSemaphores, m_cmdSubmissions, m_cmdSparseBinds;
  PresenterSync m_wsiSemaphores;
  explicit DxvkCommandList(DxvkDevice* d) : m_device(d), m_vkd(d->vkd()), m_vki(d->vki()),
    m_graphicsPool(new DxvkCommandPool(d, 0)), m_transferPool(m_graphicsPool) {}
  ~DxvkCommandList();
  void reset();
  void resetCheckpoints() {}
  void init();
  void endCommandBuffer(VkCommandBuffer);
  VkCommandBuffer allocateCommandBuffer(DxvkCmdBuffer);
  void debugBarrier(DxvkCmdBuffer, const VkDependencyInfo*) {}
  void bindSamplerHeap(VkCommandBuffer) {}
  void bindResourceHeap(VkCommandBuffer) {}
  void bindDescriptorBuffers(VkCommandBuffer) {}
#include "barrier.h"
};
struct DxvkContext : RcObject {
  Rc<DxvkDevice> m_device;
  Rc<DxvkCommandList> m_cmd;
  uint64_t m_trackingId = 0;
  std::pair<unsigned, unsigned> m_framesToCapture{0, 0};
  explicit DxvkContext(const Rc<DxvkDevice>& d) : m_device(d) {}
  void beginFrameCapture() {}
  void beginCurrentCommands() { CHECK(m_cmd && m_cmd->getCmdBuffer(DxvkCmdBuffer::ExecBuffer)); }
  void addStatCtr(DxvkStatCounter, uint64_t) {}
  void beginRecording(const Rc<DxvkCommandList>&);
};
}
#include "command.cpp"
#include "context.cpp"
#include "cs.h"
#include "cs.cpp"
using namespace dxvk;
struct Owner { unsigned* released; explicit Owner(unsigned* p) : released(p) {} ~Owner() { ++*released; } };
struct Harness {
  Rc<DxvkDevice> device = new DxvkDevice;
  Rc<DxvkContext> context = new DxvkContext(device);
  DxvkCsChunkPool pool;
  DxvkCsThread cs{device, context};
  DxvkCsChunkRef chunk() { return {pool.allocChunk(DxvkCsChunkFlags(DxvkCsChunkFlag::SingleUse)), &pool}; }
};
void healthy() {
  Harness h;
  h.context->beginRecording(new DxvkCommandList(h.device.ptr()));
  auto list = h.context->m_cmd;
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  VkCommandBufferInheritanceInfo inheritance{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
  list->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &dependency);
  auto secondary = list->m_graphicsPool->getSecondaryCommandBuffer(inheritance);
  list->endCommandBuffer(secondary);
  list->endCommandBuffer(list->getCmdBuffer(DxvkCmdBuffer::ExecBuffer));
  list->m_graphicsPool->reset();
  list->init();
  CHECK(h.device->vk.allocated == 2 && h.device->vk.begun == 3);
  CHECK(h.device->vk.reset == 1 && h.device->vk.barriers == 1);
  CHECK(h.device->getDeviceStatus() == VK_SUCCESS);
  CHECK(h.device->synchronizeSubmission() == VK_SUCCESS);
}
void failure(const std::string& which, VkResult error) {
  Harness h; unsigned released = 0, later = 0;
  bool initial = which == "create" || which == "allocate" || which == "begin";
  if (!initial) h.context->beginRecording(new DxvkCommandList(h.device.ptr()));
  h.device->vk.fail = which; h.device->vk.error = error;
  auto c = h.chunk();
  c->push([&](DxvkContext* context) {
    if (initial) context->beginRecording(new DxvkCommandList(h.device.ptr()));
    else if (which == "reset") context->m_cmd->m_graphicsPool->reset();
    else if (which == "end") context->m_cmd->endCommandBuffer(context->m_cmd->getCmdBuffer(DxvkCmdBuffer::ExecBuffer));
    else {
      VkCommandBufferInheritanceInfo inheritance{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
      context->m_cmd->m_graphicsPool->getSecondaryCommandBuffer(inheritance);
    }
  });
  c->push([owner=std::make_shared<Owner>(&released), &later](DxvkContext*) { ++later; });
  bool escaped = false;
  try { h.cs.dispatchChunk(std::move(c)); } catch (const DxvkError&) { escaped = true; }
  std::cout << "injected=" << error << " observed=" << h.device->getDeviceStatus() << " escaped=" << escaped << '\n';
  CHECK(h.device->getDeviceStatus() == error);
  CHECK(h.device->synchronizeSubmission() == error);
  CHECK(!escaped && !c && released == 1 && later == 0);
  if (initial) CHECK(!h.context->m_cmd);
  for (auto queue : {DxvkCsQueue::Ordered, DxvkCsQueue::HighPriority}) {
    c = h.chunk();
    c->push([owner=std::make_shared<Owner>(&released), &later](DxvkContext* context) {
      ++later; VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
      context->m_cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &dependency);
    });
    h.cs.injectChunk(queue, std::move(c), true); CHECK(!c);
  }
  h.cs.synchronize(DxvkCsThread::SynchronizeAll);
  CHECK(released == 3 && later == 0 && h.device->vk.barriers == 0);
}
void publication() {
  Harness h; h.device->vk.fail = "begin";
  bool caught = false;
  try { h.context->beginRecording(new DxvkCommandList(h.device.ptr())); }
  catch (const DxvkError&) { caught = true; }
  CHECK(caught); CHECK(!h.context->m_cmd); CHECK(h.context->m_trackingId == 0);
}
void retire() {
  Harness h; unsigned released = 0;
  {
    Rc<DxvkCommandList> list = new DxvkCommandList(h.device.ptr());
    list->init();
    list->m_objectTracker.push_back(std::make_shared<Owner>(&released));
    h.device->vk.fail = "reset";
    bool escaped = false;
    try { list->reset(); } catch (const DxvkError&) { escaped = true; }
    CHECK(!escaped && released == 1);
    CHECK(h.device->getDeviceStatus() == VK_ERROR_DEVICE_LOST);
  }
  CHECK(released == 1 && h.device->vk.destroyed == 1);
}
int main(int argc, char** argv) {
  try {
    std::string name = argc > 1 ? argv[1] : "healthy";
    if (name == "healthy") healthy();
    else if (name == "publish") publication();
    else if (name == "retire") retire();
    else for (auto error : {VK_ERROR_DEVICE_LOST, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) failure(name, error);
    std::cout << "PASS " << name << " checks=" << checks << '\n'; return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL checks=" << checks << ' ' << e.what() << '\n'; return 1;
  }
}
