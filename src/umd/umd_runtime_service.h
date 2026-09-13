#pragma once

#include <windows.h>
#include <dxgi.h>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <new>
#include <utility>
#include <type_traits>

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

  template<typename Function> HRESULT invoke(Function&& function) {
    bool caller = false;
    for (auto scope = Scope::current; scope; scope = scope->previous)
      if (scope->service == this) { caller = true; break; }
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_closed) return DXGI_ERROR_DEVICE_REMOVED;
    if (caller) { lock.unlock(); return function(); }
    // Outside a synchronous pump there is no legal D3D10 callback thread to
    // execute this request. Fail before reading runtime handles or backing.
    if (!m_pumping) return DXGI_ERROR_UNSUPPORTED;
    Request request;
    request.context = &function;
    request.function = [](void* ptr) -> HRESULT { return (*static_cast<std::remove_reference_t<Function>*>(ptr))(); };
    *m_tail = &request; m_tail = &request.next;
    m_changed.notify_all();
    m_changed.wait(lock, [&] { return request.complete; });
    return request.result;
  }

  // Only backend release runs on the reserved worker. This calling DDI thread
  // executes every requested runtime callback, then joins the worker before
  // any runtime handle or private-storage lifetime can end.
  template<typename Function> void drain(Function&& function) {
    Scope scope(this);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_jobContext = &function;
      m_job = [](void* ptr) { (*static_cast<std::remove_reference_t<Function>*>(ptr))(); };
      m_jobComplete = false; m_error = nullptr; m_pumping = true;
    }
    SubmitThreadpoolWork(m_work);
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
      m_changed.wait(lock, [&] { return m_head || m_jobComplete; });
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
        m_pumping = false;
        break;
      }
    }
    lock.unlock();
    // Work completion publishes the result before its callback returns. Join
    // that return too, while the runtime still owns the executing UMD module.
    WaitForThreadpoolWorkCallbacks(m_work, FALSE);
    if (m_error) std::rethrow_exception(m_error);
  }
  void close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
  }

private:
  struct Request {
    Request* next = nullptr;
    void* context = nullptr;
    HRESULT (*function)(void*) = nullptr;
    HRESULT result = E_FAIL;
    bool complete = false;
  };
  static void CALLBACK work(PTP_CALLBACK_INSTANCE, void* context, PTP_WORK) {
    auto self = static_cast<RuntimeService*>(context);
    try { self->m_job(self->m_jobContext); }
    catch (...) { self->m_error = std::current_exception(); }
    std::lock_guard<std::mutex> lock(self->m_mutex);
    self->m_jobComplete = true;
    self->m_changed.notify_all();
  }
  PTP_WORK m_work = nullptr;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  Request* m_head = nullptr;
  Request** m_tail = &m_head;
  void* m_jobContext = nullptr;
  void (*m_job)(void*) = nullptr;
  std::exception_ptr m_error;
  bool m_jobComplete = false, m_pumping = false, m_closed = false;
};
}
