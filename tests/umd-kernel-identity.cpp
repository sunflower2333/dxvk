#include "../src/umd/umd_kernel_identity.h"
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <io.h>
#include <string>
#include <vector>

static unsigned checks, opens, queries, closes;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "kernel identity check failed line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
static LUID luid = {0x12345678, -45};
static NTSTATUS openStatus, queryStatus;
static bool nullHandle, malformed, changedLuid;
static uint64_t generation = 7, capabilities = 3;
static UINT driverVersion = 1000;
static NTSTATUS versionStatus = 0;
static unsigned versionQueries = 0;
static unsigned escapes = 0;
static NTSTATUS escapeStatus = 0;
static unsigned modeScenario = 0, modeQueries = 0;
static std::vector<D3DKMT_DISPLAYMODE> displayModes;
static NTSTATUS APIENTRY getModes(D3DKMT_GETDISPLAYMODELIST* args) {
  ++modeQueries;
  CHECK(args && args->hAdapter == 0x321 && args->VidPnSourceId == 0);
  if (!args->pModeList) {
    CHECK(args->ModeCount == 0);
    if (modeScenario == 3) return NTSTATUS(0xc0000001);
    args->ModeCount = modeScenario == 1 ? 0 : modeScenario == 2 ? 0xffffffffu : UINT(displayModes.size());
    return 0;
  }
  const UINT capacity = args->ModeCount;
  if (modeScenario == 4) return NTSTATUS(0xc0000001);
  if (modeScenario == 5) return 1;
  if (modeScenario == 6 && capacity == displayModes.size()) {
    args->ModeCount = capacity + 1; return NTSTATUS(0xc0000023);
  }
  if (modeScenario == 7) { args->ModeCount = capacity + 1; return NTSTATUS(0xc0000023); }
  if (modeScenario == 8) { args->ModeCount = capacity + 1; return 0; }
  if (modeScenario == 9) return NTSTATUS(0xc00002b6);
  CHECK(capacity >= displayModes.size());
  std::memcpy(args->pModeList, displayModes.data(), displayModes.size() * sizeof(D3DKMT_DISPLAYMODE));
  args->ModeCount = UINT(displayModes.size());
  if (modeScenario == 10) ++generation;
  if (modeScenario == 11) changedLuid = true;
  if (modeScenario == 13) queryStatus = NTSTATUS(0xc00002b6);
  return 0;
}
static void checkModes(const std::shared_ptr<const dxvk::umd::AdapterIdentitySource>& source) {
  using namespace dxvk::umd;
  D3DKMT_DISPLAYMODE mode = {};
  mode.Width = 3040; mode.Height = 1904; mode.Format = D3DDDIFMT_A8R8G8B8;
  mode.RefreshRate = {165000, 1000}; mode.ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;
  mode.DisplayOrientation = D3DDDI_ROTATION_IDENTITY;
  displayModes = {mode};
  mode.Width = 1920; mode.Height = 1080; mode.RefreshRate = {60000, 1001};
  displayModes.push_back(mode); // Available modes need not be the current desktop.
  PrimaryShape requested{1, 0, 3040, 1904, 87, 3040, 1904, 87, 165, 1, 1, 1, 0};
  CHECK(source->supportsPrimaryMode(requested) == S_OK);
  CHECK(modeQueries == 2);
  auto alternate = requested;
  alternate.width = alternate.modeWidth = 1920; alternate.height = alternate.modeHeight = 1080;
  alternate.numerator = 60000; alternate.denominator = 1001;
  CHECK(source->supportsPrimaryMode(alternate) == S_OK);
  for (unsigned mismatch = 0; mismatch < 7; ++mismatch) {
    auto input = requested;
    if (mismatch == 0) input.width = input.modeWidth = 16;
    if (mismatch == 1) input.numerator = 164;
    if (mismatch == 2) input.denominator = 0;
    if (mismatch == 3) input.format = input.modeFormat = 28;
    if (mismatch == 4) input.scanline = 2;
    if (mismatch == 5) input.rotation = 2;
    if (mismatch == 6) input.scaling = 2;
    CHECK(source->supportsPrimaryMode(input) == DXGI_ERROR_UNSUPPORTED);
  }
  requested.format = requested.modeFormat = 91; // Same encoded BGRA bytes.
  CHECK(source->supportsPrimaryMode(requested) == S_OK);
  requested.format = requested.modeFormat = 87;
  for (unsigned scenario = 1; scenario <= 13; ++scenario) {
    modeScenario = scenario;
    if (scenario == 12) queryStatus = NTSTATUS(0xc00002b6);
    const unsigned before = modeQueries;
    const HRESULT result = source->supportsPrimaryMode(requested);
    if (scenario == 6) CHECK(result == S_OK && modeQueries == before + 3);
    else if (scenario >= 9) CHECK(result == DXGI_ERROR_DEVICE_REMOVED);
    else CHECK(FAILED(result));
    CHECK(modeQueries - before <= 4); // Enumeration changes have a bounded retry.
    if (scenario == 10) --generation;
    changedLuid = false;
    queryStatus = 0;
  }
  modeScenario = 0;
  // Query every time; a previous successful mode must not become a cache.
  displayModes[0].RefreshRate.Numerator = 164000;
  CHECK(source->supportsPrimaryMode(requested) == DXGI_ERROR_UNSUPPORTED);
  std::printf("kernel display mode checks PASS queries=%u\n", modeQueries);
}
static NTSTATUS APIENTRY nativeEscape(const D3DKMT_ESCAPE* args) {
  ++escapes;
  CHECK(args && args->hAdapter == 0x321 && !args->hDevice && !args->hContext
    && args->Type == D3DKMT_ESCAPE_DRIVERPRIVATE && !args->Flags.Value
    && args->pPrivateDriverData && (args->PrivateDriverDataSize == 40
      || args->PrivateDriverDataSize == 64 || args->PrivateDriverDataSize == 128));
  return escapeStatus;
}

