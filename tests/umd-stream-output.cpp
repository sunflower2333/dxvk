#include "../src/umd/umd_api.h"
#include "../src/umd/umd_stream_output.h"
#include "umd-probe-shaders.h"
#include <array>
#include <cstdlib>
#include <memory>

using Microsoft::WRL::ComPtr;
static unsigned checks;
static HRESULT lastError = S_OK;
static DWORD callerThread;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, \
  "stream-output check %u line %d: %s error=%08lx\n", checks, __LINE__, #c, \
  static_cast<unsigned long>(lastError)); std::exit(1); } } while (0)
static void APIENTRY error(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == callerThread); lastError = result;
}
#ifdef VIOGPU_STREAM_OUTPUT_WARP
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context,
    const dxvk::umd::RuntimeBackend*) noexcept {
  return D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    &level,1,D3D11_SDK_VERSION,device,nullptr,context);
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
  explicit Storage(SIZE_T size) : value(std::calloc(1,size),&std::free) { CHECK(size && value); }
  template<typename T> T handle() const { return {value.get()}; }
};

int main(int argc, char** argv) {
  callerThread = GetCurrentThreadId();
  LUID luid{};
#ifdef VIOGPU_STREAM_OUTPUT_WARP
  CHECK(argc == 1);
  std::puts("STREAM_OUTPUT_BACKEND WARP fixture; actual native DDI, independent rasterizer");
#else
  if (argc != 2 || std::strlen(argv[1]) != 16) return 2;
  for (unsigned i = 0; i < sizeof(luid); ++i) {
    char value[] = {argv[1][2*i],argv[1][2*i+1],0}; char* end = nullptr;
    const auto byte = std::strtoul(value,&end,16);
    if (end != value+2 || byte > 255) return 2;
    reinterpret_cast<unsigned char*>(&luid)[i] = static_cast<unsigned char>(byte);
  }
  std::puts("STREAM_OUTPUT_BACKEND embedded DXVK/Turnip direct DDI; NOT Microsoft runtime activation");
#endif
  Storage deviceMemory(VioGpuDxvkPrivateDeviceSize());
  auto device = deviceMemory.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = error;
  D3D10DDI_DEVICEFUNCS f{};
  CHECK(VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core,&f) == S_OK);
  CHECK(f.pfnCreateGeometryShaderWithStreamOutput && f.pfnSoSetTargets && f.pfnDrawAuto);
  auto ok = [&] { CHECK(lastError == S_OK); };
  auto invalid = [&] { CHECK(lastError == E_INVALIDARG); lastError = S_OK; };
  std::vector<Storage> resources, shaders, queries;
  auto buffer = [&](UINT bytes, bool staging = false) {
    D3D10DDI_MIPINFO mip = {bytes,1,1,bytes,1,1};
    std::vector<UINT> initial(bytes/4,0xcccccccc);
    D3D10_DDIARG_SUBRESOURCE_UP data = {initial.data(),bytes,bytes};
    D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList = &mip; desc.pInitialDataUP = &data;
    desc.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
    desc.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    desc.BindFlags = staging ? 0 : D3D10_DDI_BIND_STREAM_OUTPUT | D3D10_DDI_BIND_VERTEX_BUFFER;
    desc.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    desc.SampleDesc.Count = desc.MipLevels = desc.ArraySize = 1;
    resources.emplace_back(f.pfnCalcPrivateResourceSize(device,&desc));
    auto result = resources.back().handle<D3D10DDI_HRESOURCE>();
    f.pfnCreateResource(device,&desc,result,{}); ok(); return result;
  };
  auto output = buffer(256), readback = buffer(256,true), extra = buffer(256);
  auto words = [&](D3D10DDI_HRESOURCE input) {
    f.pfnResourceCopy(device,readback,input); ok();
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.pfnStagingResourceMap(device,readback,0,D3D10_DDI_MAP_READ,0,&map); ok();
    CHECK(map.pData);
    std::array<UINT,64> result; std::memcpy(result.data(),map.pData,sizeof(result));
    f.pfnStagingResourceUnmap(device,readback,0); ok(); return result;
  };
  constexpr char source[] = R"(
