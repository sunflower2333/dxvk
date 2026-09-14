// SPDX-License-Identifier: MIT
// Actual native DDI + shader reconstruction; WARP is an explicit test-only backend.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "umd-probe-shaders.h"
#include <array>
#include <cstdlib>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
static unsigned checks, pixelChecks;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static ComPtr<ID3D11DeviceContext> createdContext;

// Preserve actionable failure locations and callback errors in every build mode.
static void check(bool value, unsigned line, const char* expression) {
  checks++;
  if (!value) {
    std::fprintf(stderr, "FAIL native MRT line=%u expression=%s error=%08lx\n",
      line, expression, static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x), __LINE__, #x)

// Require native callbacks to stay on the original DDI caller, not a worker.
static void APIENTRY reportError(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == callerThread);
  lastError = result;
}

// This replacement exists only in the standalone fixture, never in the UMD DLL.
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context,
    const dxvk::umd::RuntimeBackend*) noexcept {
  const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) createdContext = *context;
  return hr;
}
// This fixture never asks a staging-busy query to substitute for synchronized Map.
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept {
  return E_NOTIMPL;
}
// Flush the independent backend without inferring target-device completion.
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}

struct Storage {
  std::unique_ptr<void, decltype(&std::free)> value;
  // Model runtime-owned private storage; its address is never a kernel handle.
  explicit Storage(SIZE_T size) : value(std::calloc(1, size), &std::free) { CHECK(size && value); }
  // Use actual WDK handle types at the production entrypoints.
  template<typename T> T handle() const { return {value.get()}; }
};

struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device = storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f{};
  ComPtr<ID3D11DeviceContext> context;
  // Exercise the existing explicit harness; do not alter native admission.
  Fixture() {
    LUID luid{};
    D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = reportError;
    CHECK(VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &core, &f) == S_OK);
    context = createdContext; createdContext.Reset();
    CHECK(context && f.pfnSetRenderTargets && f.pfnCreatePixelShader);
  }
  // Release private objects before the fixture's runtime-owned memory is freed.
  ~Fixture() { f.pfnDestroyDevice(device); }
};

// A success check never clears an error that should have failed the case.
static void ok() { CHECK(lastError == S_OK); }
// Consume only an explicitly expected native rejection.
static void rejected() { CHECK(lastError == E_INVALIDARG); lastError = S_OK; }

struct Target {
  Fixture& fixture;
  std::unique_ptr<Storage> resourceStorage, viewStorage;
  D3D10DDI_HRESOURCE resource{};
  D3D10DDI_HRENDERTARGETVIEW view{};
  // Construct actual production texture/view owners through their published DDIs.
  Target(Fixture& owner, UINT width = 16, UINT height = 16, UINT layers = 1)
      : fixture(owner) {
    D3D10DDI_MIPINFO mip{width,height,1,width,height,1};
    D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.MipLevels = 1; desc.ArraySize = layers;
    resourceStorage = std::make_unique<Storage>(owner.f.pfnCalcPrivateResourceSize(owner.device, &desc));
    resource = resourceStorage->handle<D3D10DDI_HRESOURCE>();
    owner.f.pfnCreateResource(owner.device, &desc, resource, {}); ok();
    D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc{};
    viewDesc.hDrvResource = resource; viewDesc.Format = desc.Format;
    viewDesc.ResourceDimension = desc.ResourceDimension; viewDesc.Tex2D.ArraySize = layers;
    viewStorage = std::make_unique<Storage>(owner.f.pfnCalcPrivateRenderTargetViewSize(owner.device, &viewDesc));
    view = viewStorage->handle<D3D10DDI_HRENDERTARGETVIEW>();
    owner.f.pfnCreateRenderTargetView(owner.device, &viewDesc, view, {}); ok();
  }
  // Destroy views before resources; no object outlives its device/runtime callback table.
  ~Target() {
    fixture.f.pfnDestroyRenderTargetView(fixture.device, view);
    fixture.f.pfnDestroyResource(fixture.device, resource);
  }
};

