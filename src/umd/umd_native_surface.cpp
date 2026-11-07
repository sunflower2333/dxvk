#include "umd_native_surface.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <mutex>

namespace dxvk::umd {
HRESULT queryNativeSurfaceCapabilities(const AdapterIdentity& identity, uint32_t& capabilities) {
  capabilities = 0;
  if (!identity.continuity || !identity.generation) return DXGI_ERROR_UNSUPPORTED;
  auto current = [&]() -> HRESULT {
    if (!identity.available()) return DXGI_ERROR_DEVICE_REMOVED;
    RuntimeIdentity actual;
    const HRESULT hr = identity.continuity->query(actual, RuntimeQueryStage::Continuity);
    if (hr != S_OK) return hr == HRESULT_FROM_NT(NTSTATUS(0xc00000a3))
      ? DXGI_ERROR_DEVICE_REMOVED : FAILED(hr) ? hr : E_FAIL;
    AdapterLuid luid; std::memcpy(luid.data(), &identity.luid, sizeof(identity.luid));
    return identity.available() && actual.luid == luid && actual.generation == identity.generation
      && actual.capabilities == identity.capabilities ? S_OK : DXGI_ERROR_DEVICE_REMOVED;
  };
  HRESULT hr = current();
  if (hr != S_OK) return hr;
  NativeSurfaceCapabilities reply;
  hr = identity.continuity->nativeEscape(&reply, sizeof(reply));
  const HRESULT after = current();
  if (after != S_OK) return after;
  // Old KMDs reject the unknown opcode. Only known unsupported statuses
  // select ordinary backing; a malformed success must never enable imports.
  if (hr == DXGI_ERROR_UNSUPPORTED || hr == E_NOTIMPL
      || hr == HRESULT_FROM_NT(NTSTATUS(0xc0000002))
      || hr == HRESULT_FROM_NT(NTSTATUS(0xc000000d))
      || hr == HRESULT_FROM_NT(NTSTATUS(0xc00000bb))) return DXGI_ERROR_UNSUPPORTED;
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!validNativeSurfaceCapabilities(reply)) return E_FAIL;
  if (reply.generation != identity.generation) return DXGI_ERROR_DEVICE_REMOVED;
  capabilities = reply.capabilities;
  return S_OK;
}

struct NativeSurfaceRetirement {
  RuntimeIdentity initial;
  NativeSurfaceInfo surface;
  std::shared_ptr<const AdapterIdentitySource> source;
  std::unique_ptr<NativeSurfaceRetirement> next;
};
namespace {
struct Retirements {
  std::mutex mutex;
  bool draining = false;
  std::unique_ptr<NativeSurfaceRetirement> pending;
};
Retirements& retirements() { static Retirements value; return value; }
HRESULT freeSurface(const AdapterIdentitySource& source, const NativeSurfaceInfo& surface, bool& owned) {
  NativeSurfaceInfo request = surface;
  // Repair only the request envelope, preserving the exact acquired identity.
  request.magic = 0x504d5644; request.version = 0; request.headerSize = sizeof(request);
  request.reserved = 0; request.flags = 0;
  request.reserved2[0] = request.reserved2[1] = request.reserved2[2] = 0;
  request.opcode = 9; request.expected = request.generation;
  const NativeSurfaceInfo expected = request;
  const HRESULT hr = source.nativeEscape(&request, sizeof(request));
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  owned = false;
  return !std::memcmp(&request, &expected, sizeof(request)) ? S_OK : E_FAIL;
}
}
NativeSurfaceLease::NativeSurfaceLease() : m_retirement(std::make_unique<NativeSurfaceRetirement>()) { }

void NativeSurfaceLease::deferRetirement() {
  if (!m_owned) return;
  auto record = std::move(m_retirement);
  record->initial = m_initial; record->surface = m_surface; record->source = std::move(m_source);
  m_owned = false;
  auto& queue = retirements();
  std::lock_guard<std::mutex> lock(queue.mutex);
  record->next = std::move(queue.pending); queue.pending = std::move(record);
}

HRESULT NativeSurfaceLease::retire(std::shared_ptr<NativeSurfaceLease>& owner) {
  auto lease = std::move(owner);
  // A suspended allocation frame can still need the AHB before returning its
  // wrapper outputs. Its last reference performs the same guarded retirement.
  if (!lease || lease.use_count() != 1) return S_OK;
  const HRESULT hr = lease->close();
  if (hr != S_OK) lease->deferRetirement();
  return hr;
}

