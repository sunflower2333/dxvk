#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_runtime_identity.h"
#include <cstdio>
#include <cstring>

static char runtimeCookie;
static unsigned calls;
static HRESULT callbackResult = S_OK;
static std::array<uint8_t, dxvk::umd::RuntimeIdentityReplySize> response;
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  calls++;
  if (runtime != &runtimeCookie || !args || args->PrivateDriverDataSize != response.size()) return E_INVALIDARG;
  auto bytes = static_cast<const uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < response.size(); i++) if (bytes[i]) return E_INVALIDARG;
  std::memcpy(args->pPrivateDriverData, response.data(), response.size());
  return callbackResult;
}
int main() {
  LUID luid = {};
  D3D10DDI_HRTADAPTER runtime = {&runtimeCookie};
  if (VioGpuDxvkQueryRuntimeAdapterLuid({}, query, &luid) != E_INVALIDARG || calls) return 1;
  if (VioGpuDxvkQueryRuntimeAdapterLuid(runtime, nullptr, &luid) != E_INVALIDARG || calls) return 2;
  auto set = [](size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; i++) response[offset+i] = uint8_t(value >> (i*8));
  };
  set(0,0x504d5644); set(8,128);
  if (VioGpuDxvkQueryRuntimeAdapterLuid(runtime, query, &luid) != DXGI_ERROR_UNSUPPORTED) return 3;
  set(128,0x44494c56); set(132,1); set(136,32); set(140,1); set(144,0x12345678); set(152,1);
  if (FAILED(VioGpuDxvkQueryRuntimeAdapterLuid(runtime, query, &luid)) || luid.LowPart != 0x12345678 || luid.HighPart) return 4;
  callbackResult = E_FAIL;
  if (VioGpuDxvkQueryRuntimeAdapterLuid(runtime, query, &luid) != E_FAIL || luid.LowPart || luid.HighPart) return 5;
  std::puts("runtime callback identity consumer PASS; mock replies only");
}
