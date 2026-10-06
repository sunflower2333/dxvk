#include "../src/umd/umd_runtime_validation.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#ifdef _WIN32
#include "../src/umd/umd_runtime_gpu.h"
static_assert(std::is_same_v<decltype(dxvk::umd::AdapterIdentity::runtime), HANDLE>);
#endif

using namespace dxvk::umd;
static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "runtime backend line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

static int32_t MWD_CALL context(void*, mwd_context_info*) { return 0; }
static int32_t MWD_CALL allocate(void*, uint64_t, uint64_t, uint64_t, uint32_t, mwd_allocation*) { return 0; }
static int32_t MWD_CALL retain(void*, void*, mwd_allocation*) { return 0; }
static int32_t MWD_CALL release(void*, void*) { return 0; }
static int32_t MWD_CALL map(void*, void*, void**, uint32_t*) { return 0; }
static int32_t MWD_CALL submit(void*, const void*, uint32_t, const mwd_reference*, uint32_t) { return 0; }
static int32_t MWD_CALL completed(void*, uint32_t*) { return 0; }
static int32_t MWD_CALL status(void*) { return 0; }

int main() {
  static_assert(!std::is_copy_constructible_v<RuntimeBackendSnapshot>);
  static_assert(!std::is_move_constructible_v<RuntimeBackendSnapshot>);
  mwd_callbacks callbacks = {MWD_RUNTIME_MAGIC, MWD_RUNTIME_ABI_VERSION, sizeof(mwd_callbacks), 0,
    context, allocate, retain, release, map, release, submit, completed, status};
  RuntimeBackend runtime;
  runtime.owner = std::make_shared<int>(73);
  runtime.create = {MWD_STYPE_DEVICE, nullptr, &callbacks, runtime.owner.get()};
  CHECK(validRuntimeBackend(runtime));
  CHECK(!validRuntimeBackend(RuntimeBackend{}));
  for (int i = 0; i < 6; ++i) {
    auto bad = runtime;
    switch (i) {
      case 0: bad.owner.reset(); break;
      case 1: bad.create.owner = nullptr; break;
      case 2: bad.create.owner = &callbacks; break;
      case 3: bad.create.sType = MWD_STYPE_IMPORT; break;
      case 4: bad.create.pNext = &callbacks; break;
      case 5: bad.create.callbacks = nullptr; break;
    }
    CHECK(!validRuntimeBackend(bad));
  }
  for (int i = 0; i < 13; ++i) {
    auto bad = callbacks;
    switch (i) {
      case 0: bad.magic ^= 1; break;
      case 1: ++bad.version; break;
      case 2: --bad.size; break;
      case 3: bad.reserved = 1; break;
      case 4: bad.context = nullptr; break;
      case 5: bad.allocate = nullptr; break;
      case 6: bad.retain = nullptr; break;
      case 7: bad.release = nullptr; break;
      case 8: bad.map = nullptr; break;
      case 9: bad.unmap = nullptr; break;
      case 10: bad.submit = nullptr; break;
      case 11: bad.completed = nullptr; break;
      case 12: bad.status = nullptr; break;
    }
    runtime.create.callbacks = &bad;
    CHECK(!validRuntimeBackend(runtime));
  }
  runtime.create.callbacks = &callbacks;
  const mwd_support support = {MWD_STYPE_SUPPORT, nullptr, MWD_RUNTIME_MAGIC,
    MWD_RUNTIME_ABI_VERSION, sizeof(mwd_callbacks), 1};
  CHECK(supportsRuntimeBackend(support));
  CHECK(!supportsRuntimeBackend(mwd_support{}));
  for (int i = 0; i < 4; ++i) {
    auto bad = support;
    switch (i) {
      case 0: bad.magic ^= 1; break;
      case 1: ++bad.version; break;
      case 2: ++bad.size; break;
      case 3: bad.flags = 3; break;
    }
    CHECK(!supportsRuntimeBackend(bad));
  }
  const AdapterLuid luid = {1,2,3,4,5,6,7,8};
  mwd_context_info info = {};
  std::memcpy(info.luid, luid.data(), luid.size());
  info.generation = 73; info.context_id = 17; info.queue_id = 19;
  CHECK(matchesRuntimeContext(luid, info));
  CHECK(!matchesRuntimeContext(AdapterLuid{}, info));
  for (unsigned i = 0; i < luid.size(); ++i) {
    auto bad = info; bad.luid[i] ^= 1;
    CHECK(!matchesRuntimeContext(luid, bad));
  }
  for (int i = 0; i < 3; ++i) {
    auto bad = info;
    switch (i) {
      case 0: bad.generation = 0; break;
      case 1: bad.context_id = 0; break;
      case 2: bad.queue_id = 0; break;
    }
    CHECK(!matchesRuntimeContext(luid, bad));
  }
  std::weak_ptr<void> weak = runtime.owner;
  void* original = runtime.create.owner;
  {
    RuntimeBackendSnapshot snapshot(&runtime);
    const auto* retained = snapshot.get();
    CHECK(retained && validRuntimeBackend(*retained));
    CHECK(retained->create.callbacks != &callbacks);
    CHECK(retained->create.owner == original);
    runtime = {};
    callbacks = {};
    CHECK(!weak.expired());
    CHECK(validRuntimeBackend(*retained));
    CHECK(retained->create.callbacks->context(original, &info) == 0);
    CHECK(retained->create.callbacks->status(original) == 0);
  }
  CHECK(weak.expired());
  RuntimeBackendSnapshot direct(nullptr);
  CHECK(direct.get() == nullptr);
  std::printf("runtime backend ownership PASS checks=%u; CPU descriptor and lifetime contracts\n", checks);
}
