// SPDX-License-Identifier: MIT
// Actual production DDI with an explicit WARP-only factory, never native admission.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_texture1d.h"
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks = 0, texels = 0, cases = 0;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static ComPtr<ID3D11DeviceContext> createdContext;

// Keep exact failure sites and callback errors in release builds.
static void check(bool value, unsigned line, const char* expression) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL Texture1D\nline=%u\nexpression=%s\nerror=%08lx\n",
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
  ComPtr<ID3D11DeviceContext> context;
  // Enter through the real published DDI test entry; production admission stays closed.
  Fixture() {
    LUID luid{};
    D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = reportError;
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
  // Use non-power-of-two mip widths and physical padding distinct from logical size.
  Description(UINT width, UINT levels, UINT layers) : mips(levels) {
    for (UINT i = 0; i < levels; ++i) {
      UINT w = std::max(1u, width >> i);
      mips[i] = {w, 1, 1, (w + 15u) & ~15u, 1, 1};
    }
    args.pMipInfoList = mips.data();
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE1D;
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
      row.resize(std::max(1u, width >> mip));
      for (UINT x = 0; x < row.size(); ++x)
        row[x] = 0xff000000u | (layer << 16) | (mip << 8) | (x + 1);
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
        initial[i].SysMemPitch = static_cast<UINT>((*pixels)[i].size() * sizeof(UINT));
        initial[i].SysMemSlicePitch = initial[i].SysMemPitch;
      }
      args.pInitialDataUP = initial.data();
    }
    f.f.pfnCreateResource(f.device, &args, handle, {}); ok();
  }
  // Keep deferred child ownership inside the actual production destroy path.
  ~Resource() { owner.f.pfnDestroyResource(owner.device, handle); ok(); }
};
// Copy and map through DDIs and check every logical texel, never mapped padding.
static void readPixels(
    Fixture& f, Resource& resource, const D3D10DDIARG_CREATERESOURCE& original,
    const Pixels& expected) {
  auto args = original;
  args.Usage = D3D10_DDI_USAGE_STAGING; args.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  args.BindFlags = args.MiscFlags = 0; args.pInitialDataUP = nullptr;
  Resource staging(f, args);
  f.f.pfnResourceCopy(f.device, staging.handle, resource.handle); ok();
  for (UINT index = 0; index < expected.size(); ++index) {
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    f.f.pfnStagingResourceMap(f.device, staging.handle, index, D3D10_DDI_MAP_READ, 0, &mapped); ok();
    CHECK(mapped.pData);
    for (size_t x = 0; x < expected[index].size(); ++x) {
      UINT value;
      std::memcpy(&value, static_cast<const char*>(mapped.pData) + x * sizeof(UINT), sizeof(value));
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
  // Preserve explicit mip/slice ranges, including the all-remaining sentinel.
  ShaderView(Fixture& f, Resource& r, UINT mip, UINT levels, UINT layer, UINT layers)
      : owner(f), storage(f.f.pfnCalcPrivateShaderResourceViewSize(f.device, nullptr)),
        handle(storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>()) {
    D3D10DDIARG_CREATESHADERRESOURCEVIEW d{};
    d.hDrvResource = r.handle; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.ResourceDimension = D3D10DDIRESOURCE_TEXTURE1D;
    d.Tex1D.MostDetailedMip = mip; d.Tex1D.MipLevels = levels;
    d.Tex1D.FirstArraySlice = layer; d.Tex1D.ArraySize = layers;
    f.f.pfnCreateShaderResourceView(f.device, &d, handle, {}); ok();
  }
  // Inspect actual backend metadata through a separately queried API interface.
  ComPtr<ID3D11ShaderResourceView> inspect() {
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &handle); ok();
    ComPtr<ID3D11ShaderResourceView> view;
    owner.context->PSGetShaderResources(0, 1, &view);
    CHECK(view);
    D3D10DDI_HSHADERRESOURCEVIEW empty{};
    owner.f.pfnPsSetShaderResources(owner.device, 0, 1, &empty); ok();
    return view;
  }
  // Unbind before the real child-retirement DDI releases this view.
  ~ShaderView() { owner.f.pfnDestroyShaderResourceView(owner.device, handle); ok(); }
};
struct Target {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HRENDERTARGETVIEW handle;
  // Construct a real 1D or 2D view to exercise intrinsic dimensionality checks.
  Target(Fixture& f, Resource& r, UINT mip, UINT layer, UINT layers = 1,
      D3D10DDIRESOURCE_TYPE kind = D3D10DDIRESOURCE_TEXTURE1D)
      : owner(f), storage(f.f.pfnCalcPrivateRenderTargetViewSize(f.device, nullptr)),
        handle(storage.handle<D3D10DDI_HRENDERTARGETVIEW>()) {
    D3D10DDIARG_CREATERENDERTARGETVIEW d{};
    d.hDrvResource = r.handle; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.ResourceDimension = kind;
    if (kind == D3D10DDIRESOURCE_TEXTURE1D) {
      d.Tex1D.MipSlice = mip; d.Tex1D.FirstArraySlice = layer; d.Tex1D.ArraySize = layers;
    } else {
      d.Tex2D.MipSlice = mip; d.Tex2D.FirstArraySlice = layer; d.Tex2D.ArraySize = layers;
    }
    f.f.pfnCreateRenderTargetView(f.device, &d, handle, {}); ok();
  }
  // Keep the parent resource alive through view retirement.
  ~Target() { owner.f.pfnDestroyRenderTargetView(owner.device, handle); ok(); }
};

// Validate all mip/array initialization plus true Texture1D metadata and range conversion.
static void testInitialization(Fixture& f) {
  Description d(15, 4, 3); d.args.Usage = D3D10_DDI_USAGE_IMMUTABLE;
  d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
  auto expected = initialPixels(15, 4, 3);
  Resource resource(f, d.args, &expected);
  ShaderView srv(f, resource, 1, UINT32_MAX, 1, UINT32_MAX);
  auto view = srv.inspect();
  D3D11_SHADER_RESOURCE_VIEW_DESC range{}; view->GetDesc(&range);
  CHECK(range.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY);
  CHECK(range.Texture1DArray.MostDetailedMip == 1 && range.Texture1DArray.MipLevels == 3);
  CHECK(range.Texture1DArray.FirstArraySlice == 1 && range.Texture1DArray.ArraySize == 2);
  ComPtr<ID3D11Resource> backend; view->GetResource(&backend);
  D3D11_RESOURCE_DIMENSION kind{}; backend->GetType(&kind);
  CHECK(kind == D3D11_RESOURCE_DIMENSION_TEXTURE1D);
  ComPtr<ID3D11Texture1D> texture; CHECK(SUCCEEDED(backend.As(&texture)));
  D3D11_TEXTURE1D_DESC desc{}; texture->GetDesc(&desc);
  CHECK(desc.Width == 15 && desc.ArraySize == 3 && desc.MipLevels == 4);
  readPixels(f, resource, d.args, expected);
  ++cases;
}

// Check partial updates/copies, exact subresource indexing, and rejection atomicity.
static void testUpdates(Fixture& f) {
  Description d(15, 4, 3);
  auto expected = initialPixels(15, 4, 3);
  Resource src(f, d.args, &expected), dst(f, d.args, &expected);
  const UINT red[3] = {0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
  D3D10_DDI_BOX box{1, 0, 0, 4, 1, 1};
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 5, &box, red, 0, 0); ok();
  std::fill(expected[5].begin() + 1, expected[5].begin() + 4, red[0]);
  D3D10_DDI_BOX from{2, 0, 0, 5, 1, 1};
  f.f.pfnResourceCopyRegion(f.device, dst.handle, 8, 6, 0, 0, src.handle, 1, &from); ok();
  auto source = initialPixels(15, 4, 3);
  std::copy(source[1].begin() + 2, source[1].begin() + 5, expected[8].begin() + 6);
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 12, nullptr, red, 0, 0); rejected();
  D3D10_DDI_BOX outside{5, 0, 0, 8, 1, 1};
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 5, &outside, red, 0, 0); rejected();
  f.f.pfnResourceCopyRegion(f.device, dst.handle, 8, 14, 0, 0, src.handle, 1, &from); rejected();
  D3D10_DDI_BOX empty{2, 0, 0, 2, 1, 1};
  f.f.pfnResourceUpdateSubresourceUP(f.device, dst.handle, 5, &empty, nullptr, 0, 0); ok();
  readPixels(f, dst, d.args, expected);
  ++cases;
}

