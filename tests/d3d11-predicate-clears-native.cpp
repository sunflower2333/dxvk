// SPDX-License-Identifier: Zlib
// Standalone public-core render-target clear GPU probe. Load the exact matching DXVK d3d11
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
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"public predicate clear failure case=%u line=%d expression=%s\n",cases,__LINE__,#v);std::exit(1); } } while (0)
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
  bool buffer;
  ComPtr<ID3D11Resource> target,staging;
  ComPtr<ID3D11RenderTargetView> view;
  std::array<UINT,256> initial{};
  explicit Resources(bool b=false) : buffer(b) {
    for (unsigned i=0;i<initial.size();++i) initial[i]=0x28a9f573u^(i*0x1357acdfu);
    D3D11_SUBRESOURCE_DATA data{initial.data(),64,1024};
    if (buffer) {
      D3D11_BUFFER_DESC desc{};desc.ByteWidth=64;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
      ComPtr<ID3D11Buffer> resource;CHECK(device->CreateBuffer(&desc,&data,&resource)==S_OK);target=resource;
      D3D11_RENDER_TARGET_VIEW_DESC rtv{};rtv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;rtv.ViewDimension=D3D11_RTV_DIMENSION_BUFFER;
      rtv.Buffer.NumElements=16;CHECK(device->CreateRenderTargetView(target.Get(),&rtv,&view)==S_OK);
      desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      CHECK(device->CreateBuffer(&desc,nullptr,&resource)==S_OK);staging=resource;
    } else {
      D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=16;desc.MipLevels=desc.ArraySize=1;
      desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
      ComPtr<ID3D11Texture2D> resource;CHECK(device->CreateTexture2D(&desc,&data,&resource)==S_OK);target=resource;
      CHECK(device->CreateRenderTargetView(target.Get(),nullptr,&view)==S_OK);
      desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      CHECK(device->CreateTexture2D(&desc,nullptr,&resource)==S_OK);staging=resource;
    }
  }
  void reset() const { immediate->UpdateSubresource(target.Get(),0,nullptr,initial.data(),64,1024); }
  void check(bool cleared,UINT color) const {
    immediate->SetPredication(nullptr,FALSE);immediate->CopyResource(staging.Get(),target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};CHECK(immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)==S_OK && mapped.pData);
    CHECK(buffer || mapped.RowPitch>=64);
    std::array<UINT,256> actual{};
    const unsigned rows=buffer ? 1 : 16;
    for (unsigned y=0;y<rows;++y) std::memcpy(actual.data()+y*16,static_cast<const char*>(mapped.pData)+y*mapped.RowPitch,64);
    immediate->Unmap(staging.Get(),0);
    char name[80];CHECK(std::snprintf(name,sizeof(name),"predicate-clear-%03u.rgba",cases)>0);
    std::ofstream output(name,std::ios::binary);CHECK(output.is_open());
    output.write(reinterpret_cast<const char*>(actual.data()),rows*64);output.close();CHECK(!output.fail());
    for (unsigned i=0;i<rows*16;++i) {
      const UINT expected=cleared ? color : initial[i];
      if (actual[i]!=expected) std::fprintf(stderr,"predicate clear pixel case=%u buffer=%u x=%u y=%u actual=%08x expected=%08x\n",cases,unsigned(buffer),i%16,i/16,actual[i],expected);
      CHECK(actual[i]==expected);++words;
    }
    ++cases;
  }
};
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
  constexpr FLOAT red[]{1,0,0,1},blue[]{0,0,1,1},green[]{0,1,0,1},white[]{1,1,1,1},purple[]{1,0,1,1};
  for (bool visible : {false,true}) for (BOOL value : {FALSE,TRUE}) for (bool hint : {false,true}) {
    Resources resource;auto query=predicate(hint);issue(immediate.Get(),pipeline,query.Get(),visible);
    immediate->SetPredication(query.Get(),value);pipeline.bind(immediate.Get());
    immediate->ClearRenderTargetView(resource.view.Get(),red);binding(immediate.Get(),query.Get(),value);
    immediate->SetPredication(nullptr,-17);binding(immediate.Get(),nullptr,-17);
    resource.check(hint || visible!=bool(value),0xff0000ffu);
  }
  for (BOOL value : {FALSE,TRUE}) {
    Resources resource;immediate->SetPredication(nullptr,value);
    immediate->ClearRenderTargetView(resource.view.Get(),red);binding(immediate.Get(),nullptr,value);
    resource.check(true,0xff0000ffu);
  }
  for (bool visible : {false,true}) for (BOOL value : {FALSE,TRUE}) {
    Resources resource(true);auto query=predicate(false);issue(immediate.Get(),pipeline,query.Get(),visible);
    immediate->SetPredication(query.Get(),value);immediate->ClearRenderTargetView(resource.view.Get(),red);
    resource.check(visible!=bool(value),0xff0000ffu);
  }
  for (bool nested : {false,true}) for (BOOL restore : {FALSE,TRUE}) {
    Resources unbound(true),first,explicitNull(true),last,afterNested(true),afterExecution;
    ComPtr<ID3D11DeviceContext> deferred;CHECK(device->CreateDeferredContext(0,&deferred)==S_OK);
    binding(deferred.Get(),nullptr,FALSE);
    // This default clear must execute even if the immediate parent suppresses.
    deferred->ClearRenderTargetView(unbound.view.Get(),blue);
    auto query=predicate(false);issue(deferred.Get(),pipeline,query.Get(),false);
    deferred->SetPredication(query.Get(),FALSE);deferred->ClearRenderTargetView(first.view.Get(),red);
    deferred->SetPredication(nullptr,-17);binding(deferred.Get(),nullptr,-17);
    deferred->ClearRenderTargetView(explicitNull.view.Get(),green);
    issue(deferred.Get(),pipeline,query.Get(),true);deferred->SetPredication(query.Get(),FALSE);
    deferred->ClearRenderTargetView(last.view.Get(),red);deferred->SetPredication(nullptr,FALSE);
    ComPtr<ID3D11CommandList> list;CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);
    binding(deferred.Get(),nullptr,FALSE);
    auto saved=predicate(false);issue(immediate.Get(),pipeline,saved.Get(),false);
    if (nested) {
      deferred->SetPredication(saved.Get(),FALSE);
      deferred->ExecuteCommandList(list.Get(),restore);list.Reset();
      binding(deferred.Get(),restore ? saved.Get() : nullptr,FALSE);
      deferred->ClearRenderTargetView(afterNested.view.Get(),white);
      CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);binding(deferred.Get(),nullptr,FALSE);
    }
    // Release recorded API RTV/query owners. Observer resources are retained
    // for readback, and actions must own image views and buffer shadows.
    query.Reset();unbound.view.Reset();first.view.Reset();explicitNull.view.Reset();last.view.Reset();afterNested.view.Reset();deferred.Reset();
    for (unsigned replay=0;replay<3;++replay) {
      immediate->SetPredication(nullptr,FALSE);
      unbound.reset();first.reset();explicitNull.reset();last.reset();afterNested.reset();afterExecution.reset();
      immediate->SetPredication(saved.Get(),FALSE);immediate->ExecuteCommandList(list.Get(),restore);
      binding(immediate.Get(),restore ? saved.Get() : nullptr,FALSE);
      immediate->ClearRenderTargetView(afterExecution.view.Get(),purple);
      unbound.check(true,0xffff0000u);first.check(false,0xff0000ffu);explicitNull.check(true,0xff00ff00u);last.check(true,0xff0000ffu);
      afterExecution.check(!restore,0xffff00ffu);
      if (nested) afterNested.check(!restore,0xffffffffu);
    }
    list.Reset();
  }
  immediate->ClearState();immediate->Flush();
  CHECK(cases==80 && words==12320);
  std::printf("D3D11 predicate clear native PASS checks=%u cases=%u words=%u immediate_matrix=8 null=2 buffer_matrix=4 deferred_replays=12 default_null=1 explicit_null=1 historical_ends=2 nested_restore=2 parent_restore=2 raw_files=80 ordinary_runtime_admission=0 predication_complete=0\n",checks,cases,words);
}
