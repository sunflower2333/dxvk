// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace dxvk::umd::predicate10 {
constexpr unsigned Width = 16, Height = 16, PixelCount = Width * Height;
constexpr unsigned FrameCount = 36, BindingCount = 32, QueryCount = 3;
using Pixels = std::array<std::uint8_t, PixelCount * 4>;
using QueryWords = std::array<std::uint64_t, QueryCount * 4>;
// Frequency, then frame/generation/result/value/command/pre-read/start/end.
using BindingWords = std::array<std::uint64_t, 1 + BindingCount * 8>;
using OwnershipWords = std::array<std::uint64_t, 16>;

template<typename Char, std::size_t N>
bool decodeHex(const Char* text, std::array<std::uint8_t, N>& result) {
  if (!text || std::char_traits<Char>::length(text) != N * 2) return false;
  std::array<std::uint8_t, N> candidate{};
  for (std::size_t i = 0; i < N * 2; ++i) {
    const Char c = text[i]; unsigned digit;
    if (c >= Char('0') && c <= Char('9')) digit = unsigned(c - Char('0'));
    else if (c >= Char('a') && c <= Char('f')) digit = unsigned(c - Char('a')) + 10;
    else if (c >= Char('A') && c <= Char('F')) digit = unsigned(c - Char('A')) + 10;
    else return false;
    if (!(i & 1)) candidate[i / 2] = std::uint8_t(digit << 4);
    else candidate[i / 2] |= std::uint8_t(digit);
  }
  result = candidate; return true;
}

// Literal RGBA bytes; neither query data nor backend pixel output sets these.
inline std::array<std::uint8_t, 4> expectedPixel(unsigned frame) {
  constexpr std::array<std::uint8_t, 4> black{0,0,0,255}, red{255,0,0,255}, green{0,255,0,255};
  if (frame == 0) return black;
  if (frame == 1 || frame == 35) return red;
  if (frame >= 2 && frame < 26) {
    const unsigned generation = (frame - 2) / 8, value = ((frame - 2) % 8) / 4;
    return ((generation == 1) != (value != 0)) ? red : black;
  }
  if (frame >= 26 && frame < 30) return black;
  if (frame >= 30 && frame < 33) return red;
  if (frame == 33 || frame == 34) return green;
  return {};
}

inline bool readbackMatches(const void* data, std::size_t bytes, unsigned frame,
    unsigned* mismatches = nullptr) {
  unsigned errors = PixelCount;
  if (data && bytes == sizeof(Pixels) && frame < FrameCount) {
    errors = 0; const auto expected = expectedPixel(frame);
    const auto pixels = static_cast<const std::uint8_t*>(data);
    for (unsigned i = 0; i < PixelCount; ++i)
      errors += std::memcmp(pixels + i * 4, expected.data(), 4) != 0;
  }
  if (mismatches) *mismatches = errors;
  return !errors;
}

inline bool queryWordsMatch(const QueryWords& words) {
  for (unsigned i = 0; i < QueryCount; ++i) {
    const unsigned k = i * 4;
    if (words[k] != i || words[k + 1] != (i == 1 ? 1u : 0u)
        || words[k + 2] != 0 || words[k + 3] != 1) return false;
  }
  return true;
}

inline bool bindingWordsMatch(const BindingWords& words) {
  if (!words[0]) return false;
  for (unsigned i = 0; i < BindingCount; ++i) {
    const unsigned k = 1 + i * 8;
    const unsigned frame = i + 2;
    const unsigned generation = i < 24 ? i / 8 : 2;
    const unsigned value = i < 24 ? (i % 8) / 4 : (i - 24) / 4;
    const unsigned command = i < 24 ? i % 4 : 4 + (i - 24) % 4;
    if (words[k] != frame || words[k + 1] != generation
        || words[k + 2] != (generation == 1 ? 1u : 0u)
        || words[k + 3] != value || words[k + 4] != command
        || words[k + 5] != 0 || words[k + 7] < words[k + 6]) return false;
  }
  return true;
}

inline bool ownershipWordsMatch(const OwnershipWords& v) {
  // Query/context/allocation/lock/render/completion/residency counts are real.
  return v[0] > 0 && v[1] == 1 && v[2] == 1 && v[3] > 0 && v[3] == v[4]
    && v[5] > 0 && v[5] == v[6] && v[7] > 0 && v[8] >= 2
    && v[9] > 0 && v[9] == v[10] && !v[11] && !v[12] && !v[13] && !v[14] && v[15] == 1;
}
} // namespace dxvk::umd::predicate10
