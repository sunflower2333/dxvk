// SPDX-License-Identifier: Zlib
// Standalone public-core native GPU probe. Load the exact matching DXVK d3d11
// DLL explicitly; this is not ordinary Windows UMD discovery/admission.
// Not registered with existing CI/harnesses; local validation is compile only.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

using Microsoft::WRL::ComPtr;
static unsigned checks,cases,words;
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"public predicate action failure case=%u line=%d expression=%s\n",cases,__LINE__,#v);std::exit(1); } } while (0)
using CreateDevice=HRESULT (WINAPI*)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,
  const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
static ComPtr<ID3D11Device> device;
static ComPtr<ID3D11DeviceContext> immediate;
struct Pipeline {
  ComPtr<ID3D11VertexShader> vs;
  ComPtr<ID3D11PixelShader> ps;
  ComPtr<ID3D11RenderTargetView> rtv;
  ComPtr<ID3D11RasterizerState> raster;
  void bind(ID3D11DeviceContext* context) const {
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
    auto view=rtv.Get();context->OMSetRenderTargets(1,&view,nullptr);
    context->RSSetState(raster.Get());D3D11_VIEWPORT viewport{0,0,16,16,0,1};
    context->RSSetViewports(1,&viewport);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }
  Pipeline() {
    HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");CHECK(compiler);
    auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler,"D3DCompile"));CHECK(compile);
    constexpr char source[]=R"(
