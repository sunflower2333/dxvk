// SPDX-License-Identifier: MIT
#include "umd-cube-probe-oracle.h"
#include <cstdio>
#include <cstdlib>

using namespace dxvk::umd::probe::cube;
unsigned checks, flips;
void check(bool passed) { ++checks; if (!passed) std::abort(); }
void adversaries(const std::vector<uint32_t>& expected) {
  auto actual = expected;
  check(matches(actual, expected));
  for (unsigned word = 0; word < actual.size(); ++word) for (unsigned bit = 0; bit < 32; ++bit) {
    actual[word] ^= uint32_t(1) << bit;
    check(!matches(actual, expected)); ++flips;
    actual[word] ^= uint32_t(1) << bit;
  }
  actual.push_back(0); check(!matches(actual, expected));
  actual = expected; actual.pop_back(); check(!matches(actual, expected));
  if (expected.size() > 1 && expected[0] != expected[1]) {
    actual = expected; const auto first = actual.front(); actual.front() = actual.back(); actual.back() = first;
    check(!matches(actual, expected));
  }
}
int main() {
  static_assert(sizeof(float) == 4 && sizeof(uint32_t) == 4);
  check(bits(1000) == 0x447a0000); check(bits(123456) == 0x47f12000);
  unsigned texels = 0;
  const auto original = initial({7, 3, 12}), transfer = transferred(), legacy = initial({7, 3, 6});
  check(original.size() == 36 && legacy.size() == 18);
  check(original[0][24] == bits(1024)); check(original[35][0] == bits(2120));
  for (unsigned i = 0; i < original.size(); ++i) for (unsigned p = 0; p < original[i].size(); ++p) {
    const bool updated = i == 22 && (p == 0 || p == 1 || p == 3 || p == 4);
    const bool copied = i == 33 && (p == 17 || p == 18 || p == 24 || p == 25);
    if (updated) check(transfer[i][p] == bits(123456));
    else if (copied) {
      const unsigned y = p / 7 - 2, x = p % 7 - 3;
      check(transfer[i][p] == bits(1000 + (1 + y) * 7 + 1 + x));
    } else check(transfer[i][p] == original[i][p]);
  }
  for (const auto* resource : {&original, &transfer, &legacy})
    for (const auto& words : *resource) { texels += unsigned(words.size()); adversaries(words); }
  for (unsigned caseIndex = 0; caseIndex < 3; ++caseIndex) {
    const unsigned firstFace = caseIndex == 1 ? 0 : 6;
    const unsigned count = caseIndex == 0 ? UINT32_MAX : caseIndex == 1 ? 2 : 1;
    const auto result = generated(firstFace, count), input = initial({8, 4, 12}, true);
    for (unsigned face = 0; face < 12; ++face) for (unsigned mip = 0; mip < 4; ++mip) {
      const bool changed = face >= firstFace && face < firstFace + 6 && mip >= 2
        && (count == UINT32_MAX || mip < count + 1);
      for (auto value : result[face * 4 + mip])
        check(value == (changed ? bits(100 * face + 11) : input[face * 4 + mip][0]));
      texels += unsigned(result[face * 4 + mip].size()); adversaries(result[face * 4 + mip]);
    }
  }
  check(texels == 4830);
  const auto sample = sampled({7, 3, 12}, original, 6, 1, 1, 1);
  check(sample.size() == 6 && sample.front() == bits(1620) && sample.back() == bits(2120));
  adversaries(sample);
  std::printf("PASS cube probe arithmetic checks=%u bit_flips=%u texels=%u hardware=0\n", checks, flips, texels);
}
