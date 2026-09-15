#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "umd-probe-shaders.h"
#include <array>
#include <cstdlib>
#include <memory>

using Microsoft::WRL::ComPtr;
static unsigned checks;
static HRESULT lastError = S_OK;
static DWORD callerThread;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, \
  "predication check %u line %d: %s error=%08lx\n", checks, __LINE__, #c, \
  static_cast<unsigned long>(lastError)); std::exit(1); } } while (0)
static void APIENTRY error(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == callerThread);
  lastError = result;
}

#ifdef VIOGPU_PREDICATION_WARP
// Test-only independent rasterizer. The target probe links the actual DLL
// and never imports D3D11CreateDevice or substitutes the LUID/Turnip backend.
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context,
    const dxvk::umd::RuntimeBackend*) noexcept {
  level = dxvk::umd::implementationFeatureLevel(level);
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept {
  return E_NOTIMPL;
}
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}
#endif

struct Storage {
  std::unique_ptr<void, decltype(&std::free)> value;
  explicit Storage(SIZE_T size) : value(std::calloc(1, size), &std::free) { CHECK(size && value); }
  template<typename T> T handle() const { return {value.get()}; }
};

int main(int argc, char** argv) {
  callerThread = GetCurrentThreadId();
  LUID luid{};
#ifdef VIOGPU_PREDICATION_WARP
  CHECK(argc == 1);
  std::puts("PREDICATION_BACKEND WARP fixture: actual native DDI with independent rasterizer");
#else
  if (argc != 2 || std::strlen(argv[1]) != 16) return 2;
  for (unsigned i = 0; i < sizeof(luid); ++i) {
    char value[] = {argv[1][2*i], argv[1][2*i+1], 0}; char* end = nullptr;
    const auto byte = std::strtoul(value, &end, 16);
    if (end != value+2 || byte > 255) return 2;
    reinterpret_cast<unsigned char*>(&luid)[i] = static_cast<unsigned char>(byte);
  }
  std::puts("PREDICATION_BACKEND embedded DXVK/Turnip direct DDI harness; NOT Microsoft runtime activation");
#endif
  Storage deviceMemory(VioGpuDxvkPrivateDeviceSize());
  auto device = deviceMemory.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = error;
  D3D10DDI_DEVICEFUNCS f{};
  const HRESULT created = VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &core, &f);
  std::printf("PREDICATION_CREATE=%08lx\n", static_cast<unsigned long>(created));
  CHECK(created == S_OK && f.pfnSetPredication);
  auto ok = [&] { CHECK(lastError == S_OK); };
  D3D10DDI_MIPINFO mip = {16,16,1,16,16,1};
  D3D10DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
  desc.MipLevels = desc.ArraySize = 1;
  Storage targetMemory(f.pfnCalcPrivateResourceSize(device, &desc));
  auto target = targetMemory.handle<D3D10DDI_HRESOURCE>();
  f.pfnCreateResource(device, &desc, target, {}); ok();
  Storage copyMemory(f.pfnCalcPrivateResourceSize(device, &desc));
  auto copy = copyMemory.handle<D3D10DDI_HRESOURCE>();
  f.pfnCreateResource(device, &desc, copy, {}); ok();
  desc.Usage = D3D10_DDI_USAGE_STAGING; desc.BindFlags = 0; desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  Storage stagingMemory(f.pfnCalcPrivateResourceSize(device, &desc));
  auto staging = stagingMemory.handle<D3D10DDI_HRESOURCE>();
  f.pfnCreateResource(device, &desc, staging, {}); ok();
  D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc{};
  viewDesc.hDrvResource = target; viewDesc.Format = desc.Format;
  viewDesc.ResourceDimension = desc.ResourceDimension; viewDesc.Tex2D.ArraySize = 1;
  Storage viewMemory(f.pfnCalcPrivateRenderTargetViewSize(device, &viewDesc));
  auto view = viewMemory.handle<D3D10DDI_HRENDERTARGETVIEW>();
  f.pfnCreateRenderTargetView(device, &viewDesc, view, {}); ok();
  viewDesc.hDrvResource = copy;
  Storage copyViewMemory(f.pfnCalcPrivateRenderTargetViewSize(device, &viewDesc));
  auto copyView = copyViewMemory.handle<D3D10DDI_HRENDERTARGETVIEW>();
  f.pfnCreateRenderTargetView(device, &viewDesc, copyView, {}); ok();
  auto pixels = [&](D3D10DDI_HRESOURCE resource, UINT expected) {
    f.pfnResourceCopy(device, staging, resource); ok();
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.pfnStagingResourceMap(device, staging, 0, D3D10_DDI_MAP_READ, 0, &map); ok();
    CHECK(map.pData && map.RowPitch >= 64);
    for (unsigned y = 0; y < 16; ++y) {
      auto row = reinterpret_cast<const UINT*>(static_cast<const char*>(map.pData) + y*map.RowPitch);
      for (unsigned x = 0; x < 16; ++x) {
        if (row[x] != expected) std::fprintf(stderr,
          "PREDICATION_PIXEL x=%u y=%u expected=%08x actual=%08x\n",x,y,expected,row[x]);
        CHECK(row[x] == expected);
      }
    }
    f.pfnStagingResourceUnmap(device, staging, 0); ok();
  };
  constexpr char shader[] = R"(
