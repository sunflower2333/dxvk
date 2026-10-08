// SPDX-License-Identifier: MIT
// Terminal owned-allocation cleanup uses actual WDK callbacks and guard pages.
#include "../src/umd/umd_allocation.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

static unsigned checks, allocations, deallocations, releases, failures, locks, unlocks, queries;
static DWORD caller;
static bool runtimeValid = true;
static HRESULT deallocateResult = S_OK;
static std::function<void()> queryAction, lockAction, unlockAction, deallocateAction;
static unsigned queryCountdown;
static char deviceCookie, adapterCookie, resourceCookies[4];
static LUID selected{0x13579024, -19};
static std::unordered_map<D3DKMT_HANDLE, std::pair<HANDLE, void*>> owned;
static std::vector<void*> retiredPages;
static D3DKMT_HANDLE nextHandle = 300;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "allocation terminal line=%d: %s\n", __LINE__, #value); std::exit(1); \
} } while (0)
static void callback() { CHECK(runtimeValid && GetCurrentThreadId() == caller); }
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* request) {
  callback(); ++queries; CHECK(runtime == &adapterCookie && request && request->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(request->pPrivateDriverData);
  auto put = [&](unsigned offset, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
  };
  put(0, 0x504d5644, 4); put(8, 128, 4); put(16, 3, 8); put(24, 23, 8);
  put(128, 0x44494c56, 4); put(132, 1, 4); put(136, 32, 4); put(140, 1, 4); put(152, 1, 4);
  std::memcpy(bytes + 144, &selected, sizeof(selected));
  if (queryAction && --queryCountdown == 0) std::exchange(queryAction, {})();
  return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hResource && request->NumAllocations == 1);
  void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); CHECK(page);
  std::memset(page, 0x39, 4096); const auto handle = nextHandle++;
  CHECK(owned.emplace(handle, std::make_pair(request->hResource, page)).second);
  request->pAllocationInfo->hAllocation = handle; request->hKMResource = handle + 1000; ++allocations; return S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hResource);
  auto found = owned.end();
  for (auto entry = owned.begin(); entry != owned.end(); ++entry) if (entry->second.first == request->hResource) found = entry;
  CHECK(found != owned.end()); ++deallocations;
  if (FAILED(deallocateResult)) { ++failures; return deallocateResult; }
  const auto page = found->second.second;
  DWORD previous; CHECK(VirtualProtect(page, 4096, PAGE_NOACCESS, &previous));
  retiredPages.push_back(page); owned.erase(found); ++releases;
  if (deallocateAction) std::exchange(deallocateAction, {})();
  return deallocateResult;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* request) {
  callback(); CHECK(device == &deviceCookie && request && owned.count(request->hAllocation)); ++locks;
  CHECK(request->Flags.LockEntire && !request->Flags.Discard && !request->Flags.IgnoreSync);
  request->pData = owned.at(request->hAllocation).second;
  if (lockAction) std::exchange(lockAction, {})();
  return S_OK;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* request) {
  callback(); CHECK(device == &deviceCookie && request && request->NumAllocations == 1 && request->phAllocations);
  CHECK(owned.count(request->phAllocations[0])); ++unlocks;
  if (unlockAction) std::exchange(unlockAction, {})();
  return S_OK;
}
static HRESULT APIENTRY createContext(HANDLE, D3DDDICB_CREATECONTEXT* request) { CHECK(!request); return E_FAIL; }
static HRESULT APIENTRY destroyContext(HANDLE, const D3DDDICB_DESTROYCONTEXT* request) { CHECK(!request); return E_FAIL; }
static HRESULT APIENTRY present(HANDLE, DXGIDDICB_PRESENT* request) { CHECK(!request); return E_FAIL; }
struct Fixture {
  dxvk::umd::RuntimeMemory memory;
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>();
  DXGI_DDI_BASE_CALLBACKS dxgi{};
  bool closed = false;
  Fixture() {
    runtimeValid = true; dxgi.pfnPresentCb = present;
    auto original = static_cast<D3DDDI_DEVICECALLBACKS*>(VirtualAlloc(nullptr,
      sizeof(D3DDDI_DEVICECALLBACKS), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)); CHECK(original);
    original->pfnAllocateCb = allocate; original->pfnDeallocateCb = deallocate;
    original->pfnLockCb = lock; original->pfnUnlockCb = unlock;
    original->pfnCreateContextCb = createContext; original->pfnDestroyContextCb = destroyContext;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = selected; identity->runtime = &adapterCookie; identity->query = query;
    identity->generation = 23; identity->capabilities = 3;
    memory.initialize(&deviceCookie, *original, &dxgi, identity, service);
    // The copied original prefix is sufficient even when borrowed storage
    // is immediately retired. No late table read can supply a replacement.
    CHECK(VirtualFree(original, 0, MEM_RELEASE));
  }
  HRESULT close() {
    dxvk::umd::RuntimeService::Scope scope(service.get());
    const HRESULT hr = memory.closeDeviceAllocations();
    service->close(); runtimeValid = false; closed = true; return hr;
  }
  ~Fixture() {
    if (!closed) CHECK(close() == S_OK);
    CHECK(owned.empty());
    for (auto page : retiredPages) CHECK(VirtualFree(page, 0, MEM_RELEASE));
    retiredPages.clear(); CHECK(!queryAction && !lockAction && !unlockAction && !deallocateAction);
  }
  void create(dxvk::umd::RuntimeAllocation& allocation, unsigned index) {
    dxvk::umd::RuntimeService::Scope scope(service.get());
    CHECK(memory.allocate(allocation, resourceCookies + index, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
  }
};
static void movedAndOpened() {
  Fixture f; dxvk::umd::RuntimeAllocation a, b, opened;
  f.create(a, 0); f.create(b, 1); dxvk::umd::RuntimeAllocation moved(std::move(a));
  auto info = moved.info();
  { dxvk::umd::RuntimeService::Scope scope(f.service.get()); CHECK(f.memory.adopt(opened, 9001, 10001, info) == S_OK); }
  CHECK(!a.handle() && moved.handle() && b.handle() && opened.opened());
  const auto before = deallocations; CHECK(f.close() == S_OK && deallocations == before + 2);
  CHECK(!moved.handle() && !b.handle() && !opened.handle());
  CHECK(moved.release() == S_OK && b.release() == S_OK && opened.release() == S_OK && a.release() == S_OK);
  CHECK(f.close() == DXGI_ERROR_DEVICE_REMOVED); // No callback service remains.
}
static void reentrantLedgerMutation() {
  Fixture f; dxvk::umd::RuntimeAllocation a, b, c;
  f.create(a, 0); f.create(b, 1); f.create(c, 2); std::optional<dxvk::umd::RuntimeAllocation> moved;
  deallocateAction = [&] {
    // c was detached before this callback; move the current head, then remove
    // a different node. The outer cleanup must follow the actual new links.
    moved.emplace(std::move(b)); CHECK(a.release() == S_OK);
    CHECK(f.memory.closeDeviceAllocations() == S_OK);
  };
  const auto before = deallocations; CHECK(f.close() == S_OK && deallocations == before + 3);
  CHECK(!a.handle() && !b.handle() && !c.handle() && !moved->handle()); moved.reset();
}
static void transferRetirement(unsigned where) {
  Fixture f; dxvk::umd::RuntimeAllocation a; f.create(a, 0);
  std::vector<uint8_t> pixels(24, 0xa6); const auto beforeLock = locks, beforeUnlock = unlocks;
  auto retire = [&] { CHECK(f.close() == S_OK && !a.handle()); };
  if (where < 2) { queryAction = retire; queryCountdown = where + 1; }
  else if (where == 2) lockAction = retire;
  else unlockAction = retire;
  {
    dxvk::umd::RuntimeService::Scope scope(f.service.get());
    CHECK(f.memory.download(a, pixels.data(), 12) == DXGI_ERROR_DEVICE_REMOVED);
  }
  if (where < 3) for (auto byte : pixels) CHECK(byte == 0xa6);
  CHECK(locks == beforeLock + (where ? 1 : 0));
  // where=2 destroys the callback owner while LockCb is still pending. The
  // returned address is already guarded; no stale UnlockCb is permitted.
  CHECK(unlocks == beforeUnlock + ((where == 1 || where == 3) ? 1 : 0));
  CHECK(a.release() == S_OK && owned.empty());
}
static void failedTerminalCallback() {
  Fixture f; dxvk::umd::RuntimeAllocation a; f.create(a, 0);
  deallocateResult = E_OUTOFMEMORY; CHECK(f.close() == E_OUTOFMEMORY); deallocateResult = S_OK;
  CHECK(!a.handle() && a.release() == S_OK && owned.size() == 1);
  // A failed runtime callback was not a release. Preserve that distinction:
  // only this fixture's external runtime-device teardown removes the residual,
  // and the retired UMD makes no callback or success claim afterwards.
  const auto page = owned.begin()->second.second; DWORD previous;
  CHECK(VirtualProtect(page, 4096, PAGE_NOACCESS, &previous));
  retiredPages.push_back(page); owned.clear();
}
int main() {
  caller = GetCurrentThreadId(); movedAndOpened(); reentrantLedgerMutation();
  for (unsigned where = 0; where < 4; ++where) transferRetirement(where);
  failedTerminalCallback();
  CHECK(allocations == 10 && deallocations == 10 && releases == 9 && failures == 1 && locks == 3 && unlocks == 2);
  std::printf("DXGI terminal allocations PASS checks=%u owned=10 opened=1 transfers=4 attempts=10 released=9 failures=1 locks=3 unlocks=2 retired_lock_returns=1\n", checks);
}
