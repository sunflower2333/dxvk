// Internal backend acceptance probe. This deliberately does not call the
// system D3D runtime; runtime-to-DDI acceptance is a separate pending gate.
#include "../src/umd/umd_backend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dxvk { Logger Logger::s_instance("dxvk-umd-backend-probe.log"); }

static bool parseLuid(const char* text, dxvk::umd::AdapterLuid& luid) {
  if (!text || std::strlen(text) != 16)
    return false;
  for (size_t i = 0; i < luid.size(); i++) {
    char byte[3] = {text[2*i], text[2*i+1], 0};
    char* end = nullptr;
    const auto value = std::strtoul(byte, &end, 16);
    if (end != byte + 2 || value > 255)
      return false;
    luid[i] = uint8_t(value);
  }
  return luid != dxvk::umd::AdapterLuid{};
}

int main(int argc, char** argv) {
  using namespace dxvk;
  umd::AdapterLuid luid = {};
  if (argc != 2 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-backend-probe <16 hex LUID bytes>\n");
    return 2;
  }
  std::unique_ptr<umd::Backend> backend;
  HRESULT hr = umd::Backend::create(luid, D3D_FEATURE_LEVEL_10_0, backend);
  if (FAILED(hr)) {
    std::printf("BACKEND_CREATE_FAIL hr=%08lx\n", static_cast<unsigned long>(hr));
    return 3;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = 64; desc.Height = 64;
  desc.MipLevels = 1; desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  Com<ID3D11Texture2D> target;
  Com<ID3D11RenderTargetView> rtv;
  Com<ID3D11Texture2D> staging;
  if (FAILED(backend->d3d->CreateTexture2D(&desc, nullptr, &target)) ||
      FAILED(backend->d3d->CreateRenderTargetView(target.ptr(), nullptr, &rtv)))
    return 4;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  if (FAILED(backend->d3d->CreateTexture2D(&desc, nullptr, &staging)))
    return 5;

  const float color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
  backend->context->ClearRenderTargetView(rtv.ptr(), color);
  backend->context->CopyResource(staging.ptr(), target.ptr());
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  hr = backend->context->Map(staging.ptr(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    std::printf("MAP_FAIL hr=%08lx\n", static_cast<unsigned long>(hr));
    return 6;
  }
  unsigned mismatches = 0;
  const uint8_t expected[4] = {255, 0, 255, 255};
  for (unsigned y = 0; y < desc.Height; y++)
    for (unsigned x = 0; x < desc.Width; x++) {
      const auto pixel = static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch + 4*x;
      mismatches += std::memcmp(pixel, expected, sizeof(expected)) != 0;
    }
  backend->context->Unmap(staging.ptr(), 0);
  std::printf("BACKEND_PIXELS %s pixels=4096 mismatches=%u\n",
              mismatches ? "FAIL" : "PASS", mismatches);
  return mismatches ? 7 : 0;
}
