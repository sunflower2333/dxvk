#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_runtime_identity.h"
#include "../src/umd/umd_adapter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static char runtimeCookie;
static unsigned calls;
static HRESULT callbackResult = S_OK;
static UINT returnedSize = dxvk::umd::RuntimeIdentityReplySize;
static bool callbackThrows;
static std::array<uint8_t, dxvk::umd::RuntimeIdentityReplySize> response;
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  calls++;
  if (runtime != &runtimeCookie || !args || args->PrivateDriverDataSize != response.size()) return E_INVALIDARG;
  auto bytes = static_cast<const uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < response.size(); i++) if (bytes[i]) return E_INVALIDARG;
  if (callbackThrows) throw std::bad_alloc();
  std::memcpy(args->pPrivateDriverData, response.data(), response.size());
  const_cast<D3DDDICB_QUERYADAPTERINFO*>(args)->PrivateDriverDataSize = returnedSize;
  return callbackResult;
}

static void traceFlag(const char* value) {
#ifdef _WIN32
  if (!SetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", value)) std::abort();
#else
  if (value ? ::setenv("DXVK_UMD_ADAPTER_QUERY_TRACE", value, 1) : ::unsetenv("DXVK_UMD_ADAPTER_QUERY_TRACE")) std::abort();
#endif
}

static unsigned traceControls(D3D10DDI_HRTADAPTER runtime) {
  using dxvk::umd::RuntimeIdentity;
  using dxvk::umd::RuntimeQueryStage;
  unsigned checks = 0;
  auto check = [&](bool value) { ++checks; if (!value) std::abort(); };
  // This fixture process alone owns its environment and temporary stderr sink.
  // No driver, registration, loader, factory or GPU operation is performed.
  FILE* output = nullptr;
#ifdef _WIN32
  check(::tmpfile_s(&output) == 0 && output);
  const int saved = ::_dup(::_fileno(stderr));
  check(saved >= 0 && ::_dup2(::_fileno(output), ::_fileno(stderr)) == 0);
#else
  output = std::tmpfile(); check(output);
  const int saved = ::dup(::fileno(stderr));
  check(saved >= 0 && ::dup2(::fileno(output), ::fileno(stderr)) >= 0);
#endif
  RuntimeIdentity identity;
  const auto emptyIdentity = [&] {
    return identity.luid == dxvk::umd::AdapterLuid{}
      && identity.generation == 0 && identity.capabilities == 0;
  };
  callbackResult = S_OK;
  const char* flags[] = {nullptr, "0", "true", "1x"};
  for (const char* flag : flags) {
    traceFlag(flag);
    const unsigned before = calls;
    check(dxvk::umd::queryRuntimeIdentity(runtime, query, identity) == S_OK);
    check(calls == before + 1 && identity.generation == 7);
  }
  check(std::fflush(stderr) == 0 && std::ftell(output) == 0);
  traceFlag("1");
  auto run = [&](HRESULT callback, HRESULT expected, RuntimeQueryStage stage) {
    callbackResult = callback;
    const unsigned before = calls;
    identity.generation = 99;
    check(dxvk::umd::queryRuntimeIdentity(runtime, query, identity, stage) == expected);
    check(calls == before + 1);
    check(expected == S_OK ? identity.generation == 7 : emptyIdentity());
  };
  run(S_OK, S_OK, RuntimeQueryStage::OpenAdapter);
  run(E_FAIL, E_FAIL, RuntimeQueryStage::GetCapsThreading);
  run(S_FALSE, E_FAIL, RuntimeQueryStage::GetCapsShader);
  returnedSize = 128;
  run(S_OK, DXGI_ERROR_UNSUPPORTED, RuntimeQueryStage::CalcPrivateDeviceSize);
  returnedSize = dxvk::umd::RuntimeIdentityReplySize;
  callbackThrows = true;
  const unsigned beforeThrow = calls;
  bool thrown = false;
  try {
    dxvk::umd::queryRuntimeIdentity(runtime, query, identity,
      RuntimeQueryStage::CreateDeviceBeforeBackend);
  } catch (const std::bad_alloc&) { thrown = true; }
  check(thrown && calls == beforeThrow + 1 && emptyIdentity());
  callbackThrows = false;
  callbackResult = S_OK;
  // Trace suppression never suppresses a real continuity query.
  const unsigned beforePoll = calls;
  for (unsigned i = 0; i < 70; ++i)
    check(dxvk::umd::queryRuntimeIdentity(runtime, query, identity) == S_OK);
  check(calls == beforePoll + 70);
  check(std::fflush(stderr) == 0);
  std::rewind(output);
  std::string records;
  char buffer[2048];
  while (const size_t bytes = std::fread(buffer, 1, sizeof(buffer), output)) records.append(buffer, bytes);
  check(!std::ferror(output) && std::feof(output));
  check(records.find("event=return stage=get-caps-threading sequence=2") != std::string::npos);
  check(records.find("callback_result_known=1 callback_hr=00000001 result_known=1 hr=80004005") != std::string::npos);
  check(records.find("observed_bytes=128 identity_valid=0") != std::string::npos);
  check(records.find("event=throw stage=create-device-before-backend") != std::string::npos);
  check(records.find("callback_result_known=0") != std::string::npos);
  check(records.find("sequence=64 ") != std::string::npos && records.find("sequence=65 ") == std::string::npos);
#ifdef _WIN32
  check(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
  check(::dup2(saved, ::fileno(stderr)) >= 0 && ::close(saved) == 0);
#endif
  check(std::fclose(output) == 0);
  traceFlag(nullptr);
  return checks;
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
  set(24, 7);
  const unsigned diagnosticChecks = traceControls(runtime);
  std::puts("runtime callback identity consumer PASS; mock replies only");
  std::printf("runtime adapter query diagnostic controls PASS checks=%u; mock replies only\n", diagnosticChecks);
}