float4 vs(uint id : SV_VertexID) : SV_Position {
  float2 xy = float2((id << 1) & 2, id & 2);
  return float4(xy * float2(2,-2) + float2(-1,1), 0, 1);
}
float4 ps() : SV_Target { return float4(1,0,0,1); }
)";
  std::vector<uint32_t> vs, ps;
  CHECK(compileHlslTokens(shader, "vs", "vs_4_0", vs));
  CHECK(compileHlslTokens(shader, "ps", "ps_4_0", ps));
  D3D10DDIARG_SIGNATURE_ENTRY vi = {D3D10_SB_NAME_VERTEX_ID,0,1};
  D3D10DDIARG_SIGNATURE_ENTRY vo = {D3D10_SB_NAME_POSITION,0,15};
  D3D10DDIARG_SIGNATURE_ENTRY po = {D3D10_SB_NAME_UNDEFINED,0,15};
  D3D10DDIARG_STAGE_IO_SIGNATURES vsSig = {&vi,1,&vo,1}, psSig = {nullptr,0,&po,1};
  Storage vsMemory(f.pfnCalcPrivateShaderSize(device, vs.data(), &vsSig));
  Storage psMemory(f.pfnCalcPrivateShaderSize(device, ps.data(), &psSig));
  auto vertex = vsMemory.handle<D3D10DDI_HSHADER>(), pixel = psMemory.handle<D3D10DDI_HSHADER>();
  f.pfnCreateVertexShader(device, vs.data(), vertex, {}, &vsSig); ok();
  f.pfnCreatePixelShader(device, ps.data(), pixel, {}, &psSig); ok();
  f.pfnVsSetShader(device, vertex); f.pfnPsSetShader(device, pixel);
  f.pfnSetRenderTargets(device, &view, 1, 0, {});
  D3D10_DDI_VIEWPORT viewport = {0,0,16,16,0,1};
  f.pfnSetViewports(device, 1, 0, &viewport);
  f.pfnIaSetTopology(device, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D10_DDI_RASTERIZER_DESC rasterDesc{};
  rasterDesc.FillMode = D3D10_DDI_FILL_SOLID; rasterDesc.CullMode = D3D10_DDI_CULL_NONE;
  rasterDesc.DepthClipEnable = TRUE;
  Storage rasterMemory(f.pfnCalcPrivateRasterizerStateSize(device, &rasterDesc));
  auto raster = rasterMemory.handle<D3D10DDI_HRASTERIZERSTATE>();
  f.pfnCreateRasterizerState(device, &rasterDesc, raster, {});
  f.pfnSetRasterizerState(device, raster); ok();
  UINT indices[] = {0,1,2};
  D3D10DDI_MIPINFO indexMip = {12,1,1,12,1,1};
  D3D10_DDIARG_SUBRESOURCE_UP indexData = {indices,12,12};
  D3D10DDIARG_CREATERESOURCE indexDesc{};
  indexDesc.pMipInfoList = &indexMip; indexDesc.pInitialDataUP = &indexData;
  indexDesc.ResourceDimension = D3D10DDIRESOURCE_BUFFER; indexDesc.Usage = D3D10_DDI_USAGE_DEFAULT;
  indexDesc.BindFlags = D3D10_DDI_BIND_INDEX_BUFFER; indexDesc.MipLevels = indexDesc.ArraySize = 1;
  indexDesc.SampleDesc.Count = 1;
  Storage indexMemory(f.pfnCalcPrivateResourceSize(device, &indexDesc));
  auto index = indexMemory.handle<D3D10DDI_HRESOURCE>();
  f.pfnCreateResource(device, &indexDesc, index, {});
  f.pfnIaSetIndexBuffer(device, index, DXGI_FORMAT_R32_UINT, 0); ok();

  D3D10DDIARG_CREATEQUERY predicateDesc = {D3D10DDI_QUERY_OCCLUSIONPREDICATE,0};
  Storage predicateMemory(f.pfnCalcPrivateQuerySize(device, &predicateDesc));
  auto predicate = predicateMemory.handle<D3D10DDI_HQUERY>();
  f.pfnCreateQuery(device, &predicateDesc, predicate, {}); ok();
  FLOAT black[] = {0,0,0,1}, red[] = {1,0,0,1};
  auto unbind = [&] { f.pfnSetPredication(device, {}, TRUE); ok(); };
  auto issue = [&](bool visible) {
    if (visible) { f.pfnQueryBegin(device, predicate); f.pfnDraw(device,3,0); }
    f.pfnQueryEnd(device, predicate); ok();
  };
  unsigned draws = 0;
  for (bool visible : {false,true,false}) {
    issue(visible); // Includes legal implicit begin and reuse after a true result.
    for (BOOL value : {FALSE,TRUE}) {
      for (unsigned mode = 0; mode < 4; ++mode) {
        f.pfnClearRenderTargetView(device, view, black); ok();
        // No QueryGetData before binding: pending predicate must still work.
        f.pfnSetPredication(device, predicate, value); ok();
        if (mode == 0) f.pfnDraw(device,3,0);
        if (mode == 1) f.pfnDrawIndexed(device,3,0,0);
        if (mode == 2) f.pfnDrawInstanced(device,3,1,0,0);
        if (mode == 3) f.pfnDrawIndexedInstanced(device,3,1,0,0,0);
        ok(); unbind(); pixels(target, visible != bool(value) ? 0xff0000ff : 0xff000000);
        ++draws;
      }
    }
    BOOL result = !visible;
    f.pfnQueryGetData(device, predicate, &result, sizeof(result), D3D10_DDI_GET_DATA_DO_NOT_FLUSH);
    ok(); CHECK(bool(result) == visible);
    std::printf("PREDICATION_DRAW_INTERVAL result=%d draw-cases=%u checks=%u\n",result,draws,checks);
  }
  // Empty (false) predicate suppresses value=false; all state/queries remain live.
  f.pfnClearRenderTargetView(device, view, black);
  ok(); pixels(target,0xff000000);
  f.pfnSetPredication(device, predicate, FALSE); ok();
  f.pfnQueryBegin(device, predicate); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  f.pfnQueryEnd(device, predicate); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  f.pfnDestroyQuery(device, predicate); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  std::puts("PREDICATION_CLEAR_AFTER_BOUND_QUERY_REJECTIONS");
  f.pfnClearRenderTargetView(device, view, red); ok(); unbind(); pixels(target,0xff000000);
  f.pfnClearRenderTargetView(device, copyView, red);
  f.pfnSetPredication(device, predicate, FALSE);
  f.pfnResourceCopy(device, target, copy); ok(); unbind(); pixels(target,0xff000000);
  f.pfnSetPredication(device, predicate, FALSE);
  f.pfnResourceCopyRegion(device,target,0,0,0,0,copy,0,nullptr); ok(); unbind(); pixels(target,0xff000000);
  std::array<UINT,256> green; green.fill(0xff00ff00);
  f.pfnSetPredication(device,predicate,FALSE);
  f.pfnResourceUpdateSubresourceUP(device,target,0,nullptr,green.data(),64,1024); ok();
  unbind(); pixels(target,0xff000000);
  // Re-enable resource updates and draw after unbinding, then destroy/recreate
  // the query in precisely the same private storage with no stale binding.
  f.pfnResourceUpdateSubresourceUP(device,target,0,nullptr,green.data(),64,1024); ok(); pixels(target,0xff00ff00);
  f.pfnDraw(device,3,0); ok(); pixels(target,0xff0000ff);
  f.pfnDestroyQuery(device,predicate); ok();
  predicateDesc.MiscFlags = D3D10DDI_QUERY_MISCFLAG_PREDICATEHINT;
  f.pfnCreateQuery(device,&predicateDesc,predicate,{}); ok();
  f.pfnQueryEnd(device,predicate); f.pfnSetPredication(device,predicate,FALSE); ok();
  f.pfnDraw(device,3,0); ok(); unbind();
  f.pfnQueryGetData(device,predicate,nullptr,0,0); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  f.pfnDestroyQuery(device,predicate); ok();
  // Invalid creation is reclaimed by the runtime without DestroyQuery.
  predicateDesc.MiscFlags = 2;
  f.pfnCreateQuery(device,&predicateDesc,predicate,{}); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  predicateDesc = {D3D10DDI_QUERY_OCCLUSION,0};
  f.pfnCreateQuery(device,&predicateDesc,predicate,{}); ok();
  f.pfnQueryEnd(device,predicate); ok();
  f.pfnSetPredication(device,predicate,FALSE); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  f.pfnDestroyQuery(device,predicate); ok();
  f.pfnSetRenderTargets(device,nullptr,0,1,{});
  f.pfnVsSetShader(device,{}); f.pfnPsSetShader(device,{}); f.pfnSetRasterizerState(device,{});
  f.pfnIaSetIndexBuffer(device,{},DXGI_FORMAT_R32_UINT,0);
  f.pfnDestroyShader(device,vertex); f.pfnDestroyShader(device,pixel);
  f.pfnDestroyRasterizerState(device,raster);
  f.pfnDestroyRenderTargetView(device,view); f.pfnDestroyRenderTargetView(device,copyView);
  f.pfnDestroyResource(device,index); f.pfnDestroyResource(device,staging);
  f.pfnDestroyResource(device,target); f.pfnDestroyResource(device,copy); ok();
  f.pfnFlush(device); ok(); f.pfnDestroyDevice(device); ok();
  std::printf("native predication PASS checks=%u draw-cases=%u pixels-per-case=256; synchronous fallback; native admission remains closed\n",checks,draws);
}
