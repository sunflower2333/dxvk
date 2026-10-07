// SPDX-License-Identifier: MIT
#include "umd-d3d10-hardware-oracle.h"
#include <cstdio>
#include <cstdlib>

using namespace dxvk::umd::probe10;
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, \
  "DX10 hardware oracle failure line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)

int main() {
  Profile profile = Profile::D3D10_1;
  CHECK(parseProfile("10_0", profile) && profile == Profile::D3D10_0);
  CHECK(parseProfile(L"10_1", profile) && profile == Profile::D3D10_1);
  for (const char* value : {"", "10", "10_", "10_2", "11_0", "10_01", " 10_0", "10_0 "})
    CHECK(!parseProfile(value, profile));
  CHECK(!parseProfile(static_cast<const char*>(nullptr), profile));
  std::array<std::uint8_t, 8> luid{};
  CHECK(decodeHex("0123456789aBcDeF", luid));
  const std::array<std::uint8_t, 8> expected{{1, 35, 69, 103, 137, 171, 205, 239}};
  CHECK(luid == expected && decodeHex(L"0123456789abcdef", luid) && luid == expected);
  for (const char* value : {"", "0123456789abcde", "0123456789abcdef0", "g123456789abcdef", "+123456789abcdef"}) {
    CHECK(!decodeHex(value, luid)); CHECK(luid == expected);
  }
  CHECK(!decodeHex(static_cast<const char*>(nullptr), luid));
  std::array<std::uint8_t, 32> digest{};
  CHECK(decodeHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", digest));
  for (Scene scene : {Scene::Clear, Scene::VertexPixel, Scene::Gather, Scene::SampleIndex}) {
    const std::array<std::uint8_t, 4> value = scene == Scene::Clear
      ? std::array<std::uint8_t, 4>{0,255,0,255} : scene == Scene::VertexPixel
      ? std::array<std::uint8_t, 4>{255,0,0,255} : scene == Scene::Gather
      ? std::array<std::uint8_t, 4>{0,0,255,255} : std::array<std::uint8_t, 4>{127,255,0,255};
    Pixels image{};
    for (unsigned i = 0; i < PixelCount; ++i) std::memcpy(image.data() + i * 4, value.data(), 4);
    unsigned errors = 1;
    CHECK(readbackMatches(image.data(), image.size(), scene, &errors) && errors == 0);
    CHECK(!readbackMatches(nullptr, image.size(), scene, &errors) && errors == PixelCount);
    CHECK(!readbackMatches(image.data(), image.size() - 1, scene));
    CHECK(!readbackMatches(image.data(), image.size() + 1, scene));
    for (unsigned pixel = 0; pixel < PixelCount; ++pixel) {
      for (unsigned channel = 0; channel < 4; ++channel) {
        const unsigned index = pixel * 4 + channel;
        const auto saved = image[index];
        // A single wrong byte must identify exactly one wrong pixel.
        image[index] = channel == 0 && scene == Scene::SampleIndex ? 126 : std::uint8_t(saved ^ 1);
        CHECK(!readbackMatches(image.data(), image.size(), scene, &errors) && errors == 1);
        image[index] = saved;
      }
    }
    if (scene == Scene::SampleIndex) {
      for (unsigned pixel = 0; pixel < PixelCount; ++pixel) image[pixel * 4] = 128;
      CHECK(readbackMatches(image.data(), image.size(), scene));
      image[0] = 129; CHECK(!readbackMatches(image.data(), image.size(), scene));
    }
  }
  CHECK(!pixelMatches(nullptr, Scene::Clear));
  const std::uint8_t valid[]{0,255,0,255};
  CHECK(!pixelMatches(valid, static_cast<Scene>(99)));
  std::printf("DX10 hardware oracle verified checks=%u images=4 pixels=256 hardware_execution=0\n", checks);
}
