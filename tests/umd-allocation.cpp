#include "../src/umd/umd_allocation.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>

using dxvk::umd::RuntimeAllocation;
using dxvk::umd::RuntimeMemory;
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { \
  std::fprintf(stderr, "allocation check %u line %d: %s\n", checks, __LINE__, #c); \
  std::exit(1); } } while (0)

static char deviceCookie, resourceCookie, contextCookie, dxgiCookie;
static std::vector<char> calls;
static unsigned char backing[24];
static HRESULT allocateResult = S_OK, lockResult = S_OK, unlockResult = S_OK;
static HRESULT contextResult = S_OK, presentResult = S_OK, deallocateResult = S_OK;
static bool zeroAllocation, zeroResource, nullLock, changedLock, zeroContext;
static UINT requestedFormat = 3;
static RuntimeAllocation* releaseReentry;
static RuntimeMemory* closeReentry;
static char adapterCookie, resetDuring;
static uint64_t generation = 42, capabilities = 3;
static LUID luid = {0x12345678, -97};
static HRESULT queryResult = S_OK;
static unsigned queries;
static RuntimeMemory* queryReentry;

static HRESULT APIENTRY query(HANDLE adapter, const D3DDDICB_QUERYADAPTERINFO* args) {
  CHECK(adapter == &adapterCookie && args && args->PrivateDriverDataSize == 160);
  ++queries;
  if (queryReentry) {
    auto memory = std::exchange(queryReentry, nullptr);
    RuntimeAllocation nested;
    CHECK(memory->allocate(nested, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == DXGI_ERROR_WAS_STILL_DRAWING);
  }
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(!bytes[i]);
  auto set = [bytes](unsigned offset, uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) bytes[offset+i] = uint8_t(value >> (8*i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, capabilities, 8); set(24, generation, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes + 144, &luid, sizeof(luid));
  return queryResult;
}

static void maybeReset(char phase) { if (resetDuring == phase) ++generation; }

static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  calls.push_back('A');
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(args->NumAllocations == 1 && args->pAllocationInfo);
  auto& allocation = args->pAllocationInfo[0];
  CHECK(!allocation.hAllocation && !allocation.Flags.Primary);
  CHECK(allocation.PrivateDriverDataSize == 80 && allocation.pPrivateDriverData);
  dxvk::umd::AllocationInfo info;
  std::memcpy(&info, allocation.pPrivateDriverData, sizeof(info));
  CHECK(info.magic == 0x504d5644 && info.version == 0 && info.headerSize == 80 && !info.reserved);
  CHECK(info.size == 24 && info.alignment == 4096 && info.flags == 2);
  CHECK(info.width == 3 && info.height == 2 && info.pitch == 12 && info.format == requestedFormat);
  CHECK(!info.requestedIova && !info.resetGeneration && !info.contextId);
  CHECK(!info.refreshNumerator && !info.refreshDenominator);
  // Output private data must not redefine the bridge's trusted copy bounds.
  std::memset(allocation.pPrivateDriverData, 0xff, sizeof(info));
  allocation.hAllocation = zeroAllocation ? 0 : 123;
  args->hKMResource = zeroResource ? 0 : 456;
  maybeReset('A');
  return allocateResult;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  calls.push_back('D');
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(!args->NumAllocations && !args->HandleList);
  if (releaseReentry) {
    auto allocation = std::exchange(releaseReentry, nullptr);
    CHECK(!allocation->handle() && !allocation->kernelResource()
      && !allocation->runtimeResource() && !allocation->generation());
    CHECK(allocation->release() == S_OK);
  }
  return deallocateResult;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  calls.push_back('L');
  CHECK(device == &deviceCookie && args && args->hAllocation == 123);
  CHECK(args->Flags.LockEntire && args->Flags.WriteOnly && !args->Flags.ReadOnly);
  CHECK(!args->Flags.Discard && !args->Flags.IgnoreSync && !args->Flags.DonotWait);
  CHECK(!args->NumPages && !args->pPages && !args->pData);
  args->pData = nullLock ? nullptr : backing;
  if (changedLock) args->hAllocation = 124;
  maybeReset('L');
  return lockResult;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  calls.push_back('U');
  CHECK(device == &deviceCookie && args && args->NumAllocations == 1 && args->phAllocations);
  CHECK(*args->phAllocations == (changedLock ? 124 : 123));
  maybeReset('U');
  return unlockResult;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  calls.push_back('C');
  CHECK(device == &deviceCookie && args && args->EngineAffinity == 1 && !args->NodeOrdinal);
  CHECK(!args->pPrivateDriverData && !args->PrivateDriverDataSize);
  args->hContext = zeroContext ? nullptr : &contextCookie;
  maybeReset('C');
  return contextResult;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  calls.push_back('X');
  CHECK(device == &deviceCookie && args && args->hContext == &contextCookie);
  if (closeReentry) {
    auto memory = std::exchange(closeReentry, nullptr);
    CHECK(memory->close() == S_OK);
  }
  return S_OK;
}
static HRESULT APIENTRY present(HANDLE device, DXGIDDICB_PRESENT* args) {
  calls.push_back('P');
  CHECK(device == &deviceCookie && args && args->hSrcAllocation == 123 && !args->hDstAllocation);
  CHECK(args->pDXGIContext == &dxgiCookie && args->hContext == &contextCookie);
  CHECK(!args->BroadcastContextCount);
  maybeReset('P');
  return presentResult;
}

static void sequence(const char* expected) {
  CHECK(calls.size() == std::strlen(expected));
  CHECK(calls.empty() || std::memcmp(calls.data(), expected, calls.size()) == 0);
  calls.clear();
}

int main() {
  D3DDDI_DEVICECALLBACKS callbacks = {};
  callbacks.pfnAllocateCb = allocate; callbacks.pfnDeallocateCb = deallocate;
  callbacks.pfnLockCb = lock; callbacks.pfnUnlockCb = unlock;
  callbacks.pfnCreateContextCb = createContext; callbacks.pfnDestroyContextCb = destroyContext;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = present;
  RuntimeMemory memory;
  RuntimeAllocation allocation;
  CHECK(!memory.available());
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == DXGI_ERROR_UNSUPPORTED);
  memory.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(memory.available());
  CHECK(memory.allocate(allocation, nullptr, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 0, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 0, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 16385, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R16G16B16A16_FLOAT) == DXGI_ERROR_UNSUPPORTED);
  sequence("");
  allocateResult = E_OUTOFMEMORY;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_OUTOFMEMORY);
  CHECK(!allocation.handle()); sequence("A"); allocateResult = S_OK;
  allocateResult = S_FALSE;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); allocateResult = S_OK;
  zeroAllocation = true;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); zeroAllocation = false;
  zeroResource = true;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); zeroResource = false;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
  CHECK(allocation.handle() == 123); sequence("A");
  CHECK(allocation.kernelResource() == 456 && allocation.runtimeResource() == &resourceCookie);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  DXGI_DDI_ARG_PRESENT args = {};
  args.pDXGIContext = &dxgiCookie; args.Flags.Blt = 1;
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("");
  unsigned char pixels[32];
  for (unsigned i = 0; i < sizeof(pixels); i++) pixels[i] = static_cast<unsigned char>(i);
  CHECK(memory.upload(allocation, nullptr, 16) == E_INVALIDARG);
  CHECK(memory.upload(allocation, pixels, 11) == E_INVALIDARG); sequence("");
  CHECK(memory.upload(allocation, pixels, 16) == S_OK); sequence("LU");
  CHECK(!std::memcmp(backing, pixels, 12) && !std::memcmp(backing + 12, pixels + 16, 12));
  CHECK(memory.present(allocation, args) == S_OK); sequence("CP");
  CHECK(memory.present(allocation, args) == S_OK); sequence("P");
  auto bad = args; bad.Flags.Flip = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.SrcSubResourceIndex = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.DstSubResourceIndex = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.hDstResource = reinterpret_cast<UINT_PTR>(&resourceCookie);
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.pDXGIContext = nullptr;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG); sequence("");
  presentResult = DXGI_ERROR_DEVICE_REMOVED;
  CHECK(memory.present(allocation, args) == DXGI_ERROR_DEVICE_REMOVED); sequence("P"); presentResult = S_OK;
  lockResult = DXGI_DDI_ERR_WASSTILLDRAWING;
  CHECK(memory.upload(allocation, pixels, 16) == DXGI_DDI_ERR_WASSTILLDRAWING);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("L"); lockResult = S_OK;
  nullLock = true;
  CHECK(memory.upload(allocation, pixels, 16) == E_FAIL);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("LU"); nullLock = false;
  changedLock = true;
  CHECK(memory.upload(allocation, pixels, 16) == E_FAIL); sequence("LU"); changedLock = false;
  unlockResult = DXGI_ERROR_DEVICE_REMOVED;
  CHECK(memory.upload(allocation, pixels, 16) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("LU"); unlockResult = S_OK;
  CHECK(memory.upload(allocation, pixels, 16) == S_OK); sequence("LU");
  CHECK(memory.close() == S_OK); sequence("X");
  contextResult = E_OUTOFMEMORY;
  CHECK(memory.present(allocation, args) == E_OUTOFMEMORY); sequence("C"); contextResult = S_OK;
  zeroContext = true;
  CHECK(memory.present(allocation, args) == E_FAIL); sequence("C"); zeroContext = false;
  CHECK(memory.present(allocation, args) == S_OK); sequence("CP");
  RuntimeMemory other;
  other.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(other.upload(allocation, pixels, 16) == E_INVALIDARG);
  CHECK(other.present(allocation, args) == E_INVALIDARG);
  CHECK(other.release(allocation) == E_INVALIDARG); sequence("");
  releaseReentry = &allocation;
  CHECK(allocation.release() == S_OK && !allocation.handle()); sequence("D");
  CHECK(allocation.release() == S_OK); sequence("");
  requestedFormat = 1;
  {
    RuntimeAllocation bgra;
    CHECK(memory.allocate(bgra, &resourceCookie, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM) == S_OK);
  }
  sequence("AD");
  closeReentry = &memory;
  CHECK(memory.close() == S_OK); sequence("X");
  CHECK(memory.close() == S_OK); sequence("");
  callbacks.pfnUnlockCb = nullptr;
  other.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(!other.available());
  CHECK(other.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM) == DXGI_ERROR_UNSUPPORTED);
  sequence("");

  callbacks.pfnUnlockCb = unlock; requestedFormat = 3;
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid = luid; identity->generation = generation;
  identity->capabilities = capabilities; identity->runtime.handle = &adapterCookie;
  identity->query = query;
  for (char phase : {'A', 'L', 'U', 'C', 'P'}) {
    RuntimeMemory bound;
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    RuntimeAllocation owned;
    generation = identity->generation;
    resetDuring = phase == 'A' ? 'A' : 0;
    queryReentry = &bound;
    HRESULT hr = bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (phase == 'A') {
      CHECK(hr == DXGI_ERROR_DEVICE_REMOVED && !owned.handle()); sequence("AD");
    } else {
      CHECK(hr == S_OK && owned.generation() == identity->generation);
      CHECK(owned.kernelResource() == 456 && owned.runtimeResource() == &resourceCookie);
      sequence("A");
      resetDuring = phase;
      std::memset(backing, 0x5a, sizeof(backing));
      hr = bound.upload(owned, pixels, 16);
      if (phase == 'L' || phase == 'U') {
        CHECK(hr == DXGI_ERROR_DEVICE_REMOVED); sequence("LU");
        if (phase == 'L') for (auto value : backing) CHECK(value == 0x5a);
      } else {
        CHECK(hr == S_OK); sequence("LU");
        CHECK(bound.present(owned, args) == DXGI_ERROR_DEVICE_REMOVED);
        sequence(phase == 'C' ? "C" : "CP");
      }
      // Callback success followed by reset never admits another publication,
      // even if the callback later reports the old generation again.
      generation = identity->generation; resetDuring = 0;
      CHECK(bound.upload(owned, pixels, 16) == DXGI_ERROR_DEVICE_REMOVED); sequence("");
      RuntimeAllocation moved(std::move(owned));
      CHECK(!owned.handle() && moved.handle() == 123 && moved.kernelResource() == 456);
      CHECK(moved.release() == S_OK); sequence("D");
      CHECK(bound.close() == S_OK); sequence(phase == 'C' || phase == 'P' ? "X" : "");
    }
    resetDuring = 0; generation = identity->generation;
    CHECK(bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == DXGI_ERROR_DEVICE_REMOVED); sequence("");
  }
  for (unsigned mismatch = 0; mismatch < 3; ++mismatch) {
    RuntimeMemory bound;
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    RuntimeAllocation owned;
    if (mismatch == 0) ++luid.LowPart;
    if (mismatch == 1) ++capabilities;
    if (mismatch == 2) queryResult = S_FALSE;
    CHECK(bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == (mismatch == 2 ? E_FAIL : DXGI_ERROR_DEVICE_REMOVED)); sequence("");
    luid = identity->luid; capabilities = identity->capabilities; queryResult = S_OK;
  }
  CHECK(queries > 20);
  std::printf("runtime allocation/presentation PASS checks=%u; mock callbacks, no GPU\n", checks);
}