HRESULT NativeSurfaceLease::drainRetired(const RuntimeIdentity* identity) {
  auto& queue = retirements();
  std::unique_ptr<NativeSurfaceRetirement> pending;
  {
    std::lock_guard<std::mutex> lock(queue.mutex);
    if (queue.draining) return DXGI_ERROR_WAS_STILL_DRAWING;
    if (!queue.pending) return S_OK;
    queue.draining = true; pending = std::move(queue.pending);
  }
  HRESULT result = S_OK;
  std::unique_ptr<NativeSurfaceRetirement> retained;
  while (pending) {
    auto record = std::move(pending); pending = std::move(record->next);
    bool owned = true;
    if (!identity || (record->initial.luid == identity->luid
        && record->initial.generation == identity->generation)) {
      HRESULT hr = E_FAIL;
      try { hr = freeSurface(*record->source, record->surface, owned); }
      catch (...) { }
      if (hr != S_OK && result == S_OK) result = hr;
    }
    if (owned) { record->next = std::move(retained); retained = std::move(record); }
  }
  {
    std::lock_guard<std::mutex> lock(queue.mutex);
    while (retained) {
      auto record = std::move(retained); retained = std::move(record->next);
      record->next = std::move(queue.pending); queue.pending = std::move(record);
    }
    // An unlocked FREE may trigger another final release. Its failed lease
    // was not in this drain's snapshot, but must still block new acquisition.
    // Do not retry recursively: give the next caller one bounded attempt.
    for (auto* record = queue.pending.get(); result == S_OK && record; record = record->next.get()) {
      if (!identity || (record->initial.luid == identity->luid
          && record->initial.generation == identity->generation))
        result = DXGI_ERROR_WAS_STILL_DRAWING;
    }
    queue.draining = false;
  }
  return result;
}
HRESULT NativeSurfaceLease::identity() const {
  RuntimeIdentity actual;
  const HRESULT hr = m_source->query(actual, RuntimeQueryStage::Continuity);
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  return actual.luid == m_initial.luid && actual.generation == m_initial.generation
    && actual.capabilities == m_initial.capabilities ? S_OK : DXGI_ERROR_DEVICE_REMOVED;
}

HRESULT NativeSurfaceLease::create(const RuntimeIdentity& initial,
    std::shared_ptr<const AdapterIdentitySource> source, uint32_t width, uint32_t height,
    uint32_t fourcc, std::shared_ptr<NativeSurfaceLease>& output) {
  output.reset();
  if (!source || initial.luid == AdapterLuid{} || !initial.generation
      || !width || !height || width > 16384 || height > 16384
      || uint64_t(width) * height * 4 > 256ull * 1024 * 1024
      || (fourcc != 0x34325241 && fourcc != 0x34324241)) return E_INVALIDARG;
  try {
    const HRESULT retired = drainRetired(&initial);
    if (retired != S_OK) return retired;
    auto lease = std::shared_ptr<NativeSurfaceLease>(new NativeSurfaceLease);
    lease->m_initial = initial; lease->m_source = std::move(source);
    HRESULT hr = lease->identity();
    if (hr != S_OK) return hr;
    auto& surface = lease->m_surface;
    surface.width = width; surface.height = height; surface.fourcc = fourcc;
    hr = lease->m_source->nativeEscape(&surface, sizeof(surface));
    // Preserve returned ownership before validation. A rejected reply is never
    // exposed as usable pixels; close uses its exact returned kernel identity.
    lease->m_owned = hr == S_OK && surface.key != 0 && surface.generation != 0;
    if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    if (!validNativeSurface(surface, initial.generation, width, height, fourcc)) return E_FAIL;
    hr = lease->identity();
    if (hr != S_OK) return hr;
    output = std::move(lease);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

NativeSurfaceLease::~NativeSurfaceLease() {
  const HRESULT hr = close();
  if (hr != S_OK) {
    deferRetirement();
    std::fprintf(stderr, "VIOGPU native surface lease retirement pending: %08lx\n", (unsigned long)hr);
  }
}

HRESULT NativeSurfaceLease::close() {
  if (!m_owned) return S_OK;
  if (m_closing) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_closing = true;
  struct Scope { bool& flag; ~Scope() { flag = false; } } scope{m_closing};
  return freeSurface(*m_source, m_surface, m_owned);
}

HRESULT NativeSurfaceLease::resolve(D3DKMT_HANDLE allocation, D3DKMT_HANDLE& resource) const {
  resource = 0;
  if (!m_owned || !allocation) return E_INVALIDARG;
  HRESULT hr = identity();
  if (hr != S_OK) return hr;
  NativeSurfaceResource request;
  request.allocation = allocation; request.key = m_surface.key;
  request.size = m_surface.size; request.generation = m_surface.generation;
  const NativeSurfaceResource expected = request;
  hr = m_source->nativeEscape(&request, sizeof(request));
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  const uint32_t handle = request.resource;
  request.resource = 0;
  if (!handle || std::memcmp(&request, &expected, sizeof(request))) return E_FAIL;
  hr = identity();
  if (hr == S_OK) resource = handle;
  return hr;
}

}
