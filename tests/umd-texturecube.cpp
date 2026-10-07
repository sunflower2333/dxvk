// SPDX-License-Identifier: MIT
// Actual production DDI with an explicit WARP-only factory, never native admission.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_view.h"
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks = 0, texels = 0, cases = 0;
static unsigned readbacks = 0;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static ComPtr<ID3D11DeviceContext> createdContext;

// Keep exact failure sites and callback errors in release builds.
static void check(bool value, unsigned line, const char* expression) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL TextureCube\nline=%u\nexpression=%s\nerror=%08lx\n",
      line, expression, static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x), __LINE__, #x)
// Success must never silently clear an earlier unexpected callback failure.
static void ok() { CHECK(lastError == S_OK); }
// Consume an error only at a deliberately invalid operation.
static void rejected() { CHECK(FAILED(lastError)); lastError = S_OK; }
// Verify that production workers route callbacks back to the original DDI caller.
static void APIENTRY reportError(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == callerThread);
  lastError = result;
}
// This factory is linked only into the standalone reference fixture.
HRESULT dxvk::umd::createDevice(
    const LUID&, D3D_FEATURE_LEVEL level, ID3D11Device** device,
    ID3D11DeviceContext** context, const dxvk::umd::RuntimeBackend*) noexcept {
  level = dxvk::umd::implementationFeatureLevel(level);
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) createdContext = *context;
  return hr;
}
// Readbacks below use real synchronized Map, not an invented busy result.
HRESULT dxvk::umd::isStagingResourceBusy(
    ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
// Preserve real command recording without asserting physical GPU execution.
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}

