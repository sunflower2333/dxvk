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
    std::fprintf(stderr, "FAIL CubeArrayMips\nline=%u\nexpression=%s\nerror=%08lx\n",
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
  D3D11DDI_DEVICEFUNCS f{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core{};
  ComPtr<ID3D11DeviceContext> context;
  // Enter through the real published DDI test entry; production admission stays closed.
  Fixture() {
    LUID luid{};
    core.pfnSetErrorCb = reportError;
    CHECK(VioGpuDxvkCreateDdiTestDevice11(&luid, device, {}, &core, &f, D3D_FEATURE_LEVEL_11_0) == S_OK);
    context = createdContext; createdContext.Reset();
    CHECK(context);
  }
  // All test resources have shorter lifetimes than this runtime callback owner.
  ~Fixture() { context->ClearState(); f.pfnDestroyDevice(device); ok(); }
};
struct Description {
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D11DDIARG_CREATERESOURCE args{};
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
        row[x] = 0xff000000u | (layer << 18) | (mip << 14) | (x + 1);
    }
  return data;
}
struct Resource {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  // Invoke production CreateResource with caller-owned initial-data arrays.
  Resource(Fixture& f, const D3D11DDIARG_CREATERESOURCE& desc, Pixels* pixels = nullptr)
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
    Fixture& f, Resource& resource, const D3D11DDIARG_CREATERESOURCE& original,
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
    const std::string name = "cube-array-readback-" + std::to_string(readback)
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
  ShaderView(Fixture& fixture, Resource& resource, UINT firstFace, UINT cubes, UINT mip, UINT levels)
      : owner(fixture), storage(fixture.f.pfnCalcPrivateShaderResourceViewSize(fixture.device, nullptr)),
        handle(storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>()) {
    D3D11DDIARG_CREATESHADERRESOURCEVIEW args{};
    args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    args.TexCube = {mip, levels, firstFace, cubes};
    owner.f.pfnCreateShaderResourceView(owner.device, &args, handle, {}); ok();
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &handle); ok();
    ComPtr<ID3D11ShaderResourceView> actual;
    owner.context->PSGetShaderResources(0, 1, &actual); CHECK(actual);
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{}; actual->GetDesc(&desc);
    CHECK(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY);
    CHECK(desc.TextureCubeArray.First2DArrayFace == firstFace && desc.TextureCubeArray.NumCubes == cubes);
    CHECK(desc.TextureCubeArray.MostDetailedMip == mip && desc.TextureCubeArray.MipLevels == levels);
    D3D10DDI_HSHADERRESOURCEVIEW empty{};
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &empty); ok();
  }
  ~ShaderView() { owner.f.pfnDestroyShaderResourceView(owner.device, handle); ok(); }
};

static UINT faceColor(UINT face) { return 0xff000000u | (face + 1) * 0x30303u; }
// All non-source subresources start with distinct face/mip/coordinate words.
// Uniform mip1 on each face gives exact integer mip results without filtering tolerance.
static Pixels initialCubeArray() {
  auto data = initialPixels(16, 5, 18);
  for (UINT face = 0; face < 18; ++face)
    std::fill(data[face * 5 + 1].begin(), data[face * 5 + 1].end(), faceColor(face));
  return data;
}
static void scopedMips(Fixture& fixture, UINT firstFace, UINT cubes, UINT levels) {
  Description description(16, 5, 18);
  description.args.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
  auto expected = initialCubeArray();
  Resource resource(fixture, description.args, &expected);
  ShaderView view(fixture, resource, firstFace, cubes, 1, levels);
  fixture.f.pfnGenMips(fixture.device, view.handle); ok();
  for (UINT face = firstFace; face < firstFace + cubes * 6; ++face)
    for (UINT mip = 2; mip < 1 + levels; ++mip)
      std::fill(expected[face * 5 + mip].begin(), expected[face * 5 + mip].end(), faceColor(face));
  readPixels(fixture, resource, description.args, expected);
  ++cases;
}
// A valid cube-array SRV on a resource without the automatic-mip bit is rejected.
// The complete actual readback proves that rejected generation changes no image.
static void missingAutomaticMips(Fixture& fixture) {
  Description description(16, 5, 18);
  auto expected = initialCubeArray();
  Resource resource(fixture, description.args, &expected);
  ShaderView view(fixture, resource, 6, 1, 1, 3);
  fixture.f.pfnGenMips(fixture.device, view.handle); rejected();
  readPixels(fixture, resource, description.args, expected);
  ++cases;
}
int main() {
  callerThread = GetCurrentThreadId();
  {
    Fixture fixture;
    scopedMips(fixture, 6, 1, 3);
    scopedMips(fixture, 0, 3, 4);
    scopedMips(fixture, 12, 1, 2);
    scopedMips(fixture, 6, 2, 1);
    missingAutomaticMips(fixture);
  }
  CHECK(cases == 5 && readbacks == 5 && texels == 30690);
  std::printf("PASS CubeArrayMips\ncases=%u\nchecks=%u\ntexels=%u\nreadbacks=%u\n", cases, checks, texels, readbacks);
}
