#include "../src/umd/umd_private_children.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "private children line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
struct Epoch { };
struct Child {
  std::atomic<unsigned>& destroyed;
  unsigned value;
  Child(std::atomic<unsigned>& count, unsigned tag) : destroyed(count), value(tag) { }
  ~Child() { ++destroyed; }
};
int main() {
  dxvk::umd::PrivateChildren<Child, Epoch> registry;
  std::atomic<unsigned> destroyed{0};
  auto epoch = std::make_shared<Epoch>(), foreign = std::make_shared<Epoch>();
  std::array<unsigned char, 32> storage; storage.fill(0xa5); const auto original = storage;
  const auto key = storage.data();
  CHECK(!registry.lookup(reinterpret_cast<void*>(1), epoch));
  CHECK(!registry.begin(nullptr, epoch) && !registry.begin(key, {}));
  { auto failed = registry.begin(key, epoch); CHECK(failed && !registry.lookup(key, epoch)); }
  auto create = registry.begin(key, epoch); CHECK(create);
  CHECK(!registry.begin(key, epoch) && !registry.begin(key, foreign));
  auto first = std::make_shared<Child>(destroyed, 19); CHECK(create.publish(first));
  CHECK(!create.publish(first) && registry.lookup(key, epoch) == first && !registry.lookup(key, foreign));
  auto pinned = registry.lookup(key, epoch);
  CHECK(!registry.remove(key, foreign, first));
  CHECK(registry.remove(key, epoch, first) == first && !registry.lookup(key, epoch));
  std::weak_ptr<Child> weak = first; first.reset(); CHECK(!weak.expired() && !destroyed);
  {
    auto reused = registry.begin(key, epoch); auto second = std::make_shared<Child>(destroyed, 29);
    CHECK(reused && reused.publish(second));
    CHECK(!registry.remove(key, epoch, pinned));
    CHECK(registry.lookup(key, epoch) == second && pinned->value == 19);
  }
  pinned.reset(); CHECK(weak.expired() && destroyed == 1);
  // Cleanup may itself enter another child operation; it runs without the
  // registry lock and the retired child remains pinned throughout it.
  registry.clear(epoch, [&](auto child) {
    CHECK(child->value == 29 && !registry.lookup(key, epoch));
    CHECK(registry.begin(storage.data()+8, foreign));
  });
  CHECK(destroyed == 2 && storage == original);
  // A creator suspended while device cleanup runs must neither publish nor
  // cancel a new device's child at the exact same runtime storage address.
  {
    auto suspended = registry.begin(key, epoch); CHECK(suspended);
    registry.clear(epoch, [](auto) { std::abort(); });
    auto replacement = registry.begin(key, foreign);
    auto child = std::make_shared<Child>(destroyed, 31); CHECK(replacement && replacement.publish(child));
    CHECK(!suspended.publish(std::make_shared<Child>(destroyed, 37)));
  }
  CHECK(registry.lookup(key, foreign)->value == 31 && !registry.lookup(key, epoch));
  registry.clear(foreign, [](auto) { });
  CHECK(destroyed == 4 && storage == original);
  auto replacement = [&] {
    auto suspended = registry.begin(key, epoch); CHECK(suspended);
    registry.clear(epoch, [](auto) { std::abort(); });
    auto fresh = registry.begin(key, epoch); CHECK(fresh);
    CHECK(!suspended.publish(std::make_shared<Child>(destroyed, 39)));
    return fresh;
  }();
  // The old creator's destructor ran while the new ticket was still pending.
  CHECK(replacement.publish(std::make_shared<Child>(destroyed, 40)));
  registry.clear(epoch, [](auto child) { CHECK(child->value == 40); });
  CHECK(destroyed == 6);
  // Concurrent readers retain their own object pins across key retirement
  // and immediate reuse. No callable state resides in the runtime bytes.
  std::atomic<bool> stop{false}, invalid{false};
  std::thread reader([&] {
    while (!stop) {
      auto child = registry.lookup(key, epoch);
      if (child && child->value != 41 && child->value != 43) invalid = true;
    }
  });
  for (unsigned i = 0; i < 2000; ++i) {
    auto reservation = registry.begin(key, epoch);
    auto child = std::make_shared<Child>(destroyed, i % 2 ? 41 : 43);
    CHECK(reservation && reservation.publish(child));
    CHECK(registry.remove(key, epoch, child) == child);
  }
  stop = true; reader.join();
  CHECK(!invalid && destroyed == 2006 && storage == original);
  std::printf("private children PASS checks=%u; concurrent pins/reuse/rollback/epoch cleanup, no GPU\n", checks);
}