struct Storage {
  SIZE_T size;
  std::unique_ptr<void, decltype(&std::free)> bytes;
  // Model aligned runtime-owned storage, independently of implementation objects.
  explicit Storage(SIZE_T count) : size(count), bytes(std::calloc(1, count), &std::free) {
    CHECK(count && bytes);
  }
  // Build actual WDK handles, not replacement test structs.
  template<typename T> T handle() const { return {bytes.get()}; }
  // Detect premature publication on every byte of failed CreateView storage.
  void poison() { std::memset(bytes.get(), 0xcd, size); }
  // Check only raw bytes; failed views must not require a destructor.
  bool untouched() const {
    auto p = static_cast<const unsigned char*>(bytes.get());
    return std::all_of(p, p + size, [](unsigned char v) { return v == 0xcd; });
  }
};
struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device = storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};
  ComPtr<ID3D11DeviceContext> context;
  // Enter through the real published DDI test entry; production admission stays closed.
  Fixture() {
    LUID luid{};
    core.pfnSetErrorCb = reportError;
    CHECK(VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &core, &f) == S_OK);
    context = createdContext; createdContext.Reset();
    CHECK(context);
  }
  // All test resources have shorter lifetimes than this runtime callback owner.
  ~Fixture() { context->ClearState(); f.pfnDestroyDevice(device); ok(); }
};
struct Description {
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D10DDIARG_CREATERESOURCE args{};
  // Use square logical faces with physical padding distinct from logical size.
  Description(UINT width, UINT levels, UINT layers) : mips(levels) {
    for (UINT i = 0; i < levels; ++i) {
      UINT w = std::max(1u, width >> i);
      mips[i] = {w, w, 1, (w + 15u) & ~15u, (w + 15u) & ~15u, 1};
    }
    args.pMipInfoList = mips.data();
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    args.Usage = D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
    args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.SampleDesc.Count = 1; args.MipLevels = levels; args.ArraySize = layers;
  }
  Description(const Description&) = delete;
  Description& operator=(const Description&) = delete;
};
using Pixels = std::vector<std::vector<UINT>>;
// Independent subresource ordering and unique initial texels expose index aliasing.
static Pixels initialPixels(UINT width, UINT levels, UINT layers) {
  Pixels data(levels * layers);
  for (UINT layer = 0; layer < layers; ++layer)
    for (UINT mip = 0; mip < levels; ++mip) {
      auto& row = data[mip + layer * levels];
      const UINT edge = std::max(1u, width >> mip);
      row.resize(size_t(edge) * edge);
      for (UINT x = 0; x < row.size(); ++x)
        row[x] = 0xff000000u | (layer << 20) | (mip << 16) | (x + 1);
    }
  return data;
}
struct Resource {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  // Invoke production CreateResource with caller-owned initial-data arrays.
  Resource(Fixture& f, const D3D10DDIARG_CREATERESOURCE& desc, Pixels* pixels = nullptr)
      : owner(f), storage(f.f.pfnCalcPrivateResourceSize(f.device, &desc)),
        handle(storage.handle<D3D10DDI_HRESOURCE>()) {
    auto args = desc;
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial;
    if (pixels) {
      CHECK(pixels->size() == size_t(desc.MipLevels) * desc.ArraySize);
      initial.resize(pixels->size());
      for (size_t i = 0; i < initial.size(); ++i) {
        initial[i].pSysMem = (*pixels)[i].data();
        const UINT mip = static_cast<UINT>(i % desc.MipLevels);
        initial[i].SysMemPitch = std::max(1u, desc.pMipInfoList[0].TexelWidth >> mip) * sizeof(UINT);
        initial[i].SysMemSlicePitch = static_cast<UINT>((*pixels)[i].size() * sizeof(UINT));
      }
      args.pInitialDataUP = initial.data();
    }
    f.f.pfnCreateResource(f.device, &args, handle, {}); ok();
  }
  // Keep deferred child ownership inside the actual production destroy path.
  ~Resource() { owner.f.pfnDestroyResource(owner.device, handle); ok(); }
};
static void saveBytes(const std::string& path, const void* data, size_t bytes) {
  std::ofstream output(path, std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
  output.close(); CHECK(!output.fail());
}
// Copy into a staging 2D array and compare every face/mip texel through DDIs.
static void readPixels(
    Fixture& f, Resource& resource, const D3D10DDIARG_CREATERESOURCE& original,
    const Pixels& expected) {
  auto args = original;
  args.Usage = D3D10_DDI_USAGE_STAGING; args.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  args.BindFlags = args.MiscFlags = 0; args.pInitialDataUP = nullptr;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  Resource staging(f, args);
  f.f.pfnResourceCopy(f.device, staging.handle, resource.handle); ok();
  const UINT readback = readbacks++;
  for (UINT index = 0; index < expected.size(); ++index) {
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    f.f.pfnStagingResourceMap(f.device, staging.handle, index, D3D10_DDI_MAP_READ, 0, &mapped); ok();
    CHECK(mapped.pData);
    const UINT edge = original.pMipInfoList[index % original.MipLevels].TexelWidth;
    CHECK(mapped.RowPitch >= edge * sizeof(UINT));
    std::vector<UINT> observed(expected[index].size());
    for (size_t x = 0; x < observed.size(); ++x)
      std::memcpy(&observed[x], static_cast<const char*>(mapped.pData)
        + (x / edge) * mapped.RowPitch + (x % edge) * sizeof(UINT), sizeof(UINT));
    const std::string name = "cube-readback-" + std::to_string(readback)
      + "-face-" + std::to_string(index / original.MipLevels)
      + "-mip-" + std::to_string(index % original.MipLevels);
    const UINT metadata[] = {readback, index, edge, edge, mapped.RowPitch, mapped.DepthPitch};
    saveBytes(name + ".metadata", metadata, sizeof(metadata));
    saveBytes(name + ".words", observed.data(), observed.size() * sizeof(UINT));
    for (size_t x = 0; x < expected[index].size(); ++x) {
      const UINT value = observed[x];
      if (value != expected[index][x]) {
        std::fprintf(stderr, "READBACK mismatch\nsubresource=%u\nx=%zu\nactual=%08x\nexpected=%08x\n",
          index, x, value, expected[index][x]);
      }
      ++texels; CHECK(value == expected[index][x]);
    }
    f.f.pfnStagingResourceUnmap(f.device, staging.handle, index); ok();
  }
}
struct ShaderView {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HSHADERRESOURCEVIEW handle;
  ShaderView(Fixture& f, Resource& resource, UINT mip, UINT levels)
      : owner(f), storage(f.f.pfnCalcPrivateShaderResourceViewSize(f.device, nullptr)),
        handle(storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>()) {
    D3D10DDIARG_CREATESHADERRESOURCEVIEW d{};
    d.hDrvResource = resource.handle; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    d.TexCube.MostDetailedMip = mip; d.TexCube.MipLevels = levels;
    f.f.pfnCreateShaderResourceView(f.device, &d, handle, {}); ok();
  }
  ComPtr<ID3D11ShaderResourceView> inspect() {
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &handle); ok();
    ComPtr<ID3D11ShaderResourceView> view;
    owner.context->PSGetShaderResources(0, 1, &view); CHECK(view);
    D3D10DDI_HSHADERRESOURCEVIEW empty{};
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &empty); ok();
    return view;
  }
  ~ShaderView() { owner.f.pfnDestroyShaderResourceView(owner.device, handle); ok(); }
};