// Write-discard uses a real dynamic Texture1D and synchronized staging readback.
static void testDynamic(Fixture& f) {
  Description d(13, 1, 1); d.args.Usage = D3D10_DDI_USAGE_DYNAMIC;
  d.args.MapFlags = D3D10_DDI_CPU_ACCESS_WRITE; d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
  Resource resource(f, d.args);
  for (UINT round = 0; round < 3; ++round) {
    Pixels expected(1, std::vector<UINT>(13, 0xff000000u | round * 0x10101u));
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.f.pfnDynamicResourceMapDiscard(f.device, resource.handle, 0, D3D10_DDI_MAP_WRITE_DISCARD, 0, &map); ok();
    CHECK(map.pData);
    std::memcpy(map.pData, expected[0].data(), 13 * sizeof(UINT));
    f.f.pfnDynamicResourceUnmap(f.device, resource.handle, 0); ok();
    readPixels(f, resource, d.args, expected);
  }
  ++cases;
}

// Generate only the SRV-selected mips/slices; sentinel levels must remain untouched.
static void testMips(Fixture& f) {
  Description d(16, 5, 3); d.args.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
  auto expected = initialPixels(16, 5, 3);
  for (UINT slice = 1; slice < 3; ++slice)
    std::fill(expected[slice * 5 + 1].begin(), expected[slice * 5 + 1].end(),
      slice == 1 ? 0xff0000ffu : 0xff00ff00u);
  Resource resource(f, d.args, &expected);
  ShaderView srv(f, resource, 1, UINT32_MAX, 1, UINT32_MAX);
  f.f.pfnGenMips(f.device, srv.handle); ok();
  for (UINT slice = 1; slice < 3; ++slice)
    for (UINT mip = 2; mip < 5; ++mip)
      std::fill(expected[slice * 5 + mip].begin(), expected[slice * 5 + mip].end(),
        slice == 1 ? 0xff0000ffu : 0xff00ff00u);
  readPixels(f, resource, d.args, expected);
  ++cases;
}