using Bindings = std::array<ComPtr<ID3D11RenderTargetView>,8>;
// Inspect WARP state independently from the implementation's bookkeeping flag.
static Bindings bindings(Fixture& fixture) {
  ID3D11RenderTargetView* raw[8]{};
  fixture.context->OMGetRenderTargets(8, raw, nullptr);
  Bindings result;
  for (UINT i = 0; i < 8; i++) result[i].Attach(raw[i]);
  return result;
}
// A late-invalid input must not change any earlier color slot or clear the tail.
static void sameBindings(Fixture& fixture, const Bindings& expected) {
  auto current = bindings(fixture);
  for (UINT i = 0; i < 8; i++) CHECK(current[i].Get() == expected[i].Get());
}

// Build real SM4 color outputs with explicitly chosen semantic indices.
static std::string pixelSource(const std::vector<UINT>& slots, bool white) {
  std::string text = "struct Output {";
  for (UINT slot : slots)
    text += "float4 c" + std::to_string(slot) + ":SV_Target" + std::to_string(slot) + ";";
  text += "}; Output ps(){Output o;";
  for (UINT slot : slots) {
    const UINT r = white ? 1 : slot & 1, g = white ? 1 : (slot >> 1) & 1, b = white ? 1 : (slot >> 2) & 1;
    text += "o.c" + std::to_string(slot) + "=float4(" + std::to_string(r) + "," +
      std::to_string(g) + "," + std::to_string(b) + ",1);";
  }
  return text + "return o;}";
}