static void testInitialization(Fixture& f) {
  Description d(15, 4, 6); d.args.Usage = D3D10_DDI_USAGE_IMMUTABLE;
  d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
  auto expected = initialPixels(15, 4, 6);
  Resource resource(f, d.args, &expected);
  ShaderView srv(f, resource, 1, UINT32_MAX);
  auto view = srv.inspect();
  D3D11_SHADER_RESOURCE_VIEW_DESC range{}; view->GetDesc(&range);
  CHECK(range.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE);
  CHECK(range.TextureCube.MostDetailedMip == 1 && range.TextureCube.MipLevels == 3);
  ComPtr<ID3D11Resource> backend; view->GetResource(&backend);
  ComPtr<ID3D11Texture2D> texture; CHECK(SUCCEEDED(backend.As(&texture)));
  D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
  CHECK(desc.Width == 15 && desc.Height == 15 && desc.ArraySize == 6 && desc.MipLevels == 4);
  CHECK(desc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE);
  readPixels(f, resource, d.args, expected); ++cases;
}

static void testUpdates(Fixture& f) {
  Description d(15, 4, 6); auto expected = initialPixels(15, 4, 6);
  Resource src(f, d.args, &expected), dst(f, d.args, &expected);
  const UINT red[6] = {0xff0000ffu,0xff0000ffu,0xff0000ffu,
                       0xff0000ffu,0xff0000ffu,0xff0000ffu};
  D3D10_DDI_BOX box{1, 2, 0, 4, 4, 1};
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 5, &box, red, 12, 24); ok();
  for (UINT y = 2; y < 4; ++y)
    for (UINT x = 1; x < 4; ++x) expected[5][y * 7 + x] = red[0];
  D3D10_DDI_BOX from{2, 1, 0, 5, 3, 1};
  f.f.pfnResourceCopyRegion(f.device, dst.handle, 20, 6, 4, 0, src.handle, 1, &from); ok();
  auto original = initialPixels(15, 4, 6);
  for (UINT y = 0; y < 2; ++y)
    for (UINT x = 0; x < 3; ++x) expected[20][(y + 4) * 15 + x + 6] = original[1][(y + 1) * 7 + x + 2];
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 24, nullptr, red, 12, 24); rejected();
  D3D10_DDI_BOX outside{6, 0, 0, 8, 1, 1};
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 5, &outside, red, 12, 24); rejected();
  readPixels(f, dst, d.args, expected); ++cases;
}

