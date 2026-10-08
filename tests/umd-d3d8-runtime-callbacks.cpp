#include "umd-d3d8-runtime-callbacks.h"
#include "umd-d3d8-runtime-enumeration.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
using Callbacks = dxvk::test::RuntimeCallbacks8;
unsigned checks = 0, forwarded = 0;
unsigned rejected = 0, forbiddenForwarded = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "D3D8 callback CHECK failed line=%u: %s\n", unsigned(__LINE__), #condition); std::_Exit(1); } } while (0)
int runtimeCookie = 0, adapterCookie = 0, contextCookie = 0;
constexpr D3DKMT_HANDLE allocation = 0x11223344;
int mapping = 0;
bool failRelease = false;
std::string callbackTrace;
void trace(const char* format, ...) {
  char line[4096]; va_list args; va_start(args, format);
  const int bytes = std::vsnprintf(line, sizeof(line), format, args); va_end(args);
  CHECK(bytes >= 0 && size_t(bytes) < sizeof(line)); callbackTrace.append(line, size_t(bytes));
}
HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->NumAllocations == 1 && args->pAllocationInfo);
  ++forwarded; args->pAllocationInfo[0].hAllocation = allocation; return S_OK;
}
HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->NumAllocations == 1 && args->HandleList[0] == allocation);
  ++forwarded; return failRelease ? E_FAIL : S_OK;
}
HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->hAllocation == allocation);
  ++forwarded; args->pData = &mapping; return S_OK;
}
HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->NumAllocations == 1 && args->phAllocations[0] == allocation);
  ++forwarded; return S_OK;
}
HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  CHECK(device == &runtimeCookie); CHECK(args != nullptr); ++forwarded; args->hContext = &contextCookie; return S_OK;
}
HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->hContext == &contextCookie); ++forwarded; return S_OK;
}
HRESULT APIENTRY render(HANDLE device, D3DDDICB_RENDER* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->hContext == &contextCookie); ++forwarded; return S_OK;
}
HRESULT APIENTRY residency(HANDLE device, const D3DDDICB_QUERYRESIDENCY* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->NumAllocations == 1 && args->HandleList[0] == allocation);
  ++forwarded; return S_OK;
}
HRESULT APIENTRY present(HANDLE device, D3DDDICB_PRESENT* args) {
  CHECK(device == &runtimeCookie); CHECK(args && args->hSrcAllocation == allocation); ++forwarded; return S_OK;
}
HRESULT APIENTRY escape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
  CHECK(adapter == &adapterCookie); CHECK(args && !args->hDevice); ++forwarded; return S_OK;
}
HRESULT rejectEnumeration(HANDLE device) {
  ++rejected;
  return device == &runtimeCookie ? D3DERR_NOTAVAILABLE : E_INVALIDARG;
}
HRESULT wouldForward(HANDLE) { ++forbiddenForwarded; return S_OK; }
HRESULT APIENTRY state(HANDLE device, const D3DDDIARG_RENDERSTATE*) {
  return device == &runtimeCookie ? S_OK : E_INVALIDARG;
}
HRESULT APIENTRY teardown(HANDLE device) {
  return device == &runtimeCookie ? S_OK : E_INVALIDARG;
}
void enumerationBoundary() {
  using Original = dxvk::test::EnumerationDevice8<wouldForward>;
  using Boundary = dxvk::test::EnumerationDevice8<rejectEnumeration>;
  struct GuardedTable { uint32_t before = 0x12345678; D3DDDI_DEVICEFUNCS value{}; uint32_t after = 0xabcdef01; } table;
#define ORIGINAL(member) table.value.member = Original::Entry<decltype(table.value.member)>::call
  ORIGINAL(pfnDrawPrimitive); ORIGINAL(pfnDrawIndexedPrimitive);
  ORIGINAL(pfnDrawRectPatch); ORIGINAL(pfnDrawTriPatch);
  ORIGINAL(pfnDrawPrimitive2); ORIGINAL(pfnDrawIndexedPrimitive2);
  ORIGINAL(pfnBufBlt); ORIGINAL(pfnTexBlt); ORIGINAL(pfnClear); ORIGINAL(pfnBlt);
  ORIGINAL(pfnColorFill); ORIGINAL(pfnDepthFill); ORIGINAL(pfnGenerateMipSubLevels); ORIGINAL(pfnPresent);
#undef ORIGINAL
  table.value.pfnSetRenderState = state;
  table.value.pfnDestroyDevice = teardown;
  auto* bytes = reinterpret_cast<unsigned char*>(&table.value);
  constexpr size_t prefix = offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME);
  std::memset(bytes + prefix, 0xa5, sizeof(table.value) - prefix);
  const auto original = table.value;
  CHECK(Boundary::publish(table.value) == (0x7fffu & ~(1u << 6)));
  CHECK(table.value.pfnVolBlt == nullptr);
  CHECK(table.value.pfnSetRenderState == original.pfnSetRenderState);
  CHECK(table.value.pfnDestroyDevice == original.pfnDestroyDevice);
  CHECK(!std::memcmp(bytes + prefix, reinterpret_cast<const unsigned char*>(&original) + prefix,
                    sizeof(table.value) - prefix));
  CHECK(table.before == 0x12345678 && table.after == 0xabcdef01);
  CHECK(table.value.pfnDrawPrimitive(&runtimeCookie, nullptr, nullptr) == D3DERR_NOTAVAILABLE);
  CHECK(table.value.pfnDrawIndexedPrimitive(&runtimeCookie, nullptr) == D3DERR_NOTAVAILABLE);
  CHECK(table.value.pfnPresent(&runtimeCookie, nullptr) == D3DERR_NOTAVAILABLE);
  CHECK(table.value.pfnBlt(&runtimeCookie, nullptr) == D3DERR_NOTAVAILABLE);
  CHECK(table.value.pfnClear(&runtimeCookie, nullptr, 0, nullptr) == D3DERR_NOTAVAILABLE);
  CHECK(table.value.pfnPresent(nullptr, nullptr) == E_INVALIDARG);
  CHECK(table.value.pfnSetRenderState(&runtimeCookie, nullptr) == S_OK);
  CHECK(table.value.pfnDestroyDevice(&runtimeCookie) == S_OK);
  CHECK(rejected == 6 && forbiddenForwarded == 0);
}
}
int main() {
  enumerationBoundary();
  D3DDDI_DEVICECALLBACKS callbacks{};
  callbacks.pfnAllocateCb = allocate; callbacks.pfnDeallocateCb = deallocate;
  callbacks.pfnLockCb = lock; callbacks.pfnUnlockCb = unlock;
  callbacks.pfnCreateContextCb = createContext; callbacks.pfnDestroyContextCb = destroyContext;
  callbacks.pfnRenderCb = render; callbacks.pfnQueryResidencyCb = residency;
  callbacks.pfnPresentCb = present; callbacks.pfnEscapeCb = escape;
  const auto original = callbacks;
  const auto owner = Callbacks::install(&runtimeCookie, &adapterCookie, &callbacks, trace);
  CHECK(owner != nullptr); CHECK(!std::memcmp(&callbacks, &original, sizeof(callbacks)));
  CHECK(!Callbacks::install(&runtimeCookie, &adapterCookie, &callbacks, trace));
  callbacks = {}; // Borrowed runtime storage must never be reread.
  D3DDDICB_CREATECONTEXT context{}; CHECK(owner->wrapped.pfnCreateContextCb(&runtimeCookie, &context) == S_OK);
  D3DDDI_ALLOCATIONINFO info{}; D3DDDICB_ALLOCATE request{};
  request.NumAllocations = 1; request.pAllocationInfo = &info;
  CHECK(owner->wrapped.pfnAllocateCb(&runtimeCookie, &request) == S_OK); CHECK(info.hAllocation == allocation);
  D3DDDICB_LOCK mapped{}; mapped.hAllocation = allocation;
  CHECK(owner->wrapped.pfnLockCb(&runtimeCookie, &mapped) == S_OK); CHECK(mapped.pData == &mapping);
  // A BO may stay mapped while resident and submitted. Observers cannot reject
  // or unlock it early; paired Unlock follows the actual native teardown path.
  D3DDDICB_QUERYRESIDENCY resident{}; resident.NumAllocations = 1; resident.HandleList = &info.hAllocation;
  CHECK(owner->wrapped.pfnQueryResidencyCb(&runtimeCookie, &resident) == S_OK);
  D3DDDICB_RENDER rendered{}; rendered.hContext = context.hContext;
  CHECK(owner->wrapped.pfnRenderCb(&runtimeCookie, &rendered) == S_OK);
  D3DDDICB_PRESENT presented{}; presented.hSrcAllocation = allocation;
  CHECK(owner->wrapped.pfnPresentCb(&runtimeCookie, &presented) == S_OK);
  D3DDDICB_ESCAPE escaped{}; CHECK(owner->wrapped.pfnEscapeCb(&adapterCookie, &escaped) == S_OK);
  Callbacks::summary(owner, "live");
  CHECK(callbackTrace.find("live_allocations=1 live_locks=1 live_contexts=1 tracking_errors=0") != std::string::npos);
  D3DDDICB_UNLOCK unmapped{}; unmapped.NumAllocations = 1; unmapped.phAllocations = &info.hAllocation;
  CHECK(owner->wrapped.pfnUnlockCb(&runtimeCookie, &unmapped) == S_OK);
  D3DDDICB_DEALLOCATE released{}; released.NumAllocations = 1; released.HandleList = &info.hAllocation;
  failRelease = true; CHECK(owner->wrapped.pfnDeallocateCb(&runtimeCookie, &released) == E_FAIL);
  Callbacks::summary(owner, "failed-release");
  CHECK(callbackTrace.find("live_allocations=1 live_locks=0 live_contexts=1 tracking_errors=0 callback_failures=1") != std::string::npos);
  failRelease = false; CHECK(owner->wrapped.pfnDeallocateCb(&runtimeCookie, &released) == S_OK);
  D3DDDICB_DESTROYCONTEXT ended{}; ended.hContext = context.hContext;
  CHECK(owner->wrapped.pfnDestroyContextCb(&runtimeCookie, &ended) == S_OK);
  Callbacks::summary(owner, "released");
  CHECK(callbackTrace.find("allocate=1 deallocate=1 lock=1 unlock=1 create_context=1 destroy_context=1 render=1 present=1 residency=1 live_allocations=0 live_locks=0 live_contexts=0 tracking_errors=0 callback_failures=1") != std::string::npos);
  const unsigned before = forwarded;
  Callbacks::remove(owner);
  CHECK(owner->wrapped.pfnRenderCb(&runtimeCookie, &rendered) == E_INVALIDARG); CHECK(forwarded == before);
  std::printf("D3D8 runtime callback policy PASS checks=%u forwarded=%u immutable_table=1 mapped_submit=1 failed_release_retained=1 stale_owner_rejected=1; controlled CPU only\n", checks, forwarded);
  std::printf("D3D8 enumeration table boundary PASS denied=6 forwarded=0 prefix=99 tail_unchanged=1 optional_null=1 teardown_allowed=1; controlled CPU only\n");
}
