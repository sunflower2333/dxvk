#pragma once

#include <windows.h>
#include <dxgi.h>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <new>
#include <utility>
#include <type_traits>
#include <optional>

namespace dxvk::umd {

// The current native interface is D3D10, without free-threaded callback
// admission. Runtime callbacks must stay on the thread inside the DDI.
class RuntimeService final {
public:
  struct Scope {
    RuntimeService* service;
    Scope* previous;
    inline static thread_local Scope* current = nullptr;
    explicit Scope(RuntimeService* value) : service(value), previous(current) { current = this; }
    ~Scope() { current = previous; }
  };
  RuntimeService() {
    m_work = CreateThreadpoolWork(work, this, nullptr);
    if (!m_work) throw std::bad_alloc();
  }
  ~RuntimeService() { CloseThreadpoolWork(m_work); }
  RuntimeService(const RuntimeService&) = delete;
  RuntimeService& operator=(const RuntimeService&) = delete;

  // Native rendering workers may finish between DDIs. Their requests retain
  // their own stack storage until the next permitted caller services them.
  // Flush must separately join command recording and queue submission: a
  // deferred callback is never a replacement for that submission barrier.
  void allowDeferredCalls() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deferred = true;
  }

  bool isCaller() const {
    for (auto scope = Scope::current; scope; scope = scope->previous)
      if (scope->service == this) return true;
    return false;
  }

  template<typename Function> HRESULT invoke(Function&& function) {
    const bool caller = isCaller();
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_closed) return DXGI_ERROR_DEVICE_REMOVED;
    if (caller) { lock.unlock(); return function(); }
    // Outside a synchronous pump there is no legal D3D10 callback thread to
    // execute this request. Fail before reading runtime handles or backing.
    if (!m_pumps && !m_deferred) return DXGI_ERROR_UNSUPPORTED;
    Request request;
    request.context = &function;
    request.function = [](void* ptr) -> HRESULT { return (*static_cast<std::remove_reference_t<Function>*>(ptr))(); };
    *m_tail = &request; m_tail = &request.next;
    m_changed.notify_all();
    m_changed.wait(lock, [&] { return request.complete; });
    return request.result;
  }

  // Run backend work separately while the original DDI thread services all
  // runtime callbacks. Nested runtime callbacks may enter a child DDI; give
  // that operation its own job and pump without joining its suspended parent.
  template<typename Function> auto run(Function&& function) -> decltype(function()) {
    using Result = decltype(function());
    if constexpr (std::is_void_v<Result>) {
      drain(std::forward<Function>(function));
    } else {
      std::optional<Result> result;
      drain([&] { result.emplace(function()); });
      return std::move(*result);
    }
  }

  template<typename Function> void drain(Function&& function) {
    std::lock_guard<std::recursive_mutex> entry(m_entry);
    Scope scope(this);
    Job job;
    job.context = &function;
    job.function = [](void* ptr) { (*static_cast<std::remove_reference_t<Function>*>(ptr))(); };
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      *m_jobTail = &job; m_jobTail = &job.next;
      ++m_pumps;
    }
    SubmitThreadpoolWork(m_work);
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
      m_changed.wait(lock, [&] { return m_head || job.complete; });
      if (m_head) {
        Request* request = m_head;
        m_head = request->next;
        if (!m_head) m_tail = &m_head;
        lock.unlock();
        HRESULT result;
        try { result = request->function(request->context); }
        catch (const std::bad_alloc&) { result = E_OUTOFMEMORY; }
        catch (...) { result = E_FAIL; }
        lock.lock();
        request->result = result; request->complete = true;
        m_changed.notify_all();
      } else {
        --m_pumps;
        break;
      }
    }
    const bool outermost = !m_pumps;
    lock.unlock();
    // A nested callback cannot join its own suspended parent. The outermost
    // DDI joins every work callback return before runtime/module lifetime ends.
    if (outermost) WaitForThreadpoolWorkCallbacks(m_work, FALSE);
    if (job.error) std::rethrow_exception(job.error);
  }
  void close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    while (m_head) {
      auto request = m_head;
      m_head = request->next;
      request->result = DXGI_ERROR_DEVICE_REMOVED;
      request->complete = true;
    }
    m_tail = &m_head;
    m_changed.notify_all();
  }

private:
  struct Request {
    Request* next = nullptr;
    void* context = nullptr;
    HRESULT (*function)(void*) = nullptr;
    HRESULT result = E_FAIL;
    bool complete = false;
  };
  struct Job {
    Job* next = nullptr;
    void* context = nullptr;
    void (*function)(void*) = nullptr;
    std::exception_ptr error;
    bool complete = false;
  };
  static void CALLBACK work(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK) {
    CallbackMayRunLong(instance);
    auto self = static_cast<RuntimeService*>(context);
    Job* job;
    {
      std::lock_guard<std::mutex> lock(self->m_mutex);
      job = self->m_jobHead;
      self->m_jobHead = job->next;
      if (!self->m_jobHead) self->m_jobTail = &self->m_jobHead;
    }
    try { job->function(job->context); }
    catch (...) { job->error = std::current_exception(); }
    std::lock_guard<std::mutex> lock(self->m_mutex);
    job->complete = true;
    self->m_changed.notify_all();
  }
  PTP_WORK m_work = nullptr;
  std::recursive_mutex m_entry;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  Request* m_head = nullptr;
  Request** m_tail = &m_head;
  Job* m_jobHead = nullptr;
  Job** m_jobTail = &m_jobHead;
  unsigned m_pumps = 0;
  bool m_deferred = false, m_closed = false;
};
}