float4 vs(uint id : SV_VertexID) : SV_Position {
  float2 xy = float2((id << 1) & 2, id & 2);
  return float4(xy * float2(2,-2) + float2(-1,1),0,1);
}
struct Output { float4 position : SV_Position; uint4 payload : TEXCOORD0; };
[maxvertexcount(3)]
void gs(triangle float4 positions[3] : SV_Position, inout TriangleStream<Output> stream) {
  [unroll] for (uint i = 0; i < 3; ++i) {
    Output value; value.position = positions[i];
    value.payload = uint4(0x11223344,0x87654321,0x7fc01234,0x80000000);
    stream.Append(value);
  }
  stream.RestartStrip();
}
float4 replay(float4 position : POSITION) : SV_Position { return position; }
float4 ps() : SV_Target { return float4(1,0,0,1); }
)";
  auto shader = [&](const char* entry, const char* profile, bool streamOutput,
      const D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY* declaration = nullptr, UINT count = 0, UINT stride = 0) {
    std::vector<UINT> tokens;
    ComPtr<ID3DBlob> original;
    CHECK(compileHlslTokens(source,entry,profile,tokens,&original));
    ComPtr<ID3D11ShaderReflection> reflection;
    CHECK(SUCCEEDED(D3DReflect(original->GetBufferPointer(),original->GetBufferSize(),
      __uuidof(ID3D11ShaderReflection),&reflection)));
    D3D11_SHADER_DESC desc{}; CHECK(SUCCEEDED(reflection->GetDesc(&desc)));
    std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs, outputs;
    for (bool input : {true,false}) {
      for (UINT i = 0; i < (input ? desc.InputParameters : desc.OutputParameters); ++i) {
        D3D11_SIGNATURE_PARAMETER_DESC value{};
        CHECK(SUCCEEDED(input ? reflection->GetInputParameterDesc(i,&value) : reflection->GetOutputParameterDesc(i,&value)));
        (input ? inputs : outputs).push_back({
          D3D10_SB_NAME(!input && profile[0] == 'p' ? 0 : value.SystemValueType),value.Register,value.Mask});
      }
    }
    D3D10DDIARG_STAGE_IO_SIGNATURES sig = {inputs.data(),UINT(inputs.size()),outputs.data(),UINT(outputs.size())};
    if (streamOutput) {
      CHECK(outputs.size() == 2 && outputs[0].Register == 0 && outputs[1].Register == 1);
      D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT so = {tokens.data(),declaration,count,stride};
      shaders.emplace_back(f.pfnCalcPrivateGeometryShaderWithStreamOutput(device,&so,&sig));
      auto result = shaders.back().handle<D3D10DDI_HSHADER>();
      f.pfnCreateGeometryShaderWithStreamOutput(device,&so,result,{},&sig); ok(); return result;
    }
    shaders.emplace_back(f.pfnCalcPrivateShaderSize(device,tokens.data(),&sig));
    auto result = shaders.back().handle<D3D10DDI_HSHADER>();
    if (profile[0] == 'v') f.pfnCreateVertexShader(device,tokens.data(),result,{},&sig);
    else f.pfnCreatePixelShader(device,tokens.data(),result,{},&sig);
    ok(); return result;
  };
  D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY packed[] = {{0,0,15},{0,1,15}};
  D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY split[] = {{0,0,15},{2,1,15}};
  D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY sparse[] = {{0,0,15},
    {0,D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP,3},{0,1,5}};
  auto vertex = shader("vs","vs_4_0",false);
  auto geometry = shader("gs","gs_4_0",true,packed,2,32);
  auto splitGeometry = shader("gs","gs_4_0",true,split,2,0);
  auto sparseGeometry = shader("gs","gs_4_0",true,sparse,3,32);
  auto replay = shader("replay","vs_4_0",false), pixel = shader("ps","ps_4_0",false);
  auto query = [&](D3D10DDI_QUERY type) {
    D3D10DDIARG_CREATEQUERY desc = {type,0};
    queries.emplace_back(f.pfnCalcPrivateQuerySize(device,&desc));
    auto result = queries.back().handle<D3D10DDI_HQUERY>();
    f.pfnCreateQuery(device,&desc,result,{}); ok(); return result;
  };
  auto statistics = query(D3D10DDI_QUERY_STREAMOUTPUTSTATS);
  auto overflow = query(D3D10DDI_QUERY_STREAMOVERFLOWPREDICATE);
  auto pipeline = query(D3D10DDI_QUERY_PIPELINESTATS);
  auto readQuery = [&](D3D10DDI_HQUERY object, void* result, UINT size) {
    const auto deadline = GetTickCount64()+2000;
    do {
      lastError = S_OK; f.pfnQueryGetData(device,object,result,size,0);
      if (lastError != DXGI_DDI_ERR_WASSTILLDRAWING) { ok(); return; }
      CHECK(GetTickCount64() < deadline); Sleep(1);
    } while (true);
  };
  auto bind = [&](D3D10DDI_HSHADER gs, UINT offset) {
    f.pfnGsSetShader(device,gs); f.pfnSoSetTargets(device,1,0,&output,&offset); ok();
  };
  auto unbind = [&] { f.pfnSoSetTargets(device,0,0,nullptr,nullptr); ok(); };
  f.pfnVsSetShader(device,vertex); f.pfnIaSetTopology(device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  // No PS, render targets or viewport: SO must execute independently of rasterization.
  bind(geometry,0);
  f.pfnQueryBegin(device,statistics); f.pfnQueryBegin(device,overflow); f.pfnQueryBegin(device,pipeline);
  f.pfnDraw(device,3,0); ok();
  f.pfnQueryEnd(device,pipeline); f.pfnQueryEnd(device,overflow); f.pfnQueryEnd(device,statistics); ok(); unbind();
  const auto first = words(output);
  const float positions[] = {-1,1,0,1,3,1,0,1,-1,-3,0,1};
  const UINT payload[] = {0x11223344,0x87654321,0x7fc01234,0x80000000};
  auto triangle = [&](const std::array<UINT,64>& data, UINT start) {
    for (UINT v = 0; v < 3; ++v) for (UINT c = 0; c < 4; ++c) {
      UINT expected; std::memcpy(&expected,&positions[v*4+c],4);
      CHECK(data[start+v*8+c] == expected); CHECK(data[start+v*8+4+c] == payload[c]);
    }
  };
  triangle(first,0); for (UINT i = 24; i < 64; ++i) CHECK(first[i] == 0xcccccccc);
  D3D10_DDI_QUERY_DATA_SO_STATISTICS stats{};
  BOOL overflowResult = TRUE;
  readQuery(statistics,&stats,sizeof(stats)); readQuery(overflow,&overflowResult,sizeof(overflowResult));
  CHECK(stats.NumPrimitivesWritten == 1 && stats.PrimitivesStorageNeeded == 1 && !overflowResult);
  struct { UINT64 before; D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS data; UINT64 after; } counters{};
  counters.before = counters.after = 0xcafebabefeedfaceull;
  readQuery(pipeline,&counters.data,sizeof(counters.data));
  CHECK(counters.before == 0xcafebabefeedfaceull && counters.after == counters.before);
  CHECK(counters.data.IAVertices == 3 && counters.data.IAPrimitives == 1 && counters.data.GSInvocations == 1
    && counters.data.GSPrimitives == 1 && counters.data.PSInvocations == 0);
  std::puts("STREAM_OUTPUT initial bytes/statistics/pipeline PASS");
  bind(geometry,UINT(-1)); f.pfnDraw(device,3,0); ok(); unbind();
  auto appended = words(output); triangle(appended,0); triangle(appended,24);
  for (UINT i = 48; i < 64; ++i) CHECK(appended[i] == 0xcccccccc);
  bind(geometry,64); f.pfnDraw(device,3,0); ok(); unbind();
  const auto offset = words(output); triangle(offset,16);
  for (UINT i = 0; i < 16; ++i) CHECK(offset[i] == appended[i]);
  bind(geometry,0); f.pfnQueryBegin(device,statistics); f.pfnQueryBegin(device,overflow);
  f.pfnDraw(device,9,0); f.pfnQueryEnd(device,overflow); f.pfnQueryEnd(device,statistics); ok(); unbind();
  readQuery(statistics,&stats,sizeof(stats)); readQuery(overflow,&overflowResult,sizeof(overflowResult));
  CHECK(stats.NumPrimitivesWritten == 2 && stats.PrimitivesStorageNeeded == 3 && overflowResult);
  // The overflow predicate must suppress SO as well as ordinary raster draws.
  const auto beforeSuppressed = words(output);
  bind(geometry,0); f.pfnSetPredication(device,overflow,TRUE); f.pfnDraw(device,3,0); ok();
  f.pfnSetPredication(device,{},FALSE); unbind(); CHECK(words(output) == beforeSuppressed);
  // Null holes and high-slot clearing are valid even with a zero clear hint.
  D3D10DDI_HRESOURCE targets[] = {output,{},extra}; UINT offsets[] = {0,0,0};
  f.pfnGsSetShader(device,splitGeometry); f.pfnSoSetTargets(device,3,0,targets,offsets);
  f.pfnDraw(device,3,0); ok(); unbind();
  const auto separate = words(extra);
  for (UINT i = 0; i < 12; ++i) CHECK(separate[i] == payload[i%4]);
  for (UINT i = 12; i < 64; ++i) CHECK(separate[i] == 0xcccccccc);
  f.pfnSoSetTargets(device,3,0,targets,offsets); bind(geometry,0);
  f.pfnDraw(device,3,0); ok(); unbind(); CHECK(words(extra) == separate);
  triangle(words(output),0);
  const auto beforeSparse = words(output);
  bind(sparseGeometry,0); f.pfnDraw(device,3,0); ok(); unbind();
  const auto sparseWords = words(output);
  for (UINT v = 0; v < 3; ++v) {
    for (UINT c = 0; c < 4; ++c) CHECK(sparseWords[v*8+c] == beforeSparse[v*8+c]);
    CHECK(sparseWords[v*8+4] == beforeSparse[v*8+4] && sparseWords[v*8+5] == beforeSparse[v*8+5]);
    CHECK(sparseWords[v*8+6] == payload[0] && sparseWords[v*8+7] == payload[2]);
  }
  // Invalid bindings leave the previous valid binding and counter intact.
  bind(geometry,0);
  UINT badOffset = 1; f.pfnSoSetTargets(device,1,0,&output,&badOffset); invalid();
  badOffset = 260; f.pfnSoSetTargets(device,1,0,&output,&badOffset); invalid();
  f.pfnSoSetTargets(device,5,0,targets,offsets); invalid();
  targets[1] = output; f.pfnSoSetTargets(device,2,0,targets,offsets); invalid();
  f.pfnSoSetTargets(device,1,0,&readback,offsets); invalid();
  f.pfnDraw(device,3,0); ok(); unbind(); triangle(words(output),0);
  std::puts("STREAM_OUTPUT append/reset/overflow/predicate/holes PASS");

  // Reuse the SO counter in IA slot zero. DrawAuto must draw exactly one full-screen triangle.
  D3D10DDI_MIPINFO mip = {16,16,1,16,16,1};
  D3D10DDIARG_CREATERESOURCE texture{}; texture.pMipInfoList = &mip;
  texture.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; texture.Usage = D3D10_DDI_USAGE_DEFAULT;
  texture.BindFlags = D3D10_DDI_BIND_RENDER_TARGET; texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texture.SampleDesc.Count = texture.MipLevels = texture.ArraySize = 1;
  resources.emplace_back(f.pfnCalcPrivateResourceSize(device,&texture));
  auto target = resources.back().handle<D3D10DDI_HRESOURCE>(); f.pfnCreateResource(device,&texture,target,{}); ok();
  texture.Usage = D3D10_DDI_USAGE_STAGING; texture.BindFlags = 0; texture.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  resources.emplace_back(f.pfnCalcPrivateResourceSize(device,&texture));
  auto pixels = resources.back().handle<D3D10DDI_HRESOURCE>(); f.pfnCreateResource(device,&texture,pixels,{}); ok();
  D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc{}; viewDesc.hDrvResource = target;
  viewDesc.Format = texture.Format; viewDesc.ResourceDimension = texture.ResourceDimension; viewDesc.Tex2D.ArraySize = 1;
  Storage viewMemory(f.pfnCalcPrivateRenderTargetViewSize(device,&viewDesc));
  auto view = viewMemory.handle<D3D10DDI_HRENDERTARGETVIEW>();
  f.pfnCreateRenderTargetView(device,&viewDesc,view,{}); ok();
  D3D10DDIARG_INPUT_ELEMENT_DESC element{}; element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  element.InputSlotClass = D3D10_DDI_INPUT_PER_VERTEX_DATA;
  D3D10DDIARG_CREATEELEMENTLAYOUT layoutDesc = {&element,1};
  Storage layoutMemory(f.pfnCalcPrivateElementLayoutSize(device,&layoutDesc));
  auto layout = layoutMemory.handle<D3D10DDI_HELEMENTLAYOUT>();
  f.pfnCreateElementLayout(device,&layoutDesc,layout,{}); ok();
  f.pfnGsSetShader(device,{}); f.pfnVsSetShader(device,replay); f.pfnPsSetShader(device,pixel);
  f.pfnIaSetInputLayout(device,layout); UINT stride = 32, zero = 0;
  f.pfnIaSetVertexBuffers(device,0,1,&output,&stride,&zero);
  f.pfnSetRenderTargets(device,&view,1,0,{}); D3D10_DDI_VIEWPORT viewport = {0,0,16,16,0,1};
  f.pfnSetViewports(device,1,0,&viewport); ok();
  FLOAT black[] = {0,0,0,1};
  unsigned draws = 0;
  for (BOOL value : {TRUE,FALSE}) {
    f.pfnClearRenderTargetView(device,view,black); ok();
    f.pfnSetPredication(device,overflow,value); f.pfnDrawAuto(device); ok();
    f.pfnSetPredication(device,{},FALSE); f.pfnResourceCopy(device,pixels,target); ok();
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.pfnStagingResourceMap(device,pixels,0,D3D10_DDI_MAP_READ,0,&map); ok();
    CHECK(map.pData && map.RowPitch >= 64);
    const UINT expected = value ? 0xff000000 : 0xff0000ff;
    for (UINT y = 0; y < 16; ++y) for (UINT x = 0; x < 16; ++x)
      CHECK(reinterpret_cast<const UINT*>(static_cast<const char*>(map.pData)+y*map.RowPitch)[x] == expected);
    f.pfnStagingResourceUnmap(device,pixels,0); ok(); ++draws;
  }
  D3D10DDI_HRESOURCE none{}; f.pfnIaSetVertexBuffers(device,0,1,&none,&zero,&zero);
  f.pfnIaSetInputLayout(device,{}); f.pfnSetRenderTargets(device,nullptr,0,0,{});
  f.pfnVsSetShader(device,{}); f.pfnPsSetShader(device,{});
  f.pfnDestroyElementLayout(device,layout); f.pfnDestroyRenderTargetView(device,view);
  for (const auto& object : queries) f.pfnDestroyQuery(device,object.handle<D3D10DDI_HQUERY>());
  for (const auto& object : shaders) f.pfnDestroyShader(device,object.handle<D3D10DDI_HSHADER>());
  for (const auto& object : resources) f.pfnDestroyResource(device,object.handle<D3D10DDI_HRESOURCE>());
  ok(); f.pfnFlush(device); f.pfnDestroyDevice(device); ok();
  std::printf("native stream output PASS checks=%u drawauto-cases=%u pixels-per-case=256; native admission remains closed\n",checks,draws);
}
