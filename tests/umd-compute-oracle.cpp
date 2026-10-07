#include "umd-compute-oracle.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "compute oracle check %u line %d\n", checks, __LINE__); std::exit(1); } } while (0)

int main() {
  using namespace dxvk::umd::probe;
  std::array<ComputeElement, ComputeElementCount> values{};
  std::array<unsigned, ComputeElementCount> written{};
  // Independent dispatch simulation iterates groups and then local threads,
  // rather than recovering either from the readback index used by the oracle.
  for (uint32_t groupZ = 0; groupZ < 2; ++groupZ)
    for (uint32_t groupY = 0; groupY < 2; ++groupY)
      for (uint32_t groupX = 0; groupX < 2; ++groupX)
        for (uint32_t localZ = 0; localZ < 2; ++localZ)
          for (uint32_t localY = 0; localY < 3; ++localY)
            for (uint32_t localX = 0; localX < 2; ++localX) {
              const uint32_t x = groupX * 2 + localX;
              const uint32_t y = groupY * 3 + localY;
              const uint32_t z = groupZ * 2 + localZ;
              const uint32_t index = (z * 6 + y) * 4 + x;
              CHECK(index < values.size() && !written[index]++);
              values[index] = {x | (y << 8) | (z << 16),
                groupX | (groupY << 8) | (groupZ << 16),
                localX | (localY << 8) | (localZ << 16),
                localX + 2 * localY + 6 * localZ};
            }
  CHECK(computeReadbackMatches(values));
  for (uint32_t i = 0; i < ComputeElementCount; ++i) {
    CHECK(written[i] == 1 && values[i] == expectedComputeElement(i));
    for (unsigned component = 0; component < 4; ++component) {
      const auto saved = values[i][component];
      for (unsigned bit = 0; bit < 32; ++bit) {
        values[i][component] ^= uint32_t(1) << bit;
        CHECK(!computeReadbackMatches(values));
        values[i][component] = saved;
      }
    }
  }
  CHECK(computeReadbackMatches(values));
  std::printf("typed DX11 compute oracle PASS checks=%u elements=96 words=384; CPU control only\n", checks);
}
