// SPDX-License-Identifier: MIT
// Interpret the emitted DXBC, independently compare integer channel means.
// This is CPU semantic coverage; it makes no native/GPU precision admission.
#include "../src/umd/umd_encoded_resolve_shader.h"
#include <dxbc/dxbc_container.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks, observations;
static void checkAt(bool value, unsigned line) {
  ++checks; if (!value) { std::fprintf(stderr, "encoded resolve failure checks=%u line=%u\n", checks, line); std::abort(); }
}
#define CHECK(value) checkAt(bool(value), __LINE__)
using Vector = std::array<uint32_t, 4>;
static uint32_t bits(float value) { uint32_t word; std::memcpy(&word, &value, 4); return word; }
static float number(uint32_t word) { float value; std::memcpy(&value, &word, 4); return value; }
static unsigned mean(unsigned sum, unsigned count) {
  const unsigned floor = sum / count, twice = (sum % count) * 2;
  return floor + (twice > count || (twice == count && (floor & 1)));
}
static float decode(unsigned value) {
  const double encoded = double(value) / 255;
  return float(encoded <= .04045 ? encoded / 12.92 : std::pow((encoded + .055) / 1.055, 2.4));
}
static Vector execute(const std::vector<uint32_t>& code, const std::vector<Vector>& samples) {
  std::array<Vector, 6> registers{}; Vector output{}, position{bits(.5f), bits(.5f), 0, bits(1.f)};
  CHECK(code.size() == code[1] && code[0] == 0x40);
  unsigned loads = 0, instructions = 0;
  for (size_t start = 2; start < code.size();) {
    const unsigned opcode = code[start] & 0x7ff, length = (code[start] >> 24) & 0x7f;
    CHECK(length && length <= code.size() - start); ++instructions;
    if (opcode == 88) {
      CHECK(length == 4 && ((code[start] >> 11) & 31) == 4);
      CHECK(((code[start] >> 16) & 127) == samples.size());
      CHECK(code[start + 1] == 0x00107000 && code[start + 2] == 0 && code[start + 3] == 0x5555);
    } else if (opcode == 100 || opcode == 101 || opcode == 104) {
      if (opcode == 104) CHECK(length == 2 && code[start + 1] == 6);
    } else if (opcode == 62) CHECK(length == 1 && start + length == code.size());
    else {
      size_t cursor = start + 1;
      const uint32_t destination = code[cursor++], index = code[cursor++];
      const unsigned type = (destination >> 12) & 255, mask = (destination >> 4) & 15;
      CHECK((type == 0 && index < registers.size()) || (type == 2 && index == 0));
      const auto source = [&]() {
        const uint32_t token = code[cursor++]; const unsigned kind = (token >> 12) & 255;
        if (kind == 4) { CHECK((token & 3) == 1); const uint32_t literal = code[cursor++]; return Vector{literal,literal,literal,literal}; }
        const uint32_t reg = code[cursor++]; Vector value{};
        if (kind == 0) { CHECK(reg < registers.size()); value = registers[reg]; }
        else if (kind == 1) { CHECK(reg == 0); value = position; }
        else CHECK(kind == 7 && reg == 0);
        Vector swizzled{};
        for (unsigned lane = 0; lane < 4; ++lane) swizzled[lane] = value[(token >> (4 + lane * 2)) & 3];
        return swizzled;
      };
      Vector value{}; const Vector a = source();
      if (opcode == 46) {
        const Vector resource = source(), sample = source(); (void)resource;
        CHECK(a[0] == 0 && a[1] == 0 && sample[0] == loads && loads < samples.size()); value = samples[loads++];
      } else if (opcode == 54) value = a;
      else if (opcode == 28) { for (unsigned lane = 0; lane < 4; ++lane) value[lane] = uint32_t(number(a[lane])); }
      else if (opcode == 47 || opcode == 25 || opcode == 64) {
        for (unsigned lane = 0; lane < 4; ++lane) {
          const float input = number(a[lane]);
          value[lane] = bits(opcode == 47 ? std::log2(input) : opcode == 25 ? std::exp2(input) : std::nearbyint(input));
        }
      } else {
        const Vector b = source(); Vector c{}; if (opcode == 50 || opcode == 55) c = source();
        for (unsigned lane = 0; lane < 4; ++lane) {
          const float first = number(a[lane]), second = number(b[lane]);
          if (opcode == 0) value[lane] = bits(first + second);
          else if (opcode == 56) value[lane] = bits(first * second);
          else if (opcode == 52) value[lane] = bits(std::fmax(first, second));
          else if (opcode == 29) value[lane] = first >= second ? 0xffffffff : 0;
          else if (opcode == 50) value[lane] = bits(first * second + number(c[lane]));
          else { CHECK(opcode == 55); value[lane] = a[lane] ? b[lane] : c[lane]; }
        }
      }
      CHECK(cursor == start + length);
      Vector& target = type == 2 ? output : registers[index];
      for (unsigned lane = 0; lane < 4; ++lane) if (mask & (1 << lane)) target[lane] = value[lane];
    }
    start += length;
  }
  CHECK(loads == samples.size() && instructions <= 512);
  return output;
}
int main() {
  for (bool decoded : {false,true}) for (uint32_t count : {2u,4u,8u,16u,32u}) {
    std::vector<unsigned char> bytes; CHECK(dxvk::umd::encodedResolveShaderContainer(count,bytes,decoded));
    dxbc_spv::dxbc::Container container(bytes.data(),bytes.size()); CHECK(container.validateHash());
    const auto chunk = container.getCodeChunk(); CHECK(chunk.getSize() > 8 && !(chunk.getSize() % 4));
    std::vector<uint32_t> code((chunk.getSize() - 8) / 4); std::memcpy(code.data(),chunk.getData(8),code.size() * 4);
    for (unsigned index = 0; index < 256; ++index) {
      std::vector<Vector> samples(count); std::array<unsigned,4> sums{};
      for (unsigned sample = 0; sample < count; ++sample) {
        const std::array<unsigned,4> channels{index,(index + sample * 47) & 255,
          (index * 29 + sample * 31) & 255,(index * 17 + sample * 43) & 255};
        for (unsigned lane = 0; lane < 4; ++lane) {
          sums[lane] += channels[lane]; samples[sample][lane] = bits(decoded && lane < 3 ? decode(channels[lane]) : float(channels[lane]) / 255);
        }
      }
      const auto actual = execute(code,samples);
      for (unsigned lane = 0; lane < 4; ++lane) {
        const float color = number(actual[lane]); CHECK(std::isfinite(color) && color >= 0 && color <= 1);
        CHECK(unsigned(std::nearbyint(color * 255)) == mean(sums[lane],count)); ++observations;
      }
    }
    // Independent endpoint/midpoint control, including alpha averaging.
    std::vector<Vector> halves(count);
    for (unsigned sample = 0; sample < count; ++sample) halves[sample] = sample & 1 ? Vector{bits(1),bits(1),bits(1),bits(1)} : Vector{};
    const auto actual = execute(code,halves);
    for (auto word : actual) CHECK(unsigned(std::nearbyint(number(word) * 255)) == 128);
  }
  for (uint32_t count : {0u,1u,3u,5u,31u,33u,64u,0xffffffffu}) {
    std::vector<uint32_t> code{1}; std::vector<unsigned char> bytes{1};
    CHECK(!dxvk::umd::encodedResolveTokens(count,code) && code.empty());
    CHECK(!dxvk::umd::encodedResolveShaderContainer(count,bytes) && bytes.empty());
  }
  CHECK(observations == 10240);
  std::printf("Encoded resolve shader CPU PASS variants=2 counts=5 channel_observations=10240 native_execution=0 hardware_admission=0\n");
}