// Failed view creation must not publish bytes and must allow retry without DestroyView.
static void testViewFailure(Fixture& f) {
  Description d(15, 4, 3); Resource resource(f, d.args);
  D3D10DDIARG_CREATESHADERRESOURCEVIEW args{};
  args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE1D;
  args.Tex1D.MipLevels = 4; args.Tex1D.ArraySize = 3;
  Storage storage(f.f.pfnCalcPrivateShaderResourceViewSize(f.device, &args));
  auto handle = storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>();
  for (UINT round = 0; round < 32; ++round) {
    storage.poison();
    args.Tex1D.FirstArraySlice = UINT32_MAX;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); rejected(); CHECK(storage.untouched());
    args.Tex1D.FirstArraySlice = 0; args.Format = DXGI_FORMAT_D32_FLOAT;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); rejected(); CHECK(storage.untouched());
    args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    f.f.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok();
    f.f.pfnDestroyShaderResourceView(f.device, handle); ok();
  }
  Description malformed(15, 4, 1); malformed.mips[2].TexelWidth = 4;
  Storage failed(f.f.pfnCalcPrivateResourceSize(f.device, &malformed.args));
  f.f.pfnCreateResource(f.device, &malformed.args, failed.handle<D3D10DDI_HRESOURCE>(), {}); rejected();
  malformed.mips[2].TexelWidth = 3;
  f.f.pfnCreateResource(f.device, &malformed.args, failed.handle<D3D10DDI_HRESOURCE>(), {}); ok();
  f.f.pfnDestroyResource(f.device, failed.handle<D3D10DDI_HRESOURCE>()); ok();
  ++cases;
}

