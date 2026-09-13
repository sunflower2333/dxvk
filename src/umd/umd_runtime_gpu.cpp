#include "umd_runtime_gpu.h"
#include "umd_allocation.h"
#include <cstring>
#include <limits>
#include <utility>

namespace dxvk::umd {
namespace {
// Exact VIOGPU v0 wire types. Runtime opaque handles never enter these bytes.
#pragma pack(push, 4)
struct Header {
  uint32_t magic = 0x504d5644, version = 0, size, reserved = 0;
  bool valid(uint32_t expected) const {
    return magic == 0x504d5644 && !version && size == expected && !reserved;
  }
};
struct Create { Header header{0x504d5644, 0, 32, 0}; uint64_t generation; uint32_t flags = 0, reserved = 0; };
struct Context {
  Header header{0x504d5644, 0, 64, 0}; uint32_t opcode = 1, flags = 0;
  uint64_t expected = 0, start = 0, size = 0, generation = 0;
  uint32_t context = 0, queue = 0;
};
struct Fence {
  Header header{0x504d5644, 0, 56, 0}; uint32_t opcode = 2, flags = 0;
  uint64_t expected = 0, completed = 0, generation = 0;
  uint32_t context = 0, reserved = 0;
};
struct Render {
  Header header{0x504d5644, 0, 0, 0}; uint32_t opcode = 1, flags = 0;
  uint64_t generation = 0;
  uint32_t references = 64, count = 0, stream = 0, size = 0, reserved[4] = {};
};
struct Reference { uint32_t index, flags; uint64_t offset, length; uint32_t patch, reserved; };
#pragma pack(pop)
static_assert(sizeof(Create) == 32 && sizeof(Context) == 64 && sizeof(Fence) == 56);
static_assert(sizeof(Render) == 64 && sizeof(Reference) == 32);
HRESULT exact(HRESULT hr) { return hr == S_OK || FAILED(hr) ? hr : E_FAIL; }
struct Pending {
  bool& flag;
  explicit Pending(bool& value) : flag(value) { flag = true; }
  ~Pending() { flag = false; }
};
}

const mwd_callbacks RuntimeGpu::s_callbacks = {
  MWD_RUNTIME_MAGIC, MWD_RUNTIME_ABI_VERSION, sizeof(mwd_callbacks), 0,
  getContext, allocate, retain, release, map, unmap, submit, completed, status
};

std::shared_ptr<RuntimeGpu> RuntimeGpu::create(HANDLE device,
    const D3DDDI_DEVICECALLBACKS& cb, std::shared_ptr<const AdapterIdentity> identity) {
  auto result = std::shared_ptr<RuntimeGpu>(new RuntimeGpu);
  result->m_device = device;
  result->m_identity = std::move(identity);
  // Copy only callbacks in the negotiated D3D10 table. Never retain a pointer
  // to runtime-owned tables or read a newer whole-WDK structure from old input.
  auto& copy = result->m_callbacks;
  copy.pfnAllocateCb = cb.pfnAllocateCb; copy.pfnDeallocateCb = cb.pfnDeallocateCb;
  copy.pfnLockCb = cb.pfnLockCb; copy.pfnUnlockCb = cb.pfnUnlockCb;
  copy.pfnCreateContextCb = cb.pfnCreateContextCb; copy.pfnDestroyContextCb = cb.pfnDestroyContextCb;
  copy.pfnEscapeCb = cb.pfnEscapeCb; copy.pfnRenderCb = cb.pfnRenderCb;
  return result;
}
RuntimeGpu::~RuntimeGpu() { close(); }
RuntimeBackend RuntimeGpu::backend() {
  return {{MWD_STYPE_DEVICE, nullptr, &s_callbacks, this}, shared_from_this()};
}
RuntimeGpu::Activity::Activity(RuntimeGpu& source) : value(source) {
  std::lock_guard<std::mutex> lock(value.m_activityMutex);
  ++value.m_calls;
}
RuntimeGpu::Activity::~Activity() {
  std::lock_guard<std::mutex> lock(value.m_activityMutex);
  if (!--value.m_calls) value.m_activityChanged.notify_all();
}
bool RuntimeGpu::hasActiveCalls() const {
  std::lock_guard<std::mutex> lock(m_activityMutex);
  return m_calls != 0;
}
void RuntimeGpu::waitForCalls() {
  std::unique_lock<std::mutex> lock(m_activityMutex);
  m_activityChanged.wait(lock, [this] { return !m_calls; });
}
RuntimeGpu::Call::Call(void* ptr)
: value(static_cast<RuntimeGpu*>(ptr)), owner(value->weak_from_this().lock()),
  activity(*value), lock(value->m_mutex) { ++value->m_active; }
// Member order releases the recursive lock before publishing quiescence,
// and retains RuntimeGpu until both lock and activity guards have unwound.
RuntimeGpu::Call::~Call() { --value->m_active; }
bool RuntimeGpu::Call::live(bool cleanup) const {
  return value->m_live && (cleanup || !value->m_closing) && value->m_device && value->m_identity;
}

HRESULT RuntimeGpu::identity() {
  if (!m_live || m_removed || !m_device || !m_identity) return DXGI_ERROR_DEVICE_REMOVED;
  if (m_querying) return DXGI_ERROR_WAS_STILL_DRAWING;
  Pending pending(m_querying);
  RuntimeIdentity current;
  HRESULT hr = queryRuntimeIdentity(m_identity->runtime, m_identity->query, current);
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK && (std::memcmp(current.luid.data(), &m_identity->luid, sizeof(LUID))
      || current.generation != m_identity->generation || current.capabilities != m_identity->capabilities))
    hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == DXGI_ERROR_DEVICE_REMOVED) m_removed = true;
  return exact(hr);
}
HRESULT RuntimeGpu::context() {
  if (m_creating) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (m_info.context_id) return identity();
  const auto& cb = m_callbacks;
  if (!cb.pfnAllocateCb || !cb.pfnDeallocateCb || !cb.pfnLockCb || !cb.pfnUnlockCb
      || !cb.pfnCreateContextCb || !cb.pfnDestroyContextCb || !cb.pfnEscapeCb || !cb.pfnRenderCb)
    return DXGI_ERROR_UNSUPPORTED;
  HRESULT hr = identity();
  if (FAILED(hr)) return hr;
  Pending pending(m_creating);
  if (m_context) {
    D3DDDICB_DESTROYCONTEXT destroy = {}; destroy.hContext = m_context;
    hr = exact(cb.pfnDestroyContextCb(m_device, &destroy));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(hr)) return hr;
    m_context = nullptr;
  }
  Create info; info.generation = m_identity->generation;
  D3DDDICB_CREATECONTEXT create = {};
  create.EngineAffinity = 1; create.pPrivateDriverData = &info; create.PrivateDriverDataSize = sizeof(info);
  hr = exact(cb.pfnCreateContextCb(m_device, &create));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  m_context = create.hContext;
  if (hr == S_OK && !m_context) hr = E_FAIL;
  Context reply;
  reply.expected = m_identity->generation;
  if (hr == S_OK) {
    D3DDDICB_ESCAPE escape = {};
    escape.hDevice = m_device; escape.hContext = m_context;
    escape.pPrivateDriverData = &reply; escape.PrivateDriverDataSize = sizeof(reply);
    hr = exact(cb.pfnEscapeCb(m_identity->runtime.handle, &escape));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  }
  if (hr == S_OK && (!reply.header.valid(sizeof(reply)) || reply.opcode != 1 || reply.flags
      || reply.expected != m_identity->generation || reply.generation != reply.expected
      || !reply.context || !reply.queue || !reply.start || !reply.size
      || ((reply.start | reply.size) & 4095) || reply.size > UINT64_MAX - reply.start
      || !create.pCommandBuffer || create.CommandBufferSize < sizeof(Render) || create.CommandBufferSize > 65536
      || !create.pAllocationList || !create.AllocationListSize || create.AllocationListSize > 1024
      || !create.pPatchLocationList || !create.PatchLocationListSize || create.PatchLocationListSize > 1024)) hr = E_FAIL;
  if (hr == S_OK) hr = identity();
  if (hr == S_OK) {
    std::memcpy(m_info.luid, &m_identity->luid, sizeof(LUID));
    m_info.generation = reply.generation; m_info.va_start = reply.start; m_info.va_size = reply.size;
    m_info.context_id = reply.context; m_info.queue_id = reply.queue;
    m_commands = create.pCommandBuffer; m_commandSize = create.CommandBufferSize;
    m_allocationList = create.pAllocationList; m_allocationCount = create.AllocationListSize;
    m_patchList = create.pPatchLocationList; m_patchCount = create.PatchLocationListSize;
  } else if (m_live && m_context) {
    D3DDDICB_DESTROYCONTEXT destroy = {}; destroy.hContext = m_context;
    const HRESULT cleanup = exact(cb.pfnDestroyContextCb(m_device, &destroy));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (cleanup == S_OK) m_context = nullptr;
    else hr = cleanup; // Retain ownership for close/retry when destroy fails.
  }
  return hr;
}

