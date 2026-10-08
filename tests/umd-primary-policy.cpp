// SPDX-License-Identifier: MIT
#include "../src/umd/umd_primary_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace dxvk::umd;
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "primary policy line=%d: %s\n", __LINE__, #value); std::abort(); \
} } while (0)
static PrimaryShape shape(uint32_t width, uint32_t height, uint32_t format = 28) {
  return {0, 0, width, height, format, width, height, format, 60000, 1001, 1, 1, 0};
}
static void reject(PrimaryShape input, PrimaryStatus expected) {
  PrimaryPlan output; output.scanout = true; output.driverFlags = 0xdeadbeef;
  output.pitch = 0xabcdef; output.bytes = 0x123456789abcdefull;
  const auto before = output;
  CHECK(primaryPlan(input, output) == expected);
  CHECK(output.scanout == before.scanout && output.driverFlags == before.driverFlags
    && output.pitch == before.pitch && output.bytes == before.bytes);
}
int main() {
  for (const uint32_t width : {1u, 3u, 17u, 127u, 1920u, 16384u})
    for (const uint32_t height : {1u, 2u, 31u, 1080u, 16384u})
      for (const uint32_t format : {28u, 29u, 87u, 88u, 91u, 93u})
        for (uint32_t flags = 0; flags <= 15; ++flags) {
          auto input = shape(width, height, format); input.flags = flags;
          // Independent cases from the SDK: stereo/indirect are unavailable;
          // optional copies accept sRGB; real BGRA sRGB uses encoded BGRA8.
          const bool valid = flags < 4 && (format == 28 || format == 87 || format == 88 || format == 91
            || ((flags & 1) && (format == 29 || format == 93)));
          if (!valid) { reject(input, PrimaryStatus::Unsupported); continue; }
          PrimaryPlan plan;
          CHECK(primaryPlan(input, plan) == PrimaryStatus::Valid);
          CHECK(plan.scanout == !(flags & 1) && plan.driverFlags == (flags & 1));
          CHECK(plan.pitch == width * 4 && plan.bytes == uint64_t(width) * height * 4);
        }
  for (unsigned field = 0; field < 16; ++field) {
    auto input = shape(8, 4);
    switch (field) {
      case 0: input.flags = 16; break;
      case 1: input.width = input.modeWidth = 0; break;
      case 2: input.height = input.modeHeight = 0; break;
      case 3: input.width = input.modeWidth = 16385; break;
      case 4: input.height = input.modeHeight = 16385; break;
      case 5: input.width = input.modeWidth = std::numeric_limits<uint32_t>::max(); break;
      case 6: input.modeWidth++; break;
      case 7: input.modeHeight++; break;
      case 8: input.modeFormat = 87; break;
      case 9: input.numerator = 0; break;
      case 10: input.denominator = 0; break;
      case 11: input.scanline = 4; break;
      case 12: input.rotation = 5; break;
      case 13: input.scaling = 3; break;
      case 14: input.modeWidth = std::numeric_limits<uint32_t>::max(); break;
      case 15: input.modeHeight = std::numeric_limits<uint32_t>::max(); break;
    }
    reject(input, PrimaryStatus::Invalid);
  }
  for (unsigned field = 0; field < 5; ++field) {
    auto input = shape(8, 4);
    if (field == 0) input.source = 1;
    if (field == 1) input.scanline = 2;
    if (field == 2) input.scanline = 3;
    if (field == 3) input.rotation = 2;
    if (field == 4) input.scaling = 2;
    reject(input, PrimaryStatus::Unsupported);
  }
  for (uint32_t format = 0; format <= 190; ++format) {
    if (format == 28 || format == 29 || format == 87 || format == 88 || format == 91 || format == 93) continue;
    auto input = shape(8, 4, format); input.flags = 1; reject(input, PrimaryStatus::Unsupported);
  }
  // Optional primaries have no scanout refresh requirement. Exact byte
  // arithmetic remains defined for a maximum-sized copy image.
  auto optional = shape(16384, 16384); optional.flags = 3;
  optional.numerator = optional.denominator = 0; PrimaryPlan plan;
  CHECK(primaryPlan(optional, plan) == PrimaryStatus::Valid);
  CHECK(!plan.scanout && plan.driverFlags == 1 && plan.pitch == 65536 && plan.bytes == 1073741824);
  PrimaryCopy copy; copy.width = 19; copy.height = 7;
  std::array<uint32_t, 16> words{}; std::memcpy(words.data(), &copy, sizeof(copy));
  CHECK((words == std::array<uint32_t, 16>{0x504d5644, 0, 64, 0, 2, 0, 19, 7, 0, 0, 0, 0, 0, 0, 0, 0}));
  std::printf("DXGI primary policy PASS checks=%u\n", checks);
}
