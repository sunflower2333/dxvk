#include "dxvk_device.h"
#include "dxvk_queue.h"

namespace dxvk {
  
  DxvkSubmissionQueue::DxvkSubmissionQueue(DxvkDevice* device, const DxvkQueueCallback& callback)
  : m_device        (device),
    m_checkpoints   (device->getCheckpointBuffer()),
    m_callback      (callback),
    m_inline        (device->disableExtraThreads()) {
    auto vk = m_device->vkd();

    VkSemaphoreTypeCreateInfo semaphoreType = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    semaphoreType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &semaphoreType };

    VkResult vrGraphics = vk->vkCreateSemaphore(vk->device(), &semaphoreInfo, nullptr, &m_semaphores.graphics);
    VkResult vrTransfer = vk->vkCreateSemaphore(vk->device(), &semaphoreInfo, nullptr, &m_semaphores.transfer);

    if (vrGraphics || vrTransfer) {
      throw DxvkError(str::format("Failed to create timeline semaphores: ",
        vrGraphics > vrTransfer ? vrGraphics : vrTransfer));
    }
    if (!m_inline) {
      m_submitThread = dxvk::thread([this] { submitCmdLists(); });
      m_finishThread = dxvk::thread([this] { finishCmdLists(); });
    }
  }
  
  
  DxvkSubmissionQueue::~DxvkSubmissionQueue() {
    auto vk = m_device->vkd();

    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_stopped.store(true);
    }

    m_appendCond.notify_all();
    m_submitCond.notify_all();

    if (m_submitThread.joinable()) m_submitThread.join();
    if (m_finishThread.joinable()) m_finishThread.join();

    vk->vkDestroySemaphore(vk->device(), m_semaphores.graphics, nullptr);
    vk->vkDestroySemaphore(vk->device(), m_semaphores.transfer, nullptr);
  }
  
  
  void DxvkSubmissionQueue::recordCommandError(VkResult result) {
    if (result == VK_SUCCESS)
      return;

    // Publish under the same lock as the waiter predicate, so a failure with
    // no queued submission cannot lose its wakeup. Preserve an earlier error.
    VkResult expected = VK_SUCCESS;
    if (m_inline) {
      std::lock_guard<dxvk::recursive_mutex> lock(m_inlineMutex);
      m_lastError.compare_exchange_strong(expected, result);
      m_inlineCond.notify_all();
    } else {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      m_lastError.compare_exchange_strong(expected, result);
      m_submitCond.notify_all();
      m_finishCond.notify_all();
    }
  }


  void DxvkSubmissionQueue::submit(
          DxvkSubmitInfo            submitInfo,
          DxvkLatencyInfo           latencyInfo,
          DxvkSubmitStatus*         status) {
    if (m_inline) {
      DxvkSubmitEntry entry = { };
      entry.status = status;
      entry.submit = std::move(submitInfo);
      entry.latency = std::move(latencyInfo);
      submitInline(std::move(entry));
      return;
    }
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_finishCond.wait(lock, [this] {
      return m_submitQueue.size() + m_finishQueue.size() <= MaxNumQueuedCommandBuffers;
    });

    DxvkSubmitEntry entry = { };
    entry.status = status;
    entry.submit = std::move(submitInfo);
    entry.latency = std::move(latencyInfo);

    m_submitQueue.push(std::move(entry));
    m_appendCond.notify_all();
  }


  void DxvkSubmissionQueue::present(
          DxvkPresentInfo           presentInfo,
          DxvkLatencyInfo           latencyInfo,
          DxvkSubmitStatus*         status) {
    if (m_inline) {
      DxvkSubmitEntry entry = { };
      entry.status = status;
      entry.present = std::move(presentInfo);
      entry.latency = std::move(latencyInfo);
      submitInline(std::move(entry));
      return;
    }
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    DxvkSubmitEntry entry = { };
    entry.status  = status;
    entry.present = std::move(presentInfo);
    entry.latency = std::move(latencyInfo);

    m_submitQueue.push(std::move(entry));
    m_appendCond.notify_all();
  }


  void DxvkSubmissionQueue::synchronizeSubmission(
          DxvkSubmitStatus*   status) {
    if (m_inline) {
      std::unique_lock<dxvk::recursive_mutex> lock(m_inlineMutex);
      m_inlineCond.wait(lock, [this, status] {
        return status->result.load() != VK_NOT_READY || m_lastError.load() != VK_SUCCESS;
      });
      VkResult pending = VK_NOT_READY;
      if (m_lastError.load() != VK_SUCCESS)
        status->result.compare_exchange_strong(pending, m_lastError.load());
      pollCompletions();
      return;
    }
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [status] {
      return status->result.load() != VK_NOT_READY;
    });
  }


  void DxvkSubmissionQueue::synchronize() {
    if (m_inline) {
      pollCompletions();
      return;
    }
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [this] {
      return m_submitQueue.empty();
    });
  }


  void DxvkSubmissionQueue::waitForIdle() {
    if (m_inline) {
      std::lock_guard<dxvk::recursive_mutex> lock(m_inlineMutex);
      while (finishInline(true)) { }
      return;
    }
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    m_submitCond.wait(lock, [this] {
      return m_submitQueue.empty();
    });

    m_finishCond.wait(lock, [this] {
      return m_finishQueue.empty();
    });
  }


  void DxvkSubmissionQueue::lockDeviceQueue() {
    m_mutexQueue.lock();

    if (m_callback)
      m_callback(true);
  }


  void DxvkSubmissionQueue::unlockDeviceQueue() {
    if (m_callback)
      m_callback(false);

    m_mutexQueue.unlock();
  }


  void DxvkSubmissionQueue::pollCompletions() {
    if (m_inline) {
      std::lock_guard<dxvk::recursive_mutex> lock(m_inlineMutex);
      while (finishInline(false)) { }
    }
  }

  bool DxvkSubmissionQueue::finishInline(bool wait) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_inlineMutex);
    if (m_inlineQueue.empty())
      return false;

    // Keep ownership on this stack across completion and release callbacks.
    // A reentrant DDI may append or drain other entries, but cannot retire
    // this entry twice. A nonblocking timeout restores it at the front.
    std::list<DxvkSubmitEntry> retiring;
    retiring.splice(retiring.end(), m_inlineQueue, m_inlineQueue.begin());
    auto& entry = retiring.front();
    if (!finishEntry(entry, wait ? ~0ull : 0ull)) {
      m_inlineQueue.splice(m_inlineQueue.begin(), retiring);
      return false;
    }

    if (entry.submit.cmdList != nullptr) {
      entry.submit.cmdList->reset();
      m_device->recycleCommandList(entry.submit.cmdList);
    }
    m_inlineCond.notify_all();
    return true;
  }

  // DISABLE_EXTRA_THREAD_CREATION controls CPU workers, not GPU completion.
  // Submit on the DDI caller and retain entries until their real timelines
  // complete. Resource waits, bounded backpressure and idle drain may wait;
  // an ordinary Flush or submission only polls already completed work.
  void DxvkSubmissionQueue::submitInline(DxvkSubmitEntry entry) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_inlineMutex);
    // Allocate retirement storage before submitting GPU work. All transfers
    // after executeEntry use splice, so allocation failure cannot drop the
    // last owner of an in-flight command list.
    std::list<DxvkSubmitEntry> pending;
    pending.push_back(std::move(entry));
    auto& submitted = pending.front();
    while (finishInline(false)) { }
    while (m_inlineQueue.size() >= MaxNumQueuedCommandBuffers)
      finishInline(true);

    executeEntry(submitted, m_inlineSubmitId, m_inlinePresentId);
    m_inlineCond.notify_all();
    const bool forward = submitted.result == VK_SUCCESS
      || (submitted.present.presenter != nullptr && submitted.result != VK_ERROR_DEVICE_LOST);
    if (forward) {
      // Submission feedback belongs to the caller and may be stack storage.
      // executeEntry has published it; retirement never owns this pointer.
      submitted.status = nullptr;
      m_inlineQueue.splice(m_inlineQueue.end(), pending);
      m_inlineCond.notify_all();
      while (finishInline(false)) { }
    } else {
      Logger::err(str::format("DxvkSubmissionQueue: Command submission failed: ", submitted.result));
      m_lastError = submitted.result;
      m_inlineCond.notify_all();
      if (submitted.result != VK_ERROR_DEVICE_LOST) m_device->waitForIdle();
    }
    m_device->m_objects.memoryManager().performTimedTasks();
  }

  void DxvkSubmissionQueue::executeEntry(DxvkSubmitEntry& entry,
      uint64_t& trackedSubmitId, uint64_t& trackedPresentId) {
    // Submit command buffer to device
    if (m_lastError != VK_ERROR_DEVICE_LOST) {
      std::lock_guard<dxvk::mutex> lock(m_mutexQueue);

      if (m_callback)
        m_callback(true);

      if (entry.submit.cmdList != nullptr) {
        if (entry.latency.tracker) {
          entry.latency.tracker->notifyQueueSubmit(entry.latency.frameId);

          if (!trackedSubmitId && entry.latency.frameId > trackedPresentId)
            trackedSubmitId = entry.latency.frameId;
        }

        entry.result = entry.submit.cmdList->submit(
          m_semaphores, m_timelines, trackedSubmitId);
        entry.timelines = m_timelines;
      } else if (entry.present.presenter != nullptr) {
        if (entry.latency.tracker)
          entry.latency.tracker->notifyQueuePresentBegin(entry.latency.frameId);

        entry.result = entry.present.presenter->presentImage(
          entry.present.frameId, entry.latency.tracker,
          entry.present.rects.size(), entry.present.rects.data());

        if (entry.latency.tracker) {
          entry.latency.tracker->notifyQueuePresentEnd(
            entry.latency.frameId, entry.result);

          trackedPresentId = entry.latency.frameId;
          trackedSubmitId = 0u;
        }
      }

      if (m_callback)
        m_callback(false);
    } else {
      // Don't submit anything after device loss
      // so that drivers get a chance to recover
      entry.result = VK_ERROR_DEVICE_LOST;
    }

    if (entry.status)
      entry.status->result = entry.result;

    if (entry.result == VK_ERROR_DEVICE_LOST && m_checkpoints)
      m_checkpoints->printHangInfo();

  }

  bool DxvkSubmissionQueue::finishEntry(DxvkSubmitEntry& entry, uint64_t timeout) {
    auto vk = m_device->vkd();
    if (entry.submit.cmdList != nullptr) {
      VkResult status = m_lastError.load();

      if (status != VK_ERROR_DEVICE_LOST) {
        std::array<VkSemaphore, 2> semaphores = { m_semaphores.graphics, m_semaphores.transfer };
        std::array<uint64_t, 2> timelines = { entry.timelines.graphics, entry.timelines.transfer };

        if (entry.latency.tracker && timeout)
          entry.latency.tracker->notifyGpuExecutionBegin(entry.latency.frameId);

        VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
        waitInfo.semaphoreCount = semaphores.size();
        waitInfo.pSemaphores = semaphores.data();
        waitInfo.pValues = timelines.data();

        status = vk->vkWaitSemaphores(vk->device(), &waitInfo, timeout);

        if (status == VK_TIMEOUT && !timeout)
          return false;

        if (entry.latency.tracker && status == VK_SUCCESS) {
          if (!timeout)
            entry.latency.tracker->notifyGpuExecutionBegin(entry.latency.frameId);
          entry.latency.tracker->notifyGpuExecutionEnd(entry.latency.frameId);
        }
      }

      if (status == VK_ERROR_DEVICE_LOST && m_checkpoints)
        m_checkpoints->printHangInfo();

      if (status != VK_SUCCESS) {
        m_lastError = status;

        if (status != VK_ERROR_DEVICE_LOST)
          m_device->waitForIdle();
      }
    } else if (entry.present.presenter != nullptr) {
      // Signal the frame and then immediately destroy the reference.
      // This is necessary since the front-end may want to explicitly
      // destroy the presenter object.
      entry.present.presenter->signalFrame(entry.present.frameId, entry.latency.tracker);
      entry.present.presenter = nullptr;
    }

    // Release resources and signal events, then immediately wake
    // up any thread that's currently waiting on a resource in
    // order to reduce delays as much as possible.
    if (entry.submit.cmdList != nullptr)
      entry.submit.cmdList->notifyObjects();

    return true;
  }

  void DxvkSubmissionQueue::submitCmdLists() {
    env::setThreadName("dxvk-submit");

    uint64_t trackedSubmitId = 0u;
    uint64_t trackedPresentId = 0u;

    while (!m_stopped.load()) {
      DxvkSubmitEntry entry;

      { std::unique_lock<dxvk::mutex> lock(m_mutex);

        m_appendCond.wait(lock, [this] {
          return m_stopped.load() || !m_submitQueue.empty();
        });

        if (m_stopped.load())
          return;

        entry = std::move(m_submitQueue.front());
      }

      executeEntry(entry, trackedSubmitId, trackedPresentId);

      // On success, pass it on to the queue thread
      { std::unique_lock<dxvk::mutex> lock(m_mutex);

        bool doForward = (entry.result == VK_SUCCESS) ||
          (entry.present.presenter != nullptr && entry.result != VK_ERROR_DEVICE_LOST);

        if (doForward) {
          m_finishQueue.push(std::move(entry));
        } else {
          Logger::err(str::format("DxvkSubmissionQueue: Command submission failed: ", entry.result));
          m_lastError = entry.result;

          if (m_lastError != VK_ERROR_DEVICE_LOST)
            m_device->waitForIdle();
        }

        m_submitQueue.pop();
        m_submitCond.notify_all();
      }

      // Good time to invoke allocator tasks now since we
      // expect this to get called somewhat periodically.
      m_device->m_objects.memoryManager().performTimedTasks();
    }
  }
  
  
  void DxvkSubmissionQueue::finishCmdLists() {
    env::setThreadName("dxvk-queue");

    while (!m_stopped.load()) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);

      if (m_finishQueue.empty()) {
        auto t0 = dxvk::high_resolution_clock::now();

        m_submitCond.wait(lock, [this] {
          return m_stopped.load() || !m_finishQueue.empty();
        });

        auto t1 = dxvk::high_resolution_clock::now();
        m_gpuIdle += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      }

      if (m_stopped.load())
        return;
      
      DxvkSubmitEntry entry = std::move(m_finishQueue.front());
      lock.unlock();
      
      finishEntry(entry);

      lock.lock();
      m_finishQueue.pop();
      m_finishCond.notify_all();
      lock.unlock();

      // Free the command list and associated objects now
      if (entry.submit.cmdList != nullptr) {
        entry.submit.cmdList->reset();
        m_device->recycleCommandList(entry.submit.cmdList);
      }
    }
  }
  
}
