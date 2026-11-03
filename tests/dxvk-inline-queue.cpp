// Controlled Vulkan/command-list peers around the actual dxvk_queue.h/.cpp.
// This verifies CPU queue ordering and ownership, not hardware completion.
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static std::atomic<unsigned> checks{0};
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error(#x); } while (0)
using VkResult = int;
using VkSemaphore = uint64_t;
constexpr VkResult VK_SUCCESS = 0, VK_NOT_READY = 1, VK_TIMEOUT = 2;
constexpr VkResult VK_ERROR_DEVICE_LOST = -4, VK_ERROR_OUT_OF_HOST_MEMORY = -1;
constexpr int VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO = 1;
constexpr int VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 2;
constexpr int VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO = 3;
constexpr int VK_SEMAPHORE_TYPE_TIMELINE = 1;
struct VkSemaphoreTypeCreateInfo {int sType; int semaphoreType;};
struct VkSemaphoreCreateInfo {int sType; const void* pNext;};
struct VkSemaphoreWaitInfo {int sType; uint32_t semaphoreCount; const VkSemaphore* pSemaphores; const uint64_t* pValues;};
struct VkRectLayerKHR {};

namespace dxvk {
using mutex = std::mutex;
using recursive_mutex = std::recursive_mutex;
using condition_variable = std::condition_variable;
using high_resolution_clock = std::chrono::steady_clock;
template<typename T> using Rc = std::shared_ptr<T>;
template<typename T, size_t N> using small_vector = std::vector<T>;
using DxvkQueueCallback = std::function<void(bool)>;
struct DxvkTimelineSemaphores {VkSemaphore graphics = 0, transfer = 0;};
struct DxvkTimelineSemaphoreValues {uint64_t graphics = 0, transfer = 0;};
constexpr size_t MaxNumQueuedCommandBuffers = 32;
struct DxvkError : std::runtime_error {using std::runtime_error::runtime_error;};
namespace str {template<typename... T> std::string format(const char* s, T...) {return s;}}
struct Logger {static void err(const std::string&) {} static void warn(const std::string&) {}};
namespace env {inline void setThreadName(const char*) {}}
struct thread : std::thread {
  inline static std::atomic<unsigned> created{0};
  thread() = default;
  template<typename F> thread(F f) : std::thread(std::move(f)) {++created;}
  thread(thread&&) = default;
  thread& operator=(thread&&) = default;
};
struct DxvkCheckpointBuffer {void printHangInfo() {}};
struct DxvkLatencyTracker {
  unsigned begin = 0, end = 0;
  void notifyQueueSubmit(uint64_t) {}
  void notifyQueuePresentBegin(uint64_t) {}
  void notifyQueuePresentEnd(uint64_t, VkResult) {}
  void notifyGpuExecutionBegin(uint64_t) {++begin;}
  void notifyGpuExecutionEnd(uint64_t) {++end;}
};
struct Backend {
  std::mutex mutex;
  uint64_t created = 0, graphics = 0, transfer = 0;
  unsigned blocking = 0, polling = 0, destroyed = 0;
  VkResult nextWait = VK_SUCCESS;
  bool lost = false;
  void* device() {return this;}
  VkResult vkCreateSemaphore(void*, const VkSemaphoreCreateInfo*, void*, VkSemaphore* out) {*out=++created;return VK_SUCCESS;}
  void vkDestroySemaphore(void*, VkSemaphore, void*) {++destroyed;}
  VkResult vkWaitSemaphores(void*, const VkSemaphoreWaitInfo* info, uint64_t timeout) {
    std::lock_guard<std::mutex> lock(mutex);
    if (timeout) ++blocking; else ++polling;
    VkResult error = std::exchange(nextWait, VK_SUCCESS);
    if (error != VK_SUCCESS) {lost = error == VK_ERROR_DEVICE_LOST;return error;}
    if (lost) return VK_ERROR_DEVICE_LOST;
    CHECK(info->semaphoreCount == 2);
    if (graphics >= info->pValues[0] && transfer >= info->pValues[1]) return VK_SUCCESS;
    if (!timeout) return VK_TIMEOUT;
    // A controlled infinite wait completes exactly its requested timelines.
    graphics = std::max(graphics, info->pValues[0]);
    transfer = std::max(transfer, info->pValues[1]);
    return VK_SUCCESS;
  }
  void complete(uint64_t g, uint64_t t) {std::lock_guard<std::mutex> lock(mutex);graphics=g;transfer=t;}
};
struct Counts {std::atomic<unsigned> notify{0}, reset{0}, destroyed{0};};
struct DxvkCommandList {
  Backend* backend;
  std::shared_ptr<Counts> counts;
  uint64_t graphics = 0, transfer = 0;
  VkResult submitResult = VK_SUCCESS;
  std::function<void()> resetCallback;
  DxvkCommandList(Backend* b, std::shared_ptr<Counts> c) : backend(b), counts(c) {}
  ~DxvkCommandList() {++counts->destroyed;}
  VkResult submit(const DxvkTimelineSemaphores&, DxvkTimelineSemaphoreValues& timelines, uint64_t) {
    graphics = ++timelines.graphics;transfer = ++timelines.transfer;return submitResult;
  }
  void notifyObjects() {
    {std::lock_guard<std::mutex> lock(backend->mutex);
     CHECK(backend->lost || (backend->graphics >= graphics && backend->transfer >= transfer));}
    CHECK(++counts->notify == 1);
  }
  void reset() {CHECK(counts->notify == 1);CHECK(++counts->reset == 1);if(resetCallback)resetCallback();}
};
struct Presenter {
  std::function<void()> signal;
  VkResult presentImage(uint64_t, const Rc<DxvkLatencyTracker>&, size_t, const VkRectLayerKHR*) {return VK_SUCCESS;}
  void signalFrame(uint64_t, const Rc<DxvkLatencyTracker>&) {signal();}
};
class DxvkSubmissionQueue;
#ifdef DXVK_INLINE_FENCE_CLIENTS
namespace sync { class Fence; using Spinlock = std::mutex; }
enum class DxvkStatCounter {GpuSyncCount, GpuSyncTicks};
#endif
struct DxvkDevice {
  bool inlineWork = true;
  Backend backend;
  DxvkSubmissionQueue* queue = nullptr;
  std::atomic<unsigned> recycled{0}, idles{0};
  struct Manager {void performTimedTasks() {}};
  struct Objects {Manager manager;Manager& memoryManager() {return manager;}} m_objects;
  bool disableExtraThreads() const {return inlineWork;}
  Backend* vkd() {return &backend;}
  DxvkCheckpointBuffer* getCheckpointBuffer() {return nullptr;}
  void recycleCommandList(const Rc<DxvkCommandList>&) {++recycled;}
  void waitForIdle();
#ifdef DXVK_INLINE_FENCE_CLIENTS
  const bool& m_disableExtraThreads = inlineWork;
  struct QueueAccess {
    DxvkDevice* device;
    template<typename F> void synchronizeUntil(const F& pred);
  } m_submissionQueue{this};
  struct Stats {void addCtr(DxvkStatCounter, uint64_t) {}} m_statCounters;
  sync::Spinlock m_statLock;
  std::atomic<uint32_t> samplerLive{0};
  struct SamplerStats {uint32_t liveCount;};
  SamplerStats getSamplerStats() {return {samplerLive.load()};}
  void waitForFence(sync::Fence&, uint64_t);
#endif
};
}
#include "dxvk_queue_under_test.h"
#include "dxvk_queue_under_test.cpp"
namespace dxvk {
void DxvkDevice::waitForIdle() {++idles;queue->waitForIdle();backend.complete(UINT64_MAX, UINT64_MAX);}
#ifdef DXVK_INLINE_FENCE_CLIENTS
template<typename F> void DxvkDevice::QueueAccess::synchronizeUntil(const F& pred) {
  device->queue->synchronizeUntil(pred);
}
#endif
}
using namespace dxvk;

static std::shared_ptr<Counts> submit(DxvkDevice& d, DxvkSubmissionQueue& q,
    std::function<void()> callback = {}, Rc<DxvkLatencyTracker> tracker = {}) {
  auto counts = std::make_shared<Counts>();
  auto command = std::make_shared<DxvkCommandList>(&d.backend, counts);
  command->resetCallback = std::move(callback);
  DxvkSubmitStatus status;status.result = VK_NOT_READY;
  q.submit({command}, {tracker, 1}, &status);
  CHECK(status.result == VK_SUCCESS);
  return counts;
}
static void deferred_and_both_timelines() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto tracker=std::make_shared<DxvkLatencyTracker>();auto c=submit(d,q,{},tracker);
  CHECK(d.backend.blocking == 0);CHECK(c->notify == 0);CHECK(c->destroyed == 0);
  CHECK(tracker->begin == 0 && tracker->end == 0);
  d.backend.complete(1,0);q.synchronize();CHECK(c->notify == 0);
  d.backend.complete(1,1);q.synchronize();CHECK(c->notify == 1 && c->reset == 1 && c->destroyed == 1);
  CHECK(tracker->begin == 1 && tracker->end == 1);CHECK(d.backend.blocking == 0);
  CHECK(thread::created == 0);
}
static void resource_wait_and_fifo() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto a=submit(d,q),b=submit(d,q);
  q.synchronizeUntil([&]{return a->notify.load()!=0;});
  CHECK(a->destroyed == 1 && b->notify == 0);CHECK(d.backend.blocking == 1);
  q.waitForIdle();CHECK(b->destroyed == 1);CHECK(d.backend.blocking == 2);
}
static void bounded_queue() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  std::vector<std::shared_ptr<Counts>> c;
  for (size_t i=0;i<MaxNumQueuedCommandBuffers;i++)c.push_back(submit(d,q));
  CHECK(d.backend.blocking == 0);
  c.push_back(submit(d,q));CHECK(d.backend.blocking == 1);
  CHECK(c[0]->destroyed == 1 && c[1]->notify == 0);
  q.waitForIdle();for(auto& v:c)CHECK(v->destroyed == 1);
}
static void present_and_reentry() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  std::shared_ptr<Counts> nested;
  auto a=submit(d,q,[&]{nested=submit(d,q);});
  bool signaled=false;auto p=std::make_shared<Presenter>();
  p->signal=[&]{CHECK(a->notify == 1);signaled=true;};
  std::weak_ptr<Presenter> weak=p;
  DxvkSubmitStatus status;status.result=VK_NOT_READY;
  q.present({p,1,{}},{},&status);p.reset();
  CHECK(status.result == VK_SUCCESS && !signaled && !weak.expired());
  d.backend.complete(1,1);q.synchronize();
  CHECK(a->destroyed == 1 && signaled && weak.expired());
  CHECK(nested && nested->notify == 0);q.waitForIdle();CHECK(nested->destroyed == 1);
}
static void lost_and_other_error() {
  {DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;auto c=submit(d,q);
   d.backend.nextWait=VK_ERROR_DEVICE_LOST;q.synchronize();
   CHECK(q.getLastError() == VK_ERROR_DEVICE_LOST);CHECK(c->destroyed == 1);CHECK(d.idles == 0);}
  {DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;auto a=submit(d,q),b=submit(d,q);
   d.backend.nextWait=VK_ERROR_OUT_OF_HOST_MEMORY;q.synchronize();
   CHECK(q.getLastError() == VK_ERROR_OUT_OF_HOST_MEMORY);CHECK(d.idles == 1);
   CHECK(a->destroyed == 1 && b->destroyed == 1);}
}
static void application_threads() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  std::mutex mutex;std::vector<std::shared_ptr<Counts>> all;
  std::vector<std::thread> clients;
  for(int n=0;n<4;n++)clients.emplace_back([&]{for(int i=0;i<100;i++){
    auto c=submit(d,q);std::lock_guard<std::mutex> lock(mutex);all.push_back(c);
  }});
  for(auto& t:clients)t.join();q.waitForIdle();
  CHECK(all.size() == 400);for(auto& c:all)CHECK(c->destroyed == 1);
  CHECK(thread::created == 0);
}
static void wait_before_submission() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto c=std::make_shared<Counts>();
  auto cmd=std::make_shared<DxvkCommandList>(&d.backend,c);
  DxvkSubmitStatus status;status.result=VK_NOT_READY;
  std::atomic<unsigned> entered{0};
  std::thread resourceWaiter([&]{++entered;q.synchronizeUntil([&]{return c->notify.load()!=0;});});
  std::thread submissionWaiter([&]{++entered;q.synchronizeSubmission(&status);CHECK(status.result==VK_SUCCESS);});
  while(entered.load()!=2)std::this_thread::yield();
  q.submit({cmd},{},&status);cmd.reset();
  resourceWaiter.join();submissionWaiter.join();q.waitForIdle();
  CHECK(c->destroyed == 1 && d.backend.blocking == 1);
}
static void asynchronous_path() {
  DxvkDevice d;d.inlineWork=false;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto c=std::make_shared<Counts>();auto cmd=std::make_shared<DxvkCommandList>(&d.backend,c);
  DxvkSubmitStatus status;status.result=VK_NOT_READY;q.submit({cmd},{},&status);cmd.reset();
  q.synchronizeSubmission(&status);q.waitForIdle();CHECK(status.result == VK_SUCCESS);
  CHECK(c->notify == 1);CHECK(thread::created == 2);
}
static void lost_wait(bool submission) {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto c=std::make_shared<Counts>();
  auto cmd=std::make_shared<DxvkCommandList>(&d.backend,c);
  cmd->submitResult=VK_ERROR_DEVICE_LOST;
  DxvkSubmitStatus failed;failed.result=VK_NOT_READY;
  q.submit({cmd},{},&failed);cmd.reset();
  CHECK(failed.result==VK_ERROR_DEVICE_LOST);CHECK(c->destroyed==1);
  if(submission) {
    DxvkSubmitStatus unsubmitted;unsubmitted.result=VK_NOT_READY;
    q.synchronizeSubmission(&unsubmitted);
    CHECK(unsubmitted.result==VK_ERROR_DEVICE_LOST);
  } else {
    // This CPU signal belonged to a CS command discarded on device loss.
    // There is no queued GPU work that can ever satisfy the predicate.
    q.synchronizeUntil([]{return false;});
  }
  CHECK(q.getLastError()==VK_ERROR_DEVICE_LOST);CHECK(d.backend.blocking==0);
}
static void recording_error_waits() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  DxvkSubmitStatus pending;pending.result=VK_NOT_READY;
  std::atomic<bool> checked{false};
  std::thread resource([&]{q.synchronizeUntil([&]{checked=true;return false;});});
  std::thread submission([&]{q.synchronizeSubmission(&pending);});
  while(!checked.load())std::this_thread::yield();
  q.recordCommandError(VK_ERROR_DEVICE_LOST);
  resource.join();submission.join();
  CHECK(pending.result==VK_ERROR_DEVICE_LOST);
  q.recordCommandError(VK_SUCCESS);
  q.recordCommandError(VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(q.getLastError()==VK_ERROR_DEVICE_LOST);
  CHECK(d.backend.blocking==0 && d.recycled==0);
}
static void recording_oom_preserves_gpu_owners() {
  DxvkDevice d;DxvkSubmissionQueue q(&d, {});d.queue=&q;
  auto c=submit(d,q);
  q.recordCommandError(VK_ERROR_OUT_OF_HOST_MEMORY);
  q.synchronizeUntil([]{return false;});
  CHECK(c->notify==0 && c->destroyed==0 && d.recycled==0);
  q.waitForIdle();
  CHECK(c->notify==1 && c->destroyed==1 && d.recycled==1);
  CHECK(q.getLastError()==VK_ERROR_OUT_OF_HOST_MEMORY);
}
int main(int argc,char** argv) {
  try {
    if(argc>1) {
      std::string name=argv[1];
      if(name=="recording-error")recording_error_waits();
      else if(name=="recording-oom")recording_oom_preserves_gpu_owners();
      else {CHECK(name=="lost-resource" || name=="lost-submission");lost_wait(name=="lost-submission");}
      std::cout<<"PASS "<<name<<" checks="<<checks<<'\n';return 0;
    }
    deferred_and_both_timelines();resource_wait_and_fifo();bounded_queue();
    present_and_reentry();lost_and_other_error();application_threads();wait_before_submission();asynchronous_path();
    std::cout << "PASS " << checks << " actual queue checks; controlled Vulkan peers\n";
  } catch (const std::exception& e) {std::cerr << "FAIL " << e.what() << '\n';return 1;}
}
