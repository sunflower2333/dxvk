#include "../src/umd/umd_d3d9_api.h"
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>

static unsigned checks, calls;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "D3D9 backend line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)
static int32_t MWD_CALL context(void*, mwd_context_info*) { ++calls; return 0; }
static int32_t MWD_CALL allocate(void*, uint64_t, uint64_t, uint64_t, uint32_t, mwd_allocation*) { ++calls; return 0; }
static int32_t MWD_CALL retain(void*, void*, mwd_allocation*) { ++calls; return 0; }
static int32_t MWD_CALL release(void*, void*) { ++calls; return 0; }
static int32_t MWD_CALL map(void*, void*, void**, uint32_t*) { ++calls; return 0; }
static int32_t MWD_CALL submit(void*, const void*, uint32_t, const mwd_reference*, uint32_t) { ++calls; return 0; }
static int32_t MWD_CALL completed(void*, uint32_t*) { ++calls; return 0; }
static int32_t MWD_CALL status(void*) { ++calls; return 0; }

int main() {
  mwd_callbacks callbacks = {MWD_RUNTIME_MAGIC, MWD_RUNTIME_ABI_VERSION, sizeof(mwd_callbacks), 0,
    context, allocate, retain, release, map, release, submit, completed, status};
  dxvk::umd::RuntimeBackend runtime;
  runtime.owner = std::make_shared<int>(73);
  runtime.create = {MWD_STYPE_DEVICE, nullptr, &callbacks, runtime.owner.get()};
  const LUID luid = {0xf00dcafe, 0x12345678};
  const LUID zero = {};
  UINT output = 0xdeadbeef;
  CHECK(VioGpuDxvkProbeD3D9BackendForTest(nullptr, &runtime, &output) == E_INVALIDARG);
  CHECK(VioGpuDxvkProbeD3D9BackendForTest(&luid, &runtime, nullptr) == E_INVALIDARG);
  CHECK(VioGpuDxvkProbeD3D9BackendForTest(&zero, &runtime, &output) == E_INVALIDARG);
  CHECK(VioGpuDxvkProbeD3D9BackendForTest(&luid, nullptr, &output) == E_INVALIDARG);
  CHECK(output == 0xdeadbeef && calls == 0);
  for (unsigned i = 0; i < 19; ++i) {
    auto bad = runtime;
    auto table = callbacks;
    bad.create.callbacks = &table;
    switch (i) {
      case 0: bad.owner.reset(); break;
      case 1: bad.create.owner = nullptr; break;
      case 2: bad.create.owner = &table; break;
      case 3: bad.create.sType = MWD_STYPE_IMPORT; break;
      case 4: bad.create.pNext = &table; break;
      case 5: bad.create.callbacks = nullptr; break;
      case 6: table.magic ^= 1; break;
      case 7: ++table.version; break;
      case 8: --table.size; break;
      case 9: table.reserved = 1; break;
      case 10: table.context = nullptr; break;
      case 11: table.allocate = nullptr; break;
      case 12: table.retain = nullptr; break;
      case 13: table.release = nullptr; break;
      case 14: table.map = nullptr; break;
      case 15: table.unmap = nullptr; break;
      case 16: table.submit = nullptr; break;
      case 17: table.completed = nullptr; break;
      case 18: table.status = nullptr; break;
    }
    CHECK(VioGpuDxvkProbeD3D9BackendForTest(&luid, &bad, &output) == E_INVALIDARG);
    CHECK(output == 0xdeadbeef && calls == 0);
  }
  // A valid descriptor cannot select an unrelated renderer by ordinal or
  // silently switch to WARP/direct KMT when this exact Turnip LUID is absent.
  CHECK(VioGpuDxvkProbeD3D9BackendForTest(&luid, &runtime, &output) == D3DERR_NOTAVAILABLE);
  CHECK(output == 0xdeadbeef && calls == 0);
  std::printf("D3D9 backend rejection PASS checks=%u; no GPU construction or runtime admission\n", checks);
}