float4 vs(uint id:SV_VertexID):SV_Position {
  float2 xy=float2((id<<1)&2,id&2);
  return float4(xy*float2(2,-2)+float2(-1,1),0,1);
}
float4 ps():SV_Target { return float4(1,0,0,1); }
)";
    ComPtr<ID3DBlob> vertex,pixel,errors;
    CHECK(compile(source,sizeof(source),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vertex,&errors)==S_OK);
    CHECK(compile(source,sizeof(source),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&pixel,&errors)==S_OK);
    CHECK(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs)==S_OK);
    CHECK(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps)==S_OK);
    D3D11_TEXTURE2D_DESC texture{};texture.Width=texture.Height=16;texture.MipLevels=texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;texture.SampleDesc.Count=1;texture.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;CHECK(device->CreateTexture2D(&texture,nullptr,&target)==S_OK);
    CHECK(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)==S_OK);
    D3D11_RASTERIZER_DESC state{};state.FillMode=D3D11_FILL_SOLID;state.CullMode=D3D11_CULL_NONE;state.DepthClipEnable=TRUE;
    CHECK(device->CreateRasterizerState(&state,&raster)==S_OK);
  }
};
struct Resources {
  ComPtr<ID3D11Buffer> destination,staging,source;
  ComPtr<ID3D11UnorderedAccessView> counter;
  static constexpr std::array<UINT,4> initial{{0x13b58ad0,0x456a12ce,0x917cd53b,0xfb7542aa}};
  Resources() {
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;
    D3D11_SUBRESOURCE_DATA data{initial.data(),0,0};CHECK(device->CreateBuffer(&desc,&data,&destination)==S_OK);
    desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(device->CreateBuffer(&desc,nullptr,&staging)==S_OK);
    desc={};desc.ByteWidth=64;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.StructureByteStride=4;
    CHECK(device->CreateBuffer(&desc,nullptr,&source)==S_OK);
    D3D11_UNORDERED_ACCESS_VIEW_DESC view{};view.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
    view.Buffer.NumElements=16;view.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_APPEND;
    CHECK(device->CreateUnorderedAccessView(source.Get(),&view,&counter)==S_OK);
  }
  void setCount(ID3D11DeviceContext* context,UINT value) const {
    auto view=counter.Get();context->CSSetUnorderedAccessViews(0,1,&view,&value);
    view=nullptr;context->CSSetUnorderedAccessViews(0,1,&view,nullptr);
  }
  void check(const std::array<UINT,4>& expected) const {
    immediate->SetPredication(nullptr,FALSE);immediate->CopyResource(staging.Get(),destination.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};CHECK(immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)==S_OK && mapped.pData);
    std::array<UINT,4> actual{};std::memcpy(actual.data(),mapped.pData,sizeof(actual));immediate->Unmap(staging.Get(),0);
    char name[80];CHECK(std::snprintf(name,sizeof(name),"predicate-action-%03u.bin",cases)>0);
    std::ofstream output(name,std::ios::binary);CHECK(output.is_open());
    output.write(reinterpret_cast<const char*>(actual.data()),sizeof(actual));output.close();CHECK(!output.fail());
    for (unsigned i=0;i<actual.size();++i) {
      if (actual[i]!=expected[i]) std::fprintf(stderr,"predicate action word case=%u index=%u actual=%08x expected=%08x\n",cases,i,actual[i],expected[i]);
      CHECK(actual[i]==expected[i]);++words;
    }
    ++cases;
  }
};
constexpr std::array<UINT,4> Resources::initial;
static ComPtr<ID3D11Predicate> predicate(bool hint) {
  D3D11_QUERY_DESC desc{D3D11_QUERY_OCCLUSION_PREDICATE,hint ? D3D11_QUERY_MISC_PREDICATEHINT : 0u};
  ComPtr<ID3D11Predicate> result;CHECK(device->CreatePredicate(&desc,&result)==S_OK);return result;
}
static void issue(ID3D11DeviceContext* context,const Pipeline& pipeline,ID3D11Predicate* query,bool visible) {
  context->SetPredication(nullptr,FALSE);pipeline.bind(context);context->Begin(query);
  if (visible) context->Draw(3,0);
  context->End(query);
}
static void binding(ID3D11DeviceContext* context,ID3D11Predicate* expected,BOOL value) {
  ComPtr<ID3D11Predicate> actual;BOOL actualValue=234;
  context->GetPredication(&actual,&actualValue);CHECK(actual.Get()==expected && actualValue==value);
}
int main(int argc,char** argv) {
  if (argc!=2) return 2;
  HMODULE backend=LoadLibraryA(argv[1]);CHECK(backend);
  auto create=reinterpret_cast<CreateDevice>(GetProcAddress(backend,"D3D11CreateDevice"));CHECK(create);
  const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
  CHECK(create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&immediate)==S_OK);
  Pipeline pipeline;
  for (bool visible : {false,true}) for (BOOL value : {FALSE,TRUE}) for (bool hint : {false,true}) {
    Resources resource;auto query=predicate(hint);issue(immediate.Get(),pipeline,query.Get(),visible);
    immediate->SetPredication(query.Get(),value);resource.setCount(immediate.Get(),13);
    immediate->CopyStructureCount(resource.destination.Get(),4,resource.counter.Get());binding(immediate.Get(),query.Get(),value);
    immediate->SetPredication(nullptr,-17);binding(immediate.Get(),nullptr,-17);
    auto expected=Resources::initial;if (hint || visible!=bool(value)) expected[1]=13;resource.check(expected);
  }
  for (BOOL value : {FALSE,TRUE}) {
    Resources resource;resource.setCount(immediate.Get(),7);immediate->SetPredication(nullptr,value);
    immediate->CopyStructureCount(resource.destination.Get(),4,resource.counter.Get());binding(immediate.Get(),nullptr,value);
    auto expected=Resources::initial;expected[1]=7;resource.check(expected);
  }
  // Same query's first historical result must survive a later prefixed End.
  // Repeat each immutable list; use nested lists and release query/source API
  // owners after recording, so only command/list ownership keeps them alive.
  for (bool nested : {false,true}) for (BOOL restore : {FALSE,TRUE}) {
    Resources resource;ComPtr<ID3D11DeviceContext> deferred;CHECK(device->CreateDeferredContext(0,&deferred)==S_OK);
    auto query=predicate(false);issue(deferred.Get(),pipeline,query.Get(),false);
    deferred->SetPredication(query.Get(),FALSE);resource.setCount(deferred.Get(),7);
    deferred->CopyStructureCount(resource.destination.Get(),4,resource.counter.Get());
    deferred->SetPredication(nullptr,FALSE);issue(deferred.Get(),pipeline,query.Get(),true);
    deferred->SetPredication(query.Get(),FALSE);resource.setCount(deferred.Get(),13);
    deferred->CopyStructureCount(resource.destination.Get(),8,resource.counter.Get());deferred->SetPredication(nullptr,FALSE);
    ComPtr<ID3D11CommandList> list;CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);
    if (nested) {
      deferred->ExecuteCommandList(list.Get(),restore);list.Reset();CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);
    }
    query.Reset();resource.counter.Reset();resource.source.Reset();deferred.Reset();
    auto saved=predicate(false);issue(immediate.Get(),pipeline,saved.Get(),false);
    immediate->SetPredication(saved.Get(),TRUE);
    for (unsigned replay=0;replay<3;++replay) {
      immediate->SetPredication(nullptr,FALSE);immediate->UpdateSubresource(resource.destination.Get(),0,nullptr,Resources::initial.data(),0,0);
      immediate->SetPredication(saved.Get(),TRUE);immediate->ExecuteCommandList(list.Get(),restore);
      binding(immediate.Get(),restore ? saved.Get() : nullptr,restore ? TRUE : FALSE);
      auto expected=Resources::initial;expected[2]=13;resource.check(expected);
    }
    list.Reset();
  }
  immediate->ClearState();immediate->Flush();
  CHECK(cases==22 && words==88);
  std::printf("D3D11 predicate action native PASS checks=%u cases=%u words=%u immediate_matrix=8 null=2 deferred=12 query_reissue=1 repeats=3 nested=1 restore=2 raw_files=22 ordinary_runtime_admission=0 predication_complete=0\n",checks,cases,words);
}