RuntimeGpu::Allocation* RuntimeGpu::find(void* token) {
  for (const auto& entry : m_allocations) if (entry.second->token == token) return entry.second.get();
  return nullptr;
}
void RuntimeGpu::publish(Allocation& a, mwd_allocation* out) {
  *out = {a.token, a.address, a.size, m_info.generation, a.handle, a.flags};
}
int32_t MWD_CALL RuntimeGpu::getContext(void* ptr, mwd_context_info* out) {
  if (!out) return E_POINTER;
  *out = {};
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  const HRESULT hr = self.context();
  if (hr == S_OK) *out = self.m_info;
  return hr;
}
int32_t MWD_CALL RuntimeGpu::allocate(void* ptr, uint64_t size, uint64_t alignment,
    uint64_t requested, uint32_t flags, mwd_allocation* out) {
  if (!out) return E_POINTER;
  *out = {};
  try {
    Call call(ptr); auto& self = *call.value;
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (!size || size > (UINT32_MAX & ~uint64_t(4095)) || alignment < 4096
        || (alignment & (alignment - 1)) || (requested & (alignment - 1))
        || !(flags & 4) || (flags & ~14u)) return E_INVALIDARG;
    HRESULT hr = self.context();
    if (FAILED(hr)) return hr;
    size = (size + 4095) & ~uint64_t(4095);
    uint64_t address = self.m_info.va_start;
    const uint64_t end = address + self.m_info.va_size;
    auto position = self.m_allocations.begin();
    while (true) {
      if (address > UINT64_MAX - (alignment - 1)) return E_OUTOFMEMORY;
      address = (address + alignment - 1) & ~(alignment - 1);
      const uint64_t limit = position == self.m_allocations.end() ? end : position->first;
      if (requested >= address && requested <= limit) address = requested;
      if ((!requested || address == requested) && address <= limit && size <= limit - address) break;
      if ((requested && requested < limit) || position == self.m_allocations.end()) return E_OUTOFMEMORY;
      address = position->second->address + position->second->size;
      ++position;
    }
    if (!self.m_nextToken) return E_OUTOFMEMORY;
    auto value = std::make_unique<Allocation>();
    value->token = reinterpret_cast<void*>(self.m_nextToken++);
    value->address = address; value->size = size; value->flags = flags;
    auto* allocation = value.get();
    self.m_allocations.emplace_hint(position, address, std::move(value));
    allocation->pending = true;
    struct Finish {
      RuntimeGpu& owner; Allocation* value;
      ~Finish() {
        value->pending = false;
        if (!value->handle) owner.m_allocations.erase(value->address);
      }
    } finish{self, allocation};
    AllocationInfo info;
    info.size = size; info.requestedIova = address; info.resetGeneration = self.m_info.generation;
    info.flags = flags; info.contextId = self.m_info.context_id;
    D3DDDI_ALLOCATIONINFO output = {};
    output.pPrivateDriverData = &info; output.PrivateDriverDataSize = sizeof(info);
    D3DDDICB_ALLOCATE request = {};
    request.NumAllocations = 1; request.pAllocationInfo = &output;
    hr = exact(self.m_callbacks.pfnAllocateCb(self.m_device, &request));
    allocation->handle = output.hAllocation;
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (hr == S_OK && (!allocation->handle || request.hKMResource)) hr = E_FAIL;
    if (hr == S_OK) hr = self.identity();
    if (hr == S_OK) self.publish(*allocation, out);
    // Failed creates retain unpublished ownership until close if cleanup fails.
    // Keep this record alive until Finish clears its reservation.
    else if (allocation->handle && self.m_live) {
      D3DDDICB_DEALLOCATE cleanup = {};
      cleanup.NumAllocations = 1; cleanup.HandleList = &allocation->handle;
      const HRESULT released = exact(self.m_callbacks.pfnDeallocateCb(self.m_device, &cleanup));
      if (!self.m_live) return DXGI_ERROR_DEVICE_REMOVED;
      if (released == S_OK) allocation->handle = 0;
      else hr = released;
    }
    return hr;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
int32_t MWD_CALL RuntimeGpu::retain(void* ptr, void* token, mwd_allocation* out) {
  if (!out) return E_POINTER;
  *out = {};
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  auto* a = self.find(token);
  if (!a || a->pending || !a->handle || a->users == UINT32_MAX) return E_INVALIDARG;
  Pending pending(a->pending);
  HRESULT hr = self.identity();
  if (hr == S_OK) { ++a->users; self.publish(*a, out); }
  return hr;
}
HRESULT RuntimeGpu::unlock(Allocation& a) {
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (!a.locked) return S_OK;
  D3DDDICB_UNLOCK request = {}; request.NumAllocations = 1; request.phAllocations = &a.handle;
  const HRESULT hr = exact(m_callbacks.pfnUnlockCb(m_device, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) { a.locked = false; a.mapValid = false; a.mapping = nullptr; a.maps = 0; }
  return hr;
}
HRESULT RuntimeGpu::release(Allocation& a) {
  HRESULT hr = unlock(a);
  if (FAILED(hr) || !a.handle) return hr;
  D3DDDICB_DEALLOCATE request = {}; request.NumAllocations = 1; request.HandleList = &a.handle;
  hr = exact(m_callbacks.pfnDeallocateCb(m_device, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) a.handle = 0;
  return hr;
}
int32_t MWD_CALL RuntimeGpu::release(void* ptr, void* token) {
  Call call(ptr); auto& self = *call.value;
  auto* a = self.find(token);
  if (!a || a->pending || !a->users) return E_INVALIDARG;
  if (self.m_closing && self.m_live) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (a->users > 1) { --a->users; return S_OK; }
  HRESULT hr = S_OK;
  { Pending pending(a->pending); if (call.live(true)) hr = self.release(*a); }
  if (hr == S_OK) self.m_allocations.erase(a->address);
  return hr;
}
int32_t MWD_CALL RuntimeGpu::map(void* ptr, void* token, void** out, uint32_t* handle) {
  if (!out || !handle) return E_POINTER;
  *out = nullptr; *handle = 0;
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  auto* a = self.find(token);
  if (!a || a->pending || !a->handle || !(a->flags & 2) || a->maps == UINT32_MAX) return E_INVALIDARG;
  Pending pending(a->pending);
  HRESULT hr = self.identity();
  if (FAILED(hr)) return hr;
  // A malformed successful Lock whose balancing Unlock failed is still owned,
  // but its returned pointer must never become a successful nested mapping.
  if (a->locked && !a->mapValid) return E_FAIL;
  if (!a->locked) {
    D3DDDICB_LOCK request = {}; request.hAllocation = a->handle; request.Flags.LockEntire = 1;
    const HRESULT locked = self.m_callbacks.pfnLockCb(self.m_device, &request);
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(locked)) return locked;
    a->locked = true;
    // A renamed allocation is the same owner, not a new allocation/token.
    if (request.hAllocation) a->handle = request.hAllocation;
    a->mapping = request.pData;
    hr = exact(locked);
    if (hr == S_OK && (!request.hAllocation || !request.pData)) hr = E_FAIL;
    if (hr == S_OK) hr = self.identity();
    if (FAILED(hr)) {
      const HRESULT cleanup = self.unlock(*a);
      return FAILED(cleanup) ? cleanup : hr;
    }
    a->mapValid = true;
  }
  ++a->maps; *out = a->mapping; *handle = a->handle;
  return S_OK;
}
int32_t MWD_CALL RuntimeGpu::unmap(void* ptr, void* token) {
  Call call(ptr); auto& self = *call.value;
  auto* a = self.find(token);
  if (!a || a->pending) return E_INVALIDARG;
  if (self.m_closing && self.m_live) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (!call.live(true)) return S_OK;
  if (!a->maps) return E_INVALIDARG;
  if (a->maps > 1) { --a->maps; return S_OK; }
  Pending pending(a->pending);
  return self.unlock(*a);
}
int32_t MWD_CALL RuntimeGpu::submit(void* ptr, const void* stream, uint32_t size,
    const mwd_reference* refs, uint32_t count) {
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (!stream || size < 8 || size > 65536 || !refs || !count || count > 1024 || self.m_submitting)
    return E_INVALIDARG;
  const uint64_t offset = sizeof(Render) + uint64_t(count) * sizeof(Reference), bytes = offset + size;
  if (bytes > self.m_commandSize || count > self.m_allocationCount || count > self.m_patchCount
      || !self.m_commands || !self.m_allocationList || !self.m_patchList) return E_INVALIDARG;
  // Validate all owners/ranges before modifying runtime-owned command buffers.
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = self.find(refs[i].token);
    if (!a || a->pending || !a->handle || !refs[i].flags || (refs[i].flags & ~3u)
        || ((a->flags & 8u) && (refs[i].flags & 2u)) || refs[i].offset > UINT32_MAX
        || refs[i].offset > a->size || !refs[i].length || refs[i].length > a->size - refs[i].offset
        || refs[i].patch_offset % 4 || refs[i].patch_offset > size - 8) return E_INVALIDARG;
    for (uint32_t j = 0; j < i; ++j) {
      const uint64_t x = refs[i].patch_offset, y = refs[j].patch_offset;
      if (refs[i].token == refs[j].token || (x < y + 8 && y < x + 8)) return E_INVALIDARG;
    }
  }
  struct References {
    Allocation* values[1024] = {}; uint32_t count = 0;
    ~References() { for (uint32_t i = 0; i < count; ++i) values[i]->pending = false; }
  } owners;
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = self.find(refs[i].token); a->pending = true; owners.values[owners.count++] = a;
  }
  Pending pending(self.m_submitting);
  HRESULT hr = self.identity();
  if (FAILED(hr)) return hr;
  Render header;
  header.header.size = uint32_t(bytes); header.generation = self.m_info.generation;
  header.count = count; header.stream = uint32_t(offset); header.size = size;
  auto* packet = static_cast<uint8_t*>(self.m_commands);
  std::memcpy(packet, &header, sizeof(header)); std::memcpy(packet + offset, stream, size);
  for (uint32_t i = 0; i < count; ++i) {
    const Reference wire = {i, refs[i].flags, refs[i].offset, refs[i].length, refs[i].patch_offset, 0};
    std::memcpy(packet + sizeof(Render) + i * sizeof(wire), &wire, sizeof(wire));
    auto& allocation = self.m_allocationList[i]; allocation = {};
    allocation.hAllocation = self.find(refs[i].token)->handle; allocation.WriteOperation = (refs[i].flags & 2) != 0;
    auto& patch = self.m_patchList[i]; patch = {};
    patch.AllocationIndex = i; patch.AllocationOffset = UINT(refs[i].offset); patch.PatchOffset = UINT(offset + refs[i].patch_offset);
  }
  D3DDDICB_RENDER request = {}; request.hContext = self.m_context; request.CommandLength = UINT(bytes);
  request.NumAllocations = request.NumPatchLocations = count;
  request.pNewCommandBuffer = self.m_commands; request.NewCommandBufferSize = self.m_commandSize;
  request.pNewAllocationList = self.m_allocationList; request.NewAllocationListSize = self.m_allocationCount;
  request.pNewPatchLocationList = self.m_patchList; request.NewPatchLocationListSize = self.m_patchCount;
  hr = exact(self.m_callbacks.pfnRenderCb(self.m_device, &request));
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  // Callback replacement buffers become authoritative on failure too.
  self.m_commands = request.pNewCommandBuffer; self.m_commandSize = request.NewCommandBufferSize;
  self.m_allocationList = request.pNewAllocationList; self.m_allocationCount = request.NewAllocationListSize;
  self.m_patchList = request.pNewPatchLocationList; self.m_patchCount = request.NewPatchLocationListSize;
  if (!self.m_commands || self.m_commandSize < sizeof(Render) || self.m_commandSize > 65536
      || !self.m_allocationList || !self.m_allocationCount || self.m_allocationCount > 1024
      || !self.m_patchList || !self.m_patchCount || self.m_patchCount > 1024) { self.m_removed = true; return E_FAIL; }
  return FAILED(hr) ? hr : self.identity();
}
int32_t MWD_CALL RuntimeGpu::completed(void* ptr, uint32_t* out) {
  if (!out) return E_POINTER;
  *out = 0;
  Call call(ptr); auto& self = *call.value;
  if (!call.live(true) || !self.m_info.context_id) return DXGI_ERROR_DEVICE_REMOVED;
  Fence info; info.expected = self.m_info.generation;
  D3DDDICB_ESCAPE request = {}; request.hDevice = self.m_device; request.hContext = self.m_context;
  request.pPrivateDriverData = &info; request.PrivateDriverDataSize = sizeof(info);
  HRESULT hr = exact(self.m_callbacks.pfnEscapeCb(self.m_identity->runtime.handle, &request));
  if (!call.live(true)) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK && (!info.header.valid(sizeof(info)) || info.opcode != 2 || info.flags || info.reserved
      || info.expected != self.m_info.generation || info.generation != info.expected
      || info.context != self.m_info.context_id || info.completed > UINT32_MAX)) hr = E_FAIL;
  if (hr == S_OK) hr = self.identity();
  if (hr == S_OK) *out = uint32_t(info.completed);
  return hr;
}
int32_t MWD_CALL RuntimeGpu::status(void* ptr) {
  Call call(ptr); return call.live(true) ? call.value->identity() : DXGI_ERROR_DEVICE_REMOVED;
}

HRESULT RuntimeGpu::close() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  if (!m_live || m_closing) return S_OK;
  m_closing = true;
  HRESULT result = S_OK;
  // A runtime callback can retire the device synchronously. In that case final
  // kernel teardown owns residual allocations; do not invoke a second callback
  // or free mapped backing underneath the interrupted backend operation.
  if (!m_active) {
    for (auto& entry : m_allocations) {
      Pending pending(entry.second->pending);
      const HRESULT hr = release(*entry.second);
      if (FAILED(hr) && SUCCEEDED(result)) result = hr;
    }
    if (m_context) {
      D3DDDICB_DESTROYCONTEXT request = {}; request.hContext = m_context;
      const HRESULT hr = exact(m_callbacks.pfnDestroyContextCb(m_device, &request));
      if (FAILED(hr) && SUCCEEDED(result)) result = hr;
    }
  }
  m_live = false; m_context = nullptr; m_device = nullptr; m_callbacks = {};
  m_info = {}; m_commands = nullptr; m_allocationList = nullptr; m_patchList = nullptr;
  m_commandSize = m_allocationCount = m_patchCount = 0;
  return result;
}
}