// Run production bindings, shaders, state and readback on a CPU renderer only.
int main() {
  callerThread = GetCurrentThreadId();
  std::puts("BACKEND=WARP; actual native DDI; no VIOGPU or system UMD registration");
  Fixture fixture;
  auto& f = fixture.f; auto device = fixture.device;
  std::array<std::unique_ptr<Target>,8> targets;
  std::array<D3D10DDI_HRENDERTARGETVIEW,8> views;
  for (UINT i = 0; i < 8; i++) {
    targets[i] = std::make_unique<Target>(fixture); views[i] = targets[i]->view;
  }
  constexpr char vsSource[] = R"(
float4 vs(uint id:SV_VertexID):SV_Position {
  float2 xy=float2((id<<1)&2,id&2);
  return float4(xy*float2(2,-2)+float2(-1,1),0,1);
})";
  std::vector<uint32_t> vs;
  CHECK(compileHlslTokens(vsSource,"vs","vs_4_0",vs));
  D3D10DDIARG_SIGNATURE_ENTRY vi{D3D10_SB_NAME_VERTEX_ID,0,1}, vo{D3D10_SB_NAME_POSITION,0,15};
  D3D10DDIARG_STAGE_IO_SIGNATURES vsSig{&vi,1,&vo,1};
  Storage vertexStorage(f.pfnCalcPrivateShaderSize(device,vs.data(),&vsSig));
  auto vertex = vertexStorage.handle<D3D10DDI_HSHADER>();
  f.pfnCreateVertexShader(device,vs.data(),vertex,{},&vsSig); ok();
  f.pfnVsSetShader(device,vertex);
  D3D10_DDI_VIEWPORT vp{0,0,16,16,0,1}; f.pfnSetViewports(device,1,0,&vp);
  f.pfnIaSetTopology(device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D10_DDI_RASTERIZER_DESC raster{};
  raster.FillMode = D3D10_DDI_FILL_SOLID; raster.CullMode = D3D10_DDI_CULL_NONE; raster.DepthClipEnable = TRUE;
  Storage rasterStorage(f.pfnCalcPrivateRasterizerStateSize(device,&raster));
  auto rasterHandle = rasterStorage.handle<D3D10DDI_HRASTERIZERSTATE>();
  f.pfnCreateRasterizerState(device,&raster,rasterHandle,{}); f.pfnSetRasterizerState(device,rasterHandle); ok();
  D3D10DDI_MIPINFO mip{16,16,1,16,16,1};
  D3D10DDIARG_CREATERESOURCE stagingDesc{};
  stagingDesc.pMipInfoList=&mip; stagingDesc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
  stagingDesc.Usage=D3D10_DDI_USAGE_STAGING; stagingDesc.MapFlags=D3D10_DDI_CPU_ACCESS_READ;
  stagingDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; stagingDesc.SampleDesc.Count=1;
  stagingDesc.MipLevels=stagingDesc.ArraySize=1;
  Storage stagingStorage(f.pfnCalcPrivateResourceSize(device,&stagingDesc));
  auto staging=stagingStorage.handle<D3D10DDI_HRESOURCE>();
  f.pfnCreateResource(device,&stagingDesc,staging,{}); ok();
  // Read all pixels, not just the first word or a count reported by the driver.
  auto pixels = [&](UINT slot, UINT expected) {
    f.pfnResourceCopy(device,staging,targets[slot]->resource); ok();
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.pfnStagingResourceMap(device,staging,0,D3D10_DDI_MAP_READ,0,&map); ok();
    CHECK(map.pData && map.RowPitch>=64);
    for (UINT y=0;y<16;y++) for (UINT x=0;x<16;x++) {
      UINT value; std::memcpy(&value,static_cast<char*>(map.pData)+y*map.RowPitch+x*4,4);
      pixelChecks++; CHECK(value==expected);
    }
    f.pfnStagingResourceUnmap(device,staging,0); ok();
  };
  // Reset every target before each independent workload, including unbound targets.
  auto clear = [&]() {
    FLOAT black[4]{0,0,0,1};
    for (auto& view:views) f.pfnClearRenderTargetView(device,view,black);
    ok();
  };
  Storage pixelStorage(f.pfnCalcPrivateShaderSize(device,nullptr,nullptr));
  auto pixel=pixelStorage.handle<D3D10DDI_HSHADER>(); bool hasPixel=false;
  // Reuse the same runtime private bytes only after destroying their previous shader.
  auto shader = [&](const std::vector<UINT>& slots, bool white) {
    if (hasPixel) { f.pfnPsSetShader(device,{}); f.pfnDestroyShader(device,pixel); ok(); }
    auto source=pixelSource(slots,white); std::vector<uint32_t> code;
    CHECK(compileHlslTokens(source.c_str(),"ps","ps_4_0",code));
    std::vector<D3D10DDIARG_SIGNATURE_ENTRY> outputs;
    for (UINT slot:slots) outputs.push_back({D3D10_SB_NAME_UNDEFINED,slot,15});
    D3D10DDIARG_STAGE_IO_SIGNATURES sig{nullptr,0,outputs.data(),UINT(outputs.size())};
    f.pfnCreatePixelShader(device,code.data(),pixel,{},&sig); ok();
    f.pfnPsSetShader(device,pixel); ok(); hasPixel=true;
  };
  shader({0,1,2,3,4,5,6,7},false);
  f.pfnSetRenderTargets(device,views.data(),8,0,{}); ok();
  auto all=bindings(fixture); for (const auto& v:all) CHECK(v);
  clear(); f.pfnDraw(device,3,0); ok();
  for (UINT i=0;i<8;i++) pixels(i,0xff000000u|((i&1)?255:0)|((i&2)?0xff00:0)|((i&4)?0xff0000:0));
  std::puts("PASS case=full-eight-target-draw");
  // Sparse signature order and sparse bindings must keep Target7 at slot seven.
  shader({7,0,3},false); auto sparse=views;
  for (UINT i=0;i<8;i++) if (i!=0 && i!=3 && i!=7) sparse[i]={};
  f.pfnSetRenderTargets(device,sparse.data(),8,0,{}); ok();
  clear(); f.pfnDraw(device,3,0); ok();
  for (UINT i=0;i<8;i++) pixels(i,i==7?0xffffffffu:i==3?0xff00ffffu:0xff000000u);
  std::puts("PASS case=sparse-signature-and-bindings");
  // Late failures preserve all preceding state instead of issuing a partial OM call.
  auto before=bindings(fixture);
  f.pfnSetRenderTargets(device,views.data(),9,0,{}); rejected(); sameBindings(fixture,before);
  f.pfnSetRenderTargets(device,views.data(),8,1,{}); rejected(); sameBindings(fixture,before);
  f.pfnSetRenderTargets(device,nullptr,1,0,{}); rejected(); sameBindings(fixture,before);
  {
    Target mismatch(fixture,8,16); auto invalid=views; invalid[7]=mismatch.view;
    f.pfnSetRenderTargets(device,invalid.data(),8,0,{}); rejected(); sameBindings(fixture,before);
    invalid=views; invalid[7]=views[0];
    f.pfnSetRenderTargets(device,invalid.data(),8,0,{}); rejected(); sameBindings(fixture,before);
    Fixture other; Target foreign(other); invalid=views; invalid[7]=foreign.view;
    f.pfnSetRenderTargets(device,invalid.data(),8,0,{}); rejected(); sameBindings(fixture,before);
  }
  std::puts("PASS case=late-rejection-preserves-bindings");
  // ClearSlots zero still removes the omitted tail and can unbind every target.
  f.pfnSetRenderTargets(device,views.data(),1,0,{}); ok();
  auto one=bindings(fixture); CHECK(one[0]); for (UINT i=1;i<8;i++) CHECK(!one[i]);
  f.pfnSetRenderTargets(device,nullptr,0,0,{}); ok();
  for (const auto& v:bindings(fixture)) CHECK(!v);
  std::puts("PASS case=shrink-and-unbind");
  // D3D10.0 has per-target enables/masks but shared blend factors and equations.
  shader({0,1,2,3,4,5,6,7},true);
  D3D10_DDI_BLEND_DESC blend{};
  blend.SrcBlend=blend.SrcBlendAlpha=D3D10_DDI_BLEND_ZERO;
  blend.DestBlend=blend.DestBlendAlpha=D3D10_DDI_BLEND_ONE;
  blend.BlendOp=blend.BlendOpAlpha=D3D10_DDI_BLEND_OP_ADD;
  for (UINT i=0;i<8;i++) { blend.BlendEnable[i]=i&1; blend.RenderTargetWriteMask[i]=UINT8(1u<<(i%4)); }
  Storage blendStorage(f.pfnCalcPrivateBlendStateSize(device,&blend));
  auto blendHandle=blendStorage.handle<D3D10DDI_HBLENDSTATE>();
  f.pfnCreateBlendState(device,&blend,blendHandle,{}); ok();
  FLOAT factors[4]{1,1,1,1}; f.pfnSetBlendState(device,blendHandle,factors,UINT(-1));
  f.pfnSetRenderTargets(device,views.data(),8,0,{}); ok();
  clear(); f.pfnDraw(device,3,0); ok();
  for (UINT i=0;i<8;i++) pixels(i,0xff000000u|((i&1)?0u:0xffu<<(8*(i%4))));
  std::puts("PASS case=per-target-blend-and-write-mask");
  f.pfnSetRenderTargets(device,nullptr,0,0,{});
  f.pfnSetBlendState(device,{},factors,UINT(-1)); f.pfnDestroyBlendState(device,blendHandle);
  f.pfnPsSetShader(device,{}); f.pfnVsSetShader(device,{});
  f.pfnDestroyShader(device,pixel); f.pfnDestroyShader(device,vertex);
  f.pfnSetRasterizerState(device,{}); f.pfnDestroyRasterizerState(device,rasterHandle);
  f.pfnDestroyResource(device,staging); ok();
  // Drop independent inspection references before production device teardown.
  all={}; before={}; one={};
  for (auto& target:targets) target.reset(); ok();
  std::printf("PASS native MRT: cases=5 checks=%u pixel-checks=%u; WARP, no VIOGPU\n",checks,pixelChecks);
}
