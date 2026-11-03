#include "../src/umd/umd_native_surface.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
using namespace dxvk::umd;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "native lease line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
struct Source final : AdapterIdentitySource {
  RuntimeIdentity current;
  mutable unsigned queries = 0, creates = 0, frees = 0, resolves = 0, publishes = 0;
  HRESULT queryResult = S_OK, createResult = S_OK, freeResult = S_OK;
  std::function<void(NativeSurfaceInfo&)> reply;
  std::function<void(NativeSurfaceInfo&)> freed;
  std::function<void(NativeSurfaceResource&)> resolved;
  mutable std::function<void()> onFree;
  Source() { current.luid[0] = 71; current.generation = 2; current.capabilities = 3; }
  HRESULT query(RuntimeIdentity& out, RuntimeQueryStage) const override {
    ++queries; out = current; return queryResult;
  }
  HRESULT nativeEscape(void* data, UINT bytes) const override {
    if (bytes == 128) {
      auto& p = *static_cast<NativeSurfaceInfo*>(data);
      CHECK(p.magic == 0x504d5644 && p.headerSize == 128 && !p.reserved);
      if (p.opcode == 8) {
        ++creates; CHECK(!p.key && !p.expected && !p.generation && !p.resource);
        p.key = 123; p.stride = (p.width * 4 + 255) & ~255u;
        p.size = (uint64_t(p.stride) * p.height + 4095) & ~uint64_t(4095);
        p.generation = current.generation; p.resource = 19; p.planes = p.layoutFlags = 1;
        if (reply) reply(p);
        return createResult;
      }
      CHECK(p.opcode == 9 && p.key == 123 && p.expected == p.generation);
      ++frees;
      if (onFree) { auto callback = std::exchange(onFree, {}); callback(); }
      if (freed) freed(p);
      return freeResult;
    }
    if (bytes == 64) {
      auto& p = *static_cast<NativeSurfaceResource*>(data); ++resolves;
      CHECK(p.opcode == 10 && p.allocation == 32 && p.key == 123 && p.generation == 2);
      p.resource = 44; if (resolved) resolved(p); return S_OK;
    }
    CHECK(bytes == 40);
    auto& p = *static_cast<NativeSurfacePublish*>(data); ++publishes;
    CHECK(p.opcode == 11 && p.key == 123); return S_OK;
  }
};
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("LEASE_CASE create-resolve");
  auto source = std::make_shared<Source>();
  const auto initial = source->current;
  std::shared_ptr<NativeSurfaceLease> lease;
  auto create = [&] { return NativeSurfaceLease::create(initial, source, 3040, 1904, 0x34325241, lease); };
  CHECK(create() == S_OK && lease && source->creates == 1 && source->frees == 0);
  CHECK(validNativeResourceShare(lease->share(), 3040));
  D3DKMT_HANDLE resource = 7;
  CHECK(lease->resolve(0, resource) == E_INVALIDARG && !resource && !source->resolves);
  CHECK(lease->resolve(32, resource) == S_OK && resource == 44);
  source->resolved = [](auto& p) { ++p.generation; };
  CHECK(lease->resolve(32, resource) == E_FAIL && !resource);
  source->resolved = [&](auto&) { ++source->current.generation; };
  CHECK(lease->resolve(32, resource) == DXGI_ERROR_DEVICE_REMOVED && !resource);
  source->resolved = {}; source->current = initial;
  std::puts("LEASE_CASE release-retry-reentry");
  source->onFree = [&] { CHECK(lease->close() == DXGI_ERROR_WAS_STILL_DRAWING); };
  source->freeResult = E_FAIL;
  CHECK(lease->close() == E_FAIL && source->frees == 1);
  source->freeResult = S_OK;
  CHECK(lease->close() == S_OK && source->frees == 2);
  CHECK(lease->close() == S_OK && source->frees == 2);
  CHECK(lease->resolve(32, resource) == E_INVALIDARG && !resource);
  lease.reset(); CHECK(source->frees == 2);
  std::puts("LEASE_CASE rejected-replies");
  for (unsigned kind = 0; kind < 4; ++kind) {
    source->reply = [&](auto& p) {
      if (kind == 0) ++p.magic;
      if (kind == 1) --p.stride;
      if (kind == 2) ++p.generation;
      if (kind == 3) ++source->current.generation;
    };
    const auto before = source->frees;
    CHECK(FAILED(create()) && !lease && source->frees == before + 1);
    source->current = initial;
  }
  source->reply = {};
  std::puts("LEASE_CASE failed-create");
  for (HRESULT failure : {E_FAIL, S_FALSE}) {
    source->createResult = failure;
    const auto before = source->frees;
    CHECK(create() == E_FAIL && !lease && source->frees == before);
  }
  source->createResult = S_OK;
  source->queryResult = S_FALSE;
  const auto before = source->creates;
  CHECK(create() == E_FAIL && !lease && source->creates == before);
  source->queryResult = S_OK;
  std::puts("LEASE_CASE invalid-input");
  CHECK(NativeSurfaceLease::create(initial, {}, 1, 1, 0x34325241, lease) == E_INVALIDARG);
  for (uint32_t bad : {0u, 16385u, UINT32_MAX})
    CHECK(NativeSurfaceLease::create(initial, source, bad, 1, 0x34325241, lease) == E_INVALIDARG);
  CHECK(NativeSurfaceLease::create(initial, source, 1, 1, 0, lease) == E_INVALIDARG);
  CHECK(create() == S_OK);
  std::puts("LEASE_CASE source-lifetime");
  std::weak_ptr<Source> weak = source;
  source.reset(); CHECK(!weak.expired());
  lease.reset(); CHECK(weak.expired());
  std::puts("LEASE_CASE retired-source-lifetime-and-identity");
  source = std::make_shared<Source>();
  CHECK(create() == S_OK);
  source->freeResult = E_FAIL;
  weak = source;
  source.reset();
  lease.reset();
  CHECK(!weak.expired());
  auto retainedSource = weak.lock();
  CHECK(retainedSource->frees == 1);
  auto other = initial; ++other.luid[0];
  CHECK(NativeSurfaceLease::drainRetired(&other) == S_OK && retainedSource->frees == 1);
  other = initial; ++other.generation;
  CHECK(NativeSurfaceLease::drainRetired(&other) == S_OK && retainedSource->frees == 1);
  CHECK(NativeSurfaceLease::drainRetired(&initial) == E_FAIL && retainedSource->frees == 2);
  retainedSource->freeResult = S_OK;
  retainedSource.reset();
  CHECK(NativeSurfaceLease::drainRetired(&initial) == S_OK && weak.expired());
  std::puts("LEASE_CASE malformed-successful-free");
  source = std::make_shared<Source>();
  CHECK(create() == S_OK);
  source->freed = [](auto& p) { ++p.key; };
  CHECK(NativeSurfaceLease::retire(lease) == E_FAIL && !lease && source->frees == 1);
  CHECK(NativeSurfaceLease::drainRetired() == S_OK && source->frees == 1);
  std::printf("PASS native surface lease %u checks\n", checks);
}