static NTSTATUS APIENTRY openAdapter(const D3DKMT_OPENADAPTERFROMLUID* args) {
  ++opens;
  CHECK(args && !std::memcmp(&args->AdapterLuid, &luid, sizeof(luid)) && !args->hAdapter);
  if (openStatus == 0 && !nullHandle)
    const_cast<D3DKMT_OPENADAPTERFROMLUID*>(args)->hAdapter = 0x321;
  return openStatus;
}
static NTSTATUS APIENTRY closeAdapter(const D3DKMT_CLOSEADAPTER* args) {
  ++closes; CHECK(args && args->hAdapter == 0x321); return 0;
}
static NTSTATUS APIENTRY queryAdapter(const D3DKMT_QUERYADAPTERINFO* args) {
  if (args && args->Type == KMTQAITYPE_DRIVERVERSION) {
    ++versionQueries;
    CHECK(args->hAdapter == 0x321 && args->PrivateDriverDataSize == sizeof(UINT));
    CHECK(args->pPrivateDriverData && *static_cast<UINT*>(args->pPrivateDriverData) == 0);
    *static_cast<UINT*>(args->pPrivateDriverData) = driverVersion;
    return versionStatus;
  }
  ++queries;
  CHECK(args && args->hAdapter == 0x321 && args->Type == KMTQAITYPE_UMDRIVERPRIVATE);
  CHECK(args->PrivateDriverDataSize == 160 && args->pPrivateDriverData);
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < 160; ++i) CHECK(bytes[i] == 0);
  if (queryStatus) return queryStatus;
  auto set = [bytes](size_t offset, uint64_t value, unsigned length = 4) {
    for (unsigned i = 0; i < length; ++i) bytes[offset+i] = uint8_t(value >> (i*8));
  };
  set(0, malformed ? 0 : 0x504d5644); set(8, 128);
  set(16, capabilities, 8); set(24, generation, 8);
  set(128, 0x44494c56); set(132, 1); set(136, 32); set(140, 1); set(152, 1);
  std::memcpy(bytes + 144, &luid, sizeof(luid));
  if (changedLuid) bytes[144] ^= 1;
  return 0;
}
int main() {
  using namespace dxvk::umd;
  const KernelIdentityApi api{openAdapter, queryAdapter, closeAdapter, nativeEscape, getModes};
  RuntimeIdentity initial{};
  std::memcpy(initial.luid.data(), &luid, sizeof(luid));
  initial.generation = generation; initial.capabilities = capabilities;
  std::shared_ptr<const AdapterIdentitySource> source;
  CHECK(openKernelIdentityWithApi(initial, api, source) == S_OK && source);
  CHECK(opens == 1 && queries == 1 && closes == 0);
  CHECK(versionQueries == 1 && !source->explicitResidency());
  char packet[128] = {};
  for (unsigned bytes : {40u, 64u, 128u}) CHECK(source->nativeEscape(packet, bytes) == S_OK);
  CHECK(escapes == 3);
  CHECK(source->nativeEscape(nullptr, 128) == E_INVALIDARG);
  CHECK(source->nativeEscape(packet, 88) == E_INVALIDARG);
  CHECK(escapes == 3);
  for (NTSTATUS status : {NTSTATUS(0xc000000d), NTSTATUS(1)}) {
    escapeStatus = status;
    CHECK(source->nativeEscape(packet, 128) == (status < 0 ? HRESULT_FROM_NT(status) : E_FAIL));
  }
  escapeStatus = 0;
  checkModes(source);
  RuntimeIdentity observed;
  CHECK(source->query(observed, RuntimeQueryStage::CreateDeviceBeforeBackend) == S_OK);
  CHECK(observed.luid == initial.luid && observed.generation == generation);
  // The live query observes resets rather than returning the opening cache.
  ++generation;
  CHECK(source->query(observed, RuntimeQueryStage::Continuity) == S_OK);
  CHECK(observed.generation != initial.generation);
  --generation;
  for (NTSTATUS failure : {NTSTATUS(0xc0000001), NTSTATUS(1)}) {
    queryStatus = failure; observed = initial;
    CHECK(source->query(observed, RuntimeQueryStage::Continuity)
      == (failure < 0 ? HRESULT_FROM_NT(failure) : E_FAIL));
    CHECK(observed.luid == AdapterLuid{} && !observed.generation);
  }
  queryStatus = 0;
  auto deviceOwner = source;
  source.reset(); CHECK(closes == 0);
  deviceOwner.reset(); CHECK(closes == 1);
  for (NTSTATUS failure : {NTSTATUS(0xc000000d), NTSTATUS(1)}) {
    openStatus = failure;
    CHECK(FAILED(openKernelIdentityWithApi(initial, api, source)) && !source);
    CHECK(closes == 1);
  }
  openStatus = 0; nullHandle = true;
  CHECK(openKernelIdentityWithApi(initial, api, source) == E_FAIL && !source);
  CHECK(closes == 1); nullHandle = false;
  for (unsigned failure = 0; failure < 5; ++failure) {
    malformed = failure == 0; changedLuid = failure == 1;
    generation += failure == 2; capabilities += failure == 3;
    queryStatus = failure == 4 ? NTSTATUS(0xc0000001) : 0;
    const auto before = closes;
    CHECK(FAILED(openKernelIdentityWithApi(initial, api, source)) && !source);
    CHECK(closes == before + 1);
    generation -= failure == 2; capabilities -= failure == 3;
  }
  queryStatus = 0; malformed = changedLuid = false;
  const auto before = opens;
  CHECK(openKernelIdentityWithApi({}, api, source) == E_INVALIDARG && !source);
  CHECK(openKernelIdentityWithApi(initial, {}, source) == E_INVALIDARG && !source);
  CHECK(opens == before);
  // Diagnostics retain real query/status behavior, preserve error state and
  // stop writing after 64 records even while transport queries continue.
  FILE* output = nullptr;
  CHECK(tmpfile_s(&output) == 0 && output);
  const int saved = _dup(_fileno(stderr));
  CHECK(saved >= 0 && _dup2(_fileno(output), _fileno(stderr)) == 0);
  CHECK(SetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", "0"));
  CHECK(openKernelIdentityWithApi(initial, api, source) == S_OK && source);
  CHECK(std::ftell(output) == 0);
  CHECK(SetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", "1"));
  const auto traceQueries = queries;
  for (unsigned i = 0; i < 70; ++i) {
    queryStatus = i == 1 ? NTSTATUS(0xc0000001) : 0;
    SetLastError(1234); errno = E2BIG;
    const HRESULT result = source->query(observed, RuntimeQueryStage::CreateDeviceBeforeBackend);
    CHECK(GetLastError() == 1234 && errno == E2BIG);
    CHECK(result == (queryStatus ? HRESULT_FROM_NT(queryStatus) : S_OK));
  }
  CHECK(queries == traceQueries + 70);
  std::rewind(output);
  std::string records;
  char buffer[2048];
  while (const size_t n = std::fread(buffer, 1, sizeof(buffer), output)) records.append(buffer, n);
  CHECK(!std::ferror(output) && std::feof(output));
  CHECK(records.find("sequence=2 status=c0000001 hr=d0000001") != std::string::npos);
  CHECK(records.find("identity_valid=0 luid=00000000:00000000 generation=0") != std::string::npos);
  CHECK(records.find("sequence=64 ") != std::string::npos && records.find("sequence=65 ") == std::string::npos);
  CHECK(_dup2(saved, _fileno(stderr)) == 0 && _close(saved) == 0);
  CHECK(std::fclose(output) == 0);
  CHECK(SetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", nullptr));
  source.reset();
  for (UINT version : {1300u, 2000u, 2300u, 3200u}) {
    driverVersion = version;
    CHECK(openKernelIdentityWithApi(initial, api, source) == S_OK && source);
    CHECK(source->explicitResidency() == (version >= 2000));
    source.reset();
  }
  for (NTSTATUS status : {NTSTATUS(0xc0000001), NTSTATUS(1)}) {
    versionStatus = status;
    const auto closed = closes;
    CHECK(FAILED(openKernelIdentityWithApi(initial, api, source)) && !source);
    CHECK(closes == closed + 1);
  }
  versionStatus = 0; driverVersion = 0;
  CHECK(openKernelIdentityWithApi(initial, api, source) == E_FAIL && !source);
  std::printf("kernel identity PASS checks=%u; CPU transport fixture, no GPU\n", checks);
}
