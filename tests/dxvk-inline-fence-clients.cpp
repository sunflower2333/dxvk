// Actual D3D11/D3D9 wait clients, device wait and queue methods with controlled
// submission peers. The real CPU fence is never signaled by the GPU emulator.
#define DXVK_INLINE_FENCE_CLIENTS
#define main queue_regression_main
#include "dxvk-inline-queue.cpp"
#undef main

namespace dxvk { struct RcObject { virtual ~RcObject() = default; }; }
#include "sync_signal_under_test.h"

using VkDeviceSize = uint64_t;
namespace dxvk {
namespace env { constexpr bool is32BitHostPlatform() {return CLIENT_BITS == 32;} }
static std::shared_ptr<Counts> submitFenceWork(DxvkDevice& d, std::function<void()> notify) {
  auto counts=std::make_shared<Counts>();
  auto cmd=std::make_shared<DxvkCommandList>(&d.backend,counts);
  cmd->resetCallback=std::move(notify);
  DxvkSubmitStatus status;status.result=VK_NOT_READY;
  d.queue->submit({cmd},{},&status);
  d.queue->synchronizeSubmission(&status);
  CHECK(status.result==VK_SUCCESS);
  return counts;
}
struct DxvkStagingBufferStats {uint64_t allocatedTotal = 0, allocatedSinceLastReset = 0;};
class D3D11Initializer {
public:
#include "initializer_limits_under_test.h"
  DxvkDevice* m_device;
  struct Staging {DxvkStagingBufferStats stats; DxvkStagingBufferStats getStatistics() {return stats;}} m_stagingBuffer;
  Rc<sync::Fence> m_stagingSignal = std::make_shared<sync::Fence>();
  size_t m_transferCommands = 0;
  unsigned flushes = 0;
  std::shared_ptr<Counts> counts;
  explicit D3D11Initializer(DxvkDevice* d) : m_device(d) {}
  void ExecuteFlushLocked() {
    ++flushes;
    counts=submitFenceWork(*m_device,[signal=m_stagingSignal,value=m_stagingBuffer.stats.allocatedTotal]{signal->signal(value);});
  }
  void ThrottleAllocationLocked();
};
struct DxvkSamplerPool { static constexpr uint32_t MaxSamplerCount = 2048u; };
constexpr uint32_t SamplerCount = 21u;
enum class GpuFlushType {ImplicitSynchronization};
struct DxvkCsThread {static constexpr uint64_t SynchronizeAll = UINT64_MAX; uint64_t lastSequenceNumber() {return 0;}};
inline bool likely(bool value) {return value;}
class D3D9DeviceEx {
public:
  static constexpr uint32_t SamplerCountBits = 12u;
  static constexpr uint64_t SamplerCountMask = (1u << SamplerCountBits) - 1u;
  uint64_t m_lastSamplerLiveCount = DxvkSamplerPool::MaxSamplerCount;
  uint64_t m_samplerBindCount = 1, m_lastSamplerBindCount = 1;
  std::atomic<uint64_t> m_lastSamplerStats{DxvkSamplerPool::MaxSamplerCount | (uint64_t(1) << SamplerCountBits)};
  DxvkCsThread m_csThread;
  DxvkDevice* m_dxvkDevice;
  Rc<sync::Fence> m_submissionFence = std::make_shared<sync::Fence>();
  uint64_t m_submissionId = 0;
  unsigned flushes = 0;
  std::shared_ptr<Counts> counts;
  explicit D3D9DeviceEx(DxvkDevice* d) : m_dxvkDevice(d) {d->samplerLive=DxvkSamplerPool::MaxSamplerCount;}
  void ConsiderFlush(GpuFlushType) {}
  uint64_t GetCurrentSequenceNumber() {return 0;}
  void SynchronizeCsThread(uint64_t) {}
  void ExecuteFlush(bool) {
    ++flushes; ++m_submissionId;
    counts=submitFenceWork(*m_dxvkDevice,[this]{
      m_dxvkDevice->samplerLive=0;
      m_submissionFence->signal(m_submissionId);
    });
  }
  void EnsureSamplerLimit();
};
}
#include "fence_client_methods_under_test.h"

static void initializer(bool asynchronous, bool belowLimit) {
  DxvkDevice d; d.inlineWork=!asynchronous; DxvkSubmissionQueue q(&d,{}); d.queue=&q;
  D3D11Initializer init(&d);
  init.m_stagingBuffer.stats.allocatedTotal = D3D11Initializer::MaxMemoryInFlight + (belowLimit ? 0 : 1);
  std::thread gpu([&]{std::this_thread::sleep_for(std::chrono::milliseconds(10));d.backend.complete(1,1);});
  init.ThrottleAllocationLocked();
  gpu.join();
  CHECK(init.flushes == (belowLimit ? 0u : 1u));
  if (!belowLimit) {
    q.waitForIdle();
    CHECK(init.m_stagingSignal->value() == init.m_stagingBuffer.stats.allocatedTotal);
    CHECK(init.counts->notify == 1 && init.counts->reset == 1);
  }
  CHECK(thread::created == (asynchronous ? 2u : 0u));
}
static void sampler(bool asynchronous) {
  DxvkDevice d; d.inlineWork=!asynchronous; DxvkSubmissionQueue q(&d,{}); d.queue=&q;
  D3D9DeviceEx device(&d);
  std::thread gpu([&]{std::this_thread::sleep_for(std::chrono::milliseconds(10));d.backend.complete(1,1);});
  device.EnsureSamplerLimit();
  gpu.join(); q.waitForIdle();
  CHECK(device.flushes == 1 && device.m_submissionFence->value() == 1);
  CHECK(device.counts->notify == 1 && device.counts->reset == 1);
  CHECK((device.m_lastSamplerStats.load() & D3D9DeviceEx::SamplerCountMask) == 0);
  CHECK(thread::created == (asynchronous ? 2u : 0u));
}
int main(int argc, char** argv) {
  try {
    CHECK(argc == 2); std::string name(argv[1]);
    if (name=="initializer") initializer(false,false);
    else if(name=="initializer-async") initializer(true,false);
    else if(name=="initializer-limit") initializer(false,true);
    else if(name=="sampler") sampler(false);
    else if(name=="sampler-async") sampler(true);
    else throw std::runtime_error("unknown case");
    std::cout << "PASS " << name << " bits=" << CLIENT_BITS << " checks=" << checks << '\n';
  } catch(const std::exception& e) {std::cerr << "FAIL " << e.what() << '\n';return 1;}
}