static void testFaces(Fixture& f) {
  Description d(8, 2, 6); auto expected = initialPixels(8, 2, 6);
  Resource resource(f, d.args, &expected);
  for (UINT face = 0; face < 6; ++face) {
    D3D10DDIARG_CREATERENDERTARGETVIEW target{};
    target.hDrvResource = resource.handle; target.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    target.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    target.TexCube.MipSlice = 1; target.TexCube.FirstArraySlice = face; target.TexCube.ArraySize = 1;
    Storage storage(f.f.pfnCalcPrivateRenderTargetViewSize(f.device, &target));
    auto view = storage.handle<D3D10DDI_HRENDERTARGETVIEW>();
    f.f.pfnCreateRenderTargetView(f.device, &target, view, {}); ok();
    FLOAT color[4] = {FLOAT(face & 1), FLOAT((face >> 1) & 1), FLOAT((face >> 2) & 1), 1};
    f.f.pfnClearRenderTargetView(f.device, view, color); ok();
    const UINT packed = 0xff000000u | ((face & 1) ? 0xffu : 0u)
      | ((face & 2) ? 0xff00u : 0u) | ((face & 4) ? 0xff0000u : 0u);
    std::fill(expected[face * 2 + 1].begin(), expected[face * 2 + 1].end(), packed);
    f.f.pfnDestroyRenderTargetView(f.device, view); ok();
  }
  readPixels(f, resource, d.args, expected); ++cases;
}

static void testMips(Fixture& f) {
  for (UINT selected : {UINT32_MAX, 2u, 1u}) {
    Description d(16, 5, 6); d.args.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
    auto expected = initialPixels(16, 5, 6);
    for (UINT face = 0; face < 6; ++face)
      std::fill(expected[face * 5 + 1].begin(), expected[face * 5 + 1].end(), 0xff000000u | face * 0x20202u);
    Resource resource(f, d.args, &expected);
    ShaderView view(f, resource, 1, selected);
    f.f.pfnGenMips(f.device, view.handle); ok();
    const UINT count = selected == UINT32_MAX ? 4 : selected;
    for (UINT face = 0; face < 6; ++face)
      for (UINT mip = 2; mip < 1 + count; ++mip)
        std::fill(expected[face * 5 + mip].begin(), expected[face * 5 + mip].end(), 0xff000000u | face * 0x20202u);
    readPixels(f, resource, d.args, expected); ++cases;
  }
}

static void testFailures(Fixture& f) {
  Description d(15, 4, 6); Resource resource(f, d.args);
  D3D10DDIARG_CREATESHADERRESOURCEVIEW args{};
  args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE; args.TexCube.MipLevels = 4;
  Storage storage(f.f.pfnCalcPrivateShaderResourceViewSize(f.device, &args));
  auto handle = storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>();
  for (UINT round = 0; round < 16; ++round) {
    storage.poison(); args.TexCube.MostDetailedMip = UINT32_MAX;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); rejected(); CHECK(storage.untouched());
    args.TexCube.MostDetailedMip = 1; args.TexCube.MipLevels = 4;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); rejected(); CHECK(storage.untouched());
    args.TexCube.MipLevels = UINT32_MAX;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok();
    f.f.pfnDestroyShaderResourceView(f.device, handle); ok();
  }
  Description malformed(15, 4, 6);
  Storage failed(f.f.pfnCalcPrivateResourceSize(f.device, &malformed.args));
  auto resourceHandle = failed.handle<D3D10DDI_HRESOURCE>();
  for (UINT faces : {0u, 1u, 5u, 7u, 12u, UINT32_MAX}) {
    malformed.args.ArraySize = faces; failed.poison();
    f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); rejected(); CHECK(failed.untouched());
  }
  malformed.args.ArraySize = 6; malformed.mips[2].TexelHeight = 4; failed.poison();
  f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); rejected(); CHECK(failed.untouched());
  malformed.mips[2].TexelHeight = 3;
  for (UINT sampleCount : {0u, 2u, 4u}) {
    malformed.args.SampleDesc.Count = sampleCount; failed.poison();
    f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); rejected(); CHECK(failed.untouched());
  }
  malformed.args.SampleDesc.Count = 1; malformed.args.SampleDesc.Quality = 1; failed.poison();
  f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); rejected(); CHECK(failed.untouched());
  malformed.args.SampleDesc.Quality = 0; malformed.args.MipLevels = 5; failed.poison();
  f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); rejected(); CHECK(failed.untouched());
  malformed.args.MipLevels = 4;
  f.f.pfnCreateResource(f.device, &malformed.args, resourceHandle, {}); ok();
  f.f.pfnDestroyResource(f.device, resourceHandle); ok();
  D3D10DDIARG_CREATERENDERTARGETVIEW target{};
  target.hDrvResource = resource.handle; target.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  target.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
  Storage targetStorage(f.f.pfnCalcPrivateRenderTargetViewSize(f.device, &target));
  auto targetHandle = targetStorage.handle<D3D10DDI_HRENDERTARGETVIEW>();
  for (auto range : {std::array<UINT, 3>{4, 0, 1}, {0, 6, 1}, {0, 5, 2}, {0, 0, 0}, {0, UINT32_MAX, 1}}) {
    target.TexCube.MipSlice = range[0]; target.TexCube.FirstArraySlice = range[1]; target.TexCube.ArraySize = range[2];
    targetStorage.poison();
    f.f.pfnCreateRenderTargetView(f.device, &target, targetHandle, {}); rejected(); CHECK(targetStorage.untouched());
  }
  ++cases;
}

