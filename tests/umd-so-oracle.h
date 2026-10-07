// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dxvk::umd::probe {

// Independent scalar oracle for the original four-stream HLSL in the KMT
// probe. Skipped components and the unused tail retain their initial bits.
constexpr uint32_t SoPoison = 0xa5a55a5au;
constexpr unsigned SoStreams = 4;
constexpr unsigned SoWords = 64;
constexpr std::array<unsigned, SoStreams> SoStrideWords = {8, 4, 6, 4};
using SoCapture = std::array<uint32_t, SoWords>;

constexpr SoCapture expectedSoCapture(unsigned stream, unsigned firstVertex = 0) {
  SoCapture result{};
  for (auto& word : result) word = SoPoison;
  if (stream >= SoStreams) return result;
  for (unsigned vertex = 0; vertex < 3; ++vertex) {
    for (unsigned emitted = 0; emitted <= stream; ++emitted) {
      const unsigned offset = (vertex * (stream + 1) + emitted) * SoStrideWords[stream];
      result[offset] = firstVertex + vertex + 100 * stream;
      result[offset + 1] = 900 + stream + emitted;
    }
  }
  return result;
}

constexpr bool soCaptureMatches(const SoCapture& actual, unsigned stream, unsigned firstVertex = 0) {
  if (stream >= SoStreams) return false;
  const auto expected = expectedSoCapture(stream, firstVertex);
  for (size_t i = 0; i < actual.size(); ++i)
    if (actual[i] != expected[i]) return false;
  return true;
}

// A signature-only NULL GS captures the original VS output as raw bits.
// These payloads deliberately include both signed zero and NaN encodings;
// interpreting or numerically comparing them would weaken the oracle.
constexpr SoCapture expectedNullSoCapture(bool alternate) {
  SoCapture result{};
  for (auto& word : result) word = SoPoison;
  for (unsigned vertex = 0; vertex < 3; ++vertex) {
    const unsigned i = vertex * 8;
    result[i] = result[i + 1] = result[i + 2] = 0;
    result[i + 3] = 0x3f800000u;
    result[i + 4] = alternate ? 0xffc04321u : 0x7fc01234u;
    result[i + 5] = alternate ? 0xffffffffu : 0x80000000u;
    result[i + 6] = alternate ? 0x11223344u : 0x87654321u;
    result[i + 7] = vertex + (alternate ? 100u : 0u);
  }
  return result;
}

constexpr bool nullSoCaptureMatches(const SoCapture& actual, bool alternate) {
  const auto expected = expectedNullSoCapture(alternate);
  for (size_t i = 0; i < actual.size(); ++i)
    if (actual[i] != expected[i]) return false;
  return true;
}

} // namespace dxvk::umd::probe
