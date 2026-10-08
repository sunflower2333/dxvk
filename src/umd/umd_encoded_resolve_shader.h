#pragma once
// SPDX-License-Identifier: MIT
#include "umd_shader.h"
#include <cstring>
#include <initializer_list>

namespace dxvk::umd {

constexpr bool encodedResolveSampleCount(uint32_t count) {
  return count >= 2 && count <= 32 && !(count & (count - 1));
}

// SM4.0 requires a declared MSAA count and literal ld_ms sample indices.
// The output is encoded RGB and ordinary UNORM alpha, ready for a UNORM RTV.
// Recover integer8 channel values before summation; this removes arithmetic
// roundoff at black/white endpoints and half-integer averages. Reconstruction
// of arbitrary sRGB bytes still depends on the renderer's decode accuracy.
inline bool encodedResolveTokens(uint32_t count, std::vector<uint32_t>& code,
    bool decodedSrgb = true) {
  code.clear();
  if (!encodedResolveSampleCount(count)) return false;
  const auto bits = [](float value) {
    uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result;
  };
  const auto emit = [&](std::initializer_list<uint32_t> words) {
    code.insert(code.end(), words.begin(), words.end());
  };
  emit({0x00000040, 0,
    0x04002058u | (count << 16), 0x00107000, 0, 0x00005555,
    0x04002064, 0x00101032, 0, 1, // linear_noperspective SV_Position.xy
    0x03000065, 0x001020f2, 0,
    0x02000068, 6,
    0x0500001c, 0x00100032, 0, 0x00101046, 0, // ftou pixel XY
    0x05000036, 0x001000f2, 1, 0x00004001, 0}); // integer-byte sum
  for (uint32_t sample = 0; sample < count; ++sample) {
    emit({0x0900002e, 0x001000f2, 2, 0x00100e46, 0,
      0x00107e46, 0, 0x00004001, sample});
    if (decodedSrgb) {
      // Avoid log(0). RGB is decoded by the source's original sRGB SRV;
      // alpha remains linear. Scratch registers3/4 carry high/low branches.
      emit({0x07000034, 0x00100072, 3, 0x00100e46, 2, 0x00004001, bits(1e-20f)});
      emit({0x0500002f, 0x00100072, 3, 0x00100e46, 3});
      emit({0x07000038, 0x00100072, 3, 0x00100e46, 3, 0x00004001, bits(1.f / 2.4f)});
      emit({0x05000019, 0x00100072, 3, 0x00100e46, 3});
      emit({0x09000032, 0x00100072, 3, 0x00100e46, 3,
        0x00004001, bits(1.055f), 0x00004001, bits(-.055f)});
      emit({0x07000038, 0x00100072, 4, 0x00100e46, 2, 0x00004001, bits(12.92f)});
      emit({0x0700001d, 0x00100072, 5, 0x00004001, bits(.0031308f), 0x00100e46, 2});
      emit({0x09000037, 0x00100072, 3, 0x00100e46, 5, 0x00100e46, 4, 0x00100e46, 3});
      // Explicit white endpoint: pow/mad approximation must not turn a
      // half-black/half-white encoded average into127 instead of128.
      emit({0x0700001d, 0x00100072, 5, 0x00100e46, 2, 0x00004001, bits(1.f)});
      emit({0x09000037, 0x00100072, 2, 0x00100e46, 5, 0x00004001, bits(1.f), 0x00100e46, 3});
    }
    emit({0x07000038, 0x001000f2, 2, 0x00100e46, 2, 0x00004001, bits(255.f)});
    emit({0x05000040, 0x001000f2, 2, 0x00100e46, 2});
    emit({0x07000000, 0x001000f2, 1, 0x00100e46, 1, 0x00100e46, 2});
  }
  // Counts are powers of two, so the byte-domain mean is represented
  // exactly. Round once there before normalizing for the output UNORM RTV.
  emit({0x07000038, 0x001000f2, 1, 0x00100e46, 1, 0x00004001, bits(1.f / float(count)),
    0x05000040, 0x001000f2, 1, 0x00100e46, 1,
    0x07000038, 0x001020f2, 0, 0x00100e46, 1, 0x00004001, bits(1.f / 255.f),
    0x0100003e});
  code[1] = uint32_t(code.size());
  return true;
}

inline bool encodedResolveShaderContainer(uint32_t count,
    std::vector<unsigned char>& container, bool decodedSrgb = true) {
  container.clear(); std::vector<uint32_t> code;
  if (!encodedResolveTokens(count, code, decodedSrgb)) return false;
  constexpr ShaderSignatureEntry position{1, 0, 3, ShaderScalar::Float32};
  constexpr ShaderSignatureEntry color{0, 0, 15, ShaderScalar::Float32};
  return buildShaderContainer(ShaderStage::Pixel, code.data(), code.size(),
    &position, 1, &color, 1, container);
}
}
