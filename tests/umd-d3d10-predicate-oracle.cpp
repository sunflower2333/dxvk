// SPDX-License-Identifier: MIT
#include "umd-d3d10-predicate-oracle.h"
#include <cstdio>
#include <cstdlib>
using namespace dxvk::umd::predicate10;
static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { std::fprintf(stderr, "predicate oracle line %d: %s\n", __LINE__, #v); std::abort(); } } while (0)

int main() {
  std::array<std::uint8_t, 8> luid{};
  const std::array<std::uint8_t, 8> expected{1,35,69,103,137,171,205,239};
  CHECK(decodeHex("0123456789aBcDeF", luid) && luid == expected);
  CHECK(decodeHex(L"0123456789abcdef", luid) && luid == expected);
  for (const char* text : {"", "0123456789abcde", "0123456789abcdef0", "g123456789abcdef", "+123456789abcdef"}) {
    CHECK(!decodeHex(text, luid)); CHECK(luid == expected);
  }
  CHECK(!decodeHex(static_cast<const char*>(nullptr), luid));
  // Independently list the expected colors, so the test does not mirror the branch formula.
  constexpr const char colors[] = "BRBBBBRRRRRRRRBBBBBBBBRRRRBBBBRRRGGR";
  static_assert(sizeof(colors) - 1 == FrameCount);
  for (unsigned frame = 0; frame < FrameCount; ++frame) {
    const std::array<std::uint8_t, 4> color = colors[frame] == 'R'
      ? std::array<std::uint8_t, 4>{255,0,0,255} : colors[frame] == 'G'
      ? std::array<std::uint8_t, 4>{0,255,0,255} : std::array<std::uint8_t, 4>{0,0,0,255};
    Pixels image{}; for (unsigned i = 0; i < PixelCount; ++i) std::memcpy(image.data() + i * 4, color.data(), 4);
    unsigned mismatches = 1;
    CHECK(readbackMatches(image.data(), image.size(), frame, &mismatches) && !mismatches);
    CHECK(!readbackMatches(nullptr, image.size(), frame));
    CHECK(!readbackMatches(image.data(), image.size() - 1, frame));
    CHECK(!readbackMatches(image.data(), image.size() + 1, frame));
    for (unsigned pixel = 0; pixel < PixelCount; ++pixel) for (unsigned channel = 0; channel < 4; ++channel) {
      auto& byte = image[pixel * 4 + channel]; byte ^= 1;
      CHECK(!readbackMatches(image.data(), image.size(), frame, &mismatches) && mismatches == 1); byte ^= 1;
    }
    CHECK(!readbackMatches(image.data(), image.size(), FrameCount));
  }
  QueryWords q{0,0,0,1, 1,1,0,1, 2,0,0,1}; CHECK(queryWordsMatch(q));
  for (auto& word : q) { word ^= 1; CHECK(!queryWordsMatch(q)); word ^= 1; }
  BindingWords b{}; b[0] = 10000000;
  for (unsigned i = 0; i < BindingCount; ++i) {
    const unsigned k = 1 + i * 8, g = i < 24 ? i / 8 : 2;
    b[k] = i + 2; b[k + 1] = g; b[k + 2] = g == 1;
    b[k + 3] = i < 24 ? i % 8 / 4 : (i - 24) / 4;
    b[k + 4] = i < 24 ? i % 4 : 4 + (i - 24) % 4;
    b[k + 6] = 100; b[k + 7] = 101;
  }
  CHECK(bindingWordsMatch(b)); b[0] = 0; CHECK(!bindingWordsMatch(b)); b[0] = 10000000;
  for (unsigned i = 0; i < BindingCount; ++i) {
    const unsigned k = 1 + i * 8;
    for (unsigned field = 0; field < 6; ++field) { b[k + field] ^= 1; CHECK(!bindingWordsMatch(b)); b[k + field] ^= 1; }
    b[k + 7] = 99; CHECK(!bindingWordsMatch(b)); b[k + 7] = 101;
    // Long real waits are observations; the oracle must not invent a completion bound.
    b[k + 7] = UINT64_MAX; CHECK(bindingWordsMatch(b)); b[k + 7] = 101;
  }
  OwnershipWords o{1,1,1,2,2,3,3,4,2,5,5,0,0,0,0,1}; CHECK(ownershipWordsMatch(o));
  for (unsigned i : {0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u}) {
    auto saved = o[i]; o[i] = i < 11 || i == 15 ? 0 : 1; CHECK(!ownershipWordsMatch(o)); o[i] = saved;
  }
  std::printf("D3D10_PREDICATE_ORACLE_PASS checks=%u frames=36 pixels=9216 hardware_execution=0\n", checks);
}
