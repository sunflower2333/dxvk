// SPDX-License-Identifier: MIT
#include "../src/umd/umd_multisample_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "multisample policy failure line %d: %s\n", __LINE__, #value); \
  std::abort(); } } while (0)

int main() {
  using dxvk::umd::multisampleQualityLevels;
  // Every accepted sample-count boundary executes the backend once; counts
  // outside the DDI range never enter it, even if it would report support.
  for (uint32_t count = 0; count <= 40; ++count) {
    unsigned calls = 0;
    uint32_t output = 0xdeadbeef;
    CHECK(multisampleQualityLevels(count, output, [&](uint32_t& staged) {
      ++calls; CHECK(staged == 0); CHECK(output == 0);
      staged = 7; return true;
    }));
    CHECK(calls == (count && count <= 32 ? 1u : 0u));
    CHECK(output == (count == 1 ? 1u : count && count <= 32 ? 7u : 0u));
  }
  uint32_t output = 0xdeadbeef;
  unsigned calls = 0;
  CHECK(multisampleQualityLevels(std::numeric_limits<uint32_t>::max(), output,
    [&](uint32_t&) { ++calls; return true; }));
  CHECK(output == 0 && calls == 0);

  // Backend writes on failure and throws after writes must not escape into
  // runtime output. This includes the mandatory single-sample special case.
  const uint32_t counts[] = {1, 2, 3, 4, 16, 32};
  const uint32_t mutations[] = {0, 1, 7, 0xffffffff};
  // The runtime switch preserves reachable post-callback code under /O1;
  // every injected exception must still leave the output cleared below.
  volatile bool injectException = true;
  for (uint32_t count : counts) {
    for (uint32_t mutation : mutations) {
      output = 0xdeadbeef; calls = 0;
      CHECK(!multisampleQualityLevels(count, output, [&](uint32_t& staged) {
        ++calls; staged = mutation; return false;
      }));
      CHECK(output == 0 && calls == 1);
      output = 0xdeadbeef; calls = 0;
      bool caught = false;
      try {
        multisampleQualityLevels(count, output, [&](uint32_t& staged) -> bool {
          ++calls; staged = mutation;
          if (injectException) throw std::runtime_error("injected backend error");
          return false;
        });
      } catch (const std::runtime_error&) { caught = true; }
      CHECK(caught && output == 0 && calls == 1);
    }
  }
  // A successfully queried ordinary format still has one quality at count1
  // when the renderer reports zero or several public API quality levels.
  for (uint32_t mutation : mutations) {
    output = 0xdeadbeef;
    CHECK(multisampleQualityLevels(1, output, [&](uint32_t& staged) {
      staged = mutation; return true;
    }));
    CHECK(output == 1);
  }
  std::printf("native multisample output policy verified checks=%u\n", checks);
}