// Check actual 1D RTV clear, disjoint slices, overlap and 1D/height-one-2D rejection.
static void testOutputs(Fixture& f) {
  Description d(15, 1, 2); auto expected = initialPixels(15, 1, 2);
  Resource resource(f, d.args, &expected);
  Target first(f, resource, 0, 0), second(f, resource, 0, 1), overlap(f, resource, 0, 0);
  D3D10DDI_HRENDERTARGETVIEW views[2] = {first.handle, second.handle};
  f.f.pfnSetRenderTargets(f.device, views, 2, 0, {}); ok();
  std::array<ComPtr<ID3D11RenderTargetView>, 2> original;
  ID3D11RenderTargetView* raw[2]{}; f.context->OMGetRenderTargets(2, raw, nullptr);
  original[0].Attach(raw[0]); original[1].Attach(raw[1]);
  CHECK(original[0] && original[1]);
  FLOAT red[4]{1,0,0,1}; f.f.pfnClearRenderTargetView(f.device, second.handle, red); ok();
  std::fill(expected[1].begin(), expected[1].end(), 0xff0000ffu);
  views[1] = overlap.handle;
  f.f.pfnSetRenderTargets(f.device, views, 2, 0, {}); rejected();
  Description two(15, 1, 1); two.args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  Resource twoResource(f, two.args);
  Target twoTarget(f, twoResource, 0, 0, 1, D3D10DDIRESOURCE_TEXTURE2D);
  views[1] = twoTarget.handle;
  f.f.pfnSetRenderTargets(f.device, views, 2, 0, {}); rejected();
  f.context->OMGetRenderTargets(2, raw, nullptr);
  for (UINT i = 0; i < 2; ++i) {
    CHECK(raw[i] == original[i].Get()); raw[i]->Release();
  }
  f.f.pfnSetRenderTargets(f.device, nullptr, 0, 0, {}); ok();
  readPixels(f, resource, d.args, expected);
  ++cases;
}

// Verify actual 1D DSV creation, clear and subresource readback with typeless backing.
static void testDepth(Fixture& f) {
  Description d(15, 2, 2); d.args.Format = DXGI_FORMAT_R32_TYPELESS;
  d.args.BindFlags = D3D10_DDI_BIND_DEPTH_STENCIL;
  Pixels expected(4);
  for (UINT i = 0; i < 4; ++i) expected[i].resize(i % 2 ? 7 : 15, 0u);
  Resource resource(f, d.args, &expected);
  D3D10DDIARG_CREATEDEPTHSTENCILVIEW args{};
  args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_D32_FLOAT;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE1D;
  args.Tex1D.MipSlice = 1; args.Tex1D.FirstArraySlice = 1; args.Tex1D.ArraySize = 1;
  Storage storage(f.f.pfnCalcPrivateDepthStencilViewSize(f.device, &args));
  auto view = storage.handle<D3D10DDI_HDEPTHSTENCILVIEW>();
  f.f.pfnCreateDepthStencilView(f.device, &args, view, {}); ok();
  f.f.pfnClearDepthStencilView(f.device, view, D3D10_DDI_CLEAR_DEPTH, 0.5f, 0); ok();
  f.f.pfnSetRenderTargets(f.device, nullptr, 0, 0, view); ok();
  ComPtr<ID3D11DepthStencilView> actual; f.context->OMGetRenderTargets(0, nullptr, &actual);
  CHECK(actual);
  D3D11_DEPTH_STENCIL_VIEW_DESC desc{}; actual->GetDesc(&desc);
  CHECK(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY);
  CHECK(desc.Texture1DArray.MipSlice == 1 && desc.Texture1DArray.FirstArraySlice == 1);
  f.f.pfnSetRenderTargets(f.device, nullptr, 0, 0, {}); ok();
  f.f.pfnDestroyDepthStencilView(f.device, view); ok(); actual.Reset();
  std::fill(expected[3].begin(), expected[3].end(), 0x3f000000u);
  readPixels(f, resource, d.args, expected);
  ++cases;
}

// Run the real DDI/worker/resource paths; WARP is reference evidence, never VIOGPU proof.
int main() {
  callerThread = GetCurrentThreadId();
  std::puts("BACKEND=WARP\nproduction_DDI=true\nVIOGPU=false\nregistration=false");
  {
    Fixture f;
    const struct { const char* name; void (*run)(Fixture&); } scenarios[] = {
      {"initialization", testInitialization}, {"updates", testUpdates},
      {"dynamic-discard", testDynamic}, {"scoped-mips", testMips},
      {"view-failure", testViewFailure}, {"outputs", testOutputs}, {"depth", testDepth}
    };
    for (const auto& scenario : scenarios) {
      std::printf("BEGIN Texture1D\ncase=%s\n", scenario.name); std::fflush(stdout);
      scenario.run(f);
      std::printf("PASS Texture1D case\ncase=%s\n", scenario.name); std::fflush(stdout);
    }
    f.f.pfnFlush(f.device); ok();
  }
  std::printf("PASS Texture1D\ncases=%u\nchecks=%u\ntexels=%u\n", cases, checks, texels);
  return 0;
}
