// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace dxvk::umd::probe10 {

enum class Profile { D3D10_0, D3D10_1 };
enum class Scene { Clear, VertexPixel, Gather, SampleIndex };
constexpr unsigned Width = 16, Height = 16, PixelCount = Width * Height;
using Pixels = std::array<std::uint8_t, PixelCount * 4>;

template<typename Char, std::size_t N>
bool decodeHex(const Char* text, std::array<std::uint8_t, N>& result) {
  if (!text || std::char_traits<Char>::length(text) != N * 2) return false;
  std::array<std::uint8_t, N> candidate{};
  for (std::size_t i = 0; i < N * 2; ++i) {
    const Char c = text[i];
    unsigned digit;
    if (c >= Char('0') && c <= Char('9')) digit = unsigned(c - Char('0'));
    else if (c >= Char('a') && c <= Char('f')) digit = unsigned(c - Char('a')) + 10;
    else if (c >= Char('A') && c <= Char('F')) digit = unsigned(c - Char('A')) + 10;
    else return false;
    if (!(i & 1)) candidate[i / 2] = std::uint8_t(digit << 4);
    else candidate[i / 2] |= std::uint8_t(digit);
  }
  result = candidate;
  return true;
}

template<typename Char>
bool parseProfile(const Char* text, Profile& result) {
  if (!text || std::char_traits<Char>::length(text) != 4
      || text[0] != Char('1') || text[1] != Char('0') || text[2] != Char('_')) return false;
  if (text[3] == Char('0')) result = Profile::D3D10_0;
  else if (text[3] == Char('1')) result = Profile::D3D10_1;
  else return false;
  return true;
}

inline const char* sceneName(Scene scene) {
  switch (scene) {
    case Scene::Clear: return "clear";
    case Scene::VertexPixel: return "vs-ps";
    case Scene::Gather: return "gather4";
    case Scene::SampleIndex: return "sample-index-4x";
  }
  return "invalid";
}

// R8G8B8A8_UNORM bytes, independently specified from shader execution.
// The four-sample resolve averages two zero and two one red samples.
// Both adjacent representable results around 0.5 are accepted, with exact
// green/blue/alpha and all 256 pixels checked in every image.
inline bool pixelMatches(const std::uint8_t* pixel, Scene scene) {
  if (!pixel) return false;
  switch (scene) {
    case Scene::Clear: return pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255;
    case Scene::VertexPixel: return pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255;
    case Scene::Gather: return pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255 && pixel[3] == 255;
    case Scene::SampleIndex: return pixel[0] >= 127 && pixel[0] <= 128
      && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255;
  }
  return false;
}

inline bool readbackMatches(const void* data, std::size_t bytes, Scene scene,
    unsigned* mismatches = nullptr) {
  unsigned errors = PixelCount;
  if (data && bytes == sizeof(Pixels)) {
    errors = 0;
    const auto pixels = static_cast<const std::uint8_t*>(data);
    for (unsigned i = 0; i < PixelCount; ++i) errors += !pixelMatches(pixels + i * 4, scene);
  }
  if (mismatches) *mismatches = errors;
  return !errors;
}

} // namespace dxvk::umd::probe10