static void testDepth(Fixture& f) {
  Description d(7, 3, 6); d.args.Format = DXGI_FORMAT_R32_TYPELESS;
  d.args.BindFlags = D3D10_DDI_BIND_DEPTH_STENCIL;
  auto expected = initialPixels(7, 3, 6);
  for (auto& face : expected) std::fill(face.begin(), face.end(), 0u);
  Resource resource(f, d.args, &expected);
  D3D10DDIARG_CREATEDEPTHSTENCILVIEW args{};
  args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_D32_FLOAT;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
  args.TexCube.MipSlice = 1; args.TexCube.FirstArraySlice = 2; args.TexCube.ArraySize = 3;
  Storage storage(f.f.pfnCalcPrivateDepthStencilViewSize(f.device, &args));
  auto view = storage.handle<D3D10DDI_HDEPTHSTENCILVIEW>();
  f.f.pfnCreateDepthStencilView(f.device, &args, view, {}); ok();
  f.f.pfnClearDepthStencilView(f.device, view, D3D10_DDI_CLEAR_DEPTH, 0.5f, 0); ok();
  f.f.pfnDestroyDepthStencilView(f.device, view); ok();
  for (UINT face = 2; face < 5; ++face)
    std::fill(expected[face * 3 + 1].begin(), expected[face * 3 + 1].end(), 0x3f000000u);
  readPixels(f, resource, d.args, expected); ++cases;
}

int main() {
  callerThread = GetCurrentThreadId();
  std::puts("BACKEND=WARP\nproduction_DDI=true\nVIOGPU=false\nregistration=false");
  {
    Fixture f;
    const struct { const char* name; void (*run)(Fixture&); } scenarios[] = {
      {"initialization", testInitialization}, {"updates", testUpdates},
      {"faces", testFaces}, {"scoped-mips", testMips},
      {"failures", testFailures}, {"depth", testDepth}
    };
    for (const auto& scenario : scenarios) {
      std::printf("BEGIN TextureCube\ncase=%s\n", scenario.name); std::fflush(stdout);
      scenario.run(f);
      std::printf("PASS TextureCube case\ncase=%s\n", scenario.name); std::fflush(stdout);
    }
    f.f.pfnFlush(f.device); ok();
  }
  std::printf("PASS TextureCube\ncases=%u\nchecks=%u\ntexels=%u\nreadbacks=%u\n", cases, checks, texels, readbacks);
  return 0;
}
