// SPDX-License-Identifier: Zlib
// Standalone exact-core DSV/UAV predicate probe, compiled only locally.
// No ordinary UMD discovery, native runner or hardware admission claim.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks,cases,fields,rawBytes;
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"predicate view clear failure case=%u line=%d expression=%s\n",cases,__LINE__,#v);std::exit(1); } } while (0)
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
static constexpr UINT integer[]{0xdeadbeefu,0x01234567u,0x89abcdefu,0xa55a5aa5u};
static constexpr FLOAT floating[]{0.25f,-2.0f,3.0f,1.0f};
static constexpr UINT floatBits[]{0x3e800000u,0xc0000000u,0x40400000u,0x3f800000u};
static UINT initial(unsigned i) { return 0x7e91c523u^(i*0x1357acdfu); }
static void word(std::vector<unsigned char>& bytes,UINT value) {
  bytes.push_back(static_cast<unsigned char>(value));bytes.push_back(static_cast<unsigned char>(value>>8));
  bytes.push_back(static_cast<unsigned char>(value>>16));bytes.push_back(static_cast<unsigned char>(value>>24));
}
static void literal(unsigned kind,unsigned index,UINT actual,UINT expected) {
  if (actual!=expected) std::fprintf(stderr,"predicate view clear field case=%u kind=%u index=%u actual=%08x expected=%08x\n",cases,kind,index,actual,expected);
  CHECK(actual==expected);++fields;
}
struct DepthStencil {
  ComPtr<ID3D11Texture2D> target,staging;
  std::array<ComPtr<ID3D11DepthStencilView>,4> views;
  DepthStencil() {
    D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R32G8X24_TYPELESS;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    CHECK(device->CreateTexture2D(&desc,nullptr,&target)==S_OK);
    for (UINT flags=0;flags<4;++flags) {
      D3D11_DEPTH_STENCIL_VIEW_DESC view{};view.Format=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
      view.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;view.Flags=flags;
      CHECK(device->CreateDepthStencilView(target.Get(),&view,&views[flags])==S_OK);
    }
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(device->CreateTexture2D(&desc,nullptr,&staging)==S_OK);reset();
  }
  void reset() const {
    // A new temporary view does not keep any released recorded API view alive.
    D3D11_DEPTH_STENCIL_VIEW_DESC desc{};desc.Format=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    desc.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11DepthStencilView> view;CHECK(device->CreateDepthStencilView(target.Get(),&desc,&view)==S_OK);
    immediate->ClearDepthStencilView(view.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,0.75f,0x3c);
  }
  void clear(ID3D11DeviceContext* context,UINT flags=3,UINT readOnly=0) const {
    context->ClearDepthStencilView(views[readOnly].Get(),flags,0.25f,0xa7);
  }
  void read(std::vector<unsigned char>& bytes,bool execute,UINT aspects) const {
    immediate->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK(immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)==S_OK && mapped.pData && mapped.RowPitch>=32);
    for (unsigned y=0;y<4;++y) for (unsigned x=0;x<4;++x) {
      const auto pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*8;
      UINT depth;std::memcpy(&depth,pixel,4);const UINT stencil=pixel[4];
      literal(0,y*4+x,depth,execute && (aspects&1) ? 0x3e800000u : 0x3f400000u);
      literal(1,y*4+x,stencil,execute && (aspects&2) ? 0xa7u : 0x3cu);
      word(bytes,depth);bytes.push_back(static_cast<unsigned char>(stencil));
    }
    immediate->Unmap(staging.Get(),0);
  }
  void releaseViews() { for (auto& view : views) view.Reset(); }
};
struct Uav {
  unsigned kind,count;
  bool buffer;
  ComPtr<ID3D11Resource> target,staging;
  ComPtr<ID3D11UnorderedAccessView> view;
  explicit Uav(unsigned k) : kind(k),count(k==0 ? 8 : k==1 || k==3 ? 32 : 64),buffer(k==0 || k==1 || k==3) {
    std::array<UINT,64> data{};for (unsigned i=0;i<data.size();++i) data[i]=initial(i);
    D3D11_SUBRESOURCE_DATA source{data.data(),64,256};
    const DXGI_FORMAT format=kind<3 ? DXGI_FORMAT_R32G32B32A32_UINT : DXGI_FORMAT_R32G32B32A32_FLOAT;
    if (buffer) {
      D3D11_BUFFER_DESC desc{};desc.ByteWidth=count*4;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
      desc.MiscFlags=kind==0 ? D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS : 0;
      ComPtr<ID3D11Buffer> resource;CHECK(device->CreateBuffer(&desc,&source,&resource)==S_OK);target=resource;
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=kind==0 ? DXGI_FORMAT_R32_TYPELESS : format;
      uav.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=kind==0 ? count : count/4;
      uav.Buffer.Flags=kind==0 ? D3D11_BUFFER_UAV_FLAG_RAW : 0;
      CHECK(device->CreateUnorderedAccessView(target.Get(),&uav,&view)==S_OK);
      desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      CHECK(device->CreateBuffer(&desc,nullptr,&resource)==S_OK);staging=resource;
    } else {
      D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=4;desc.MipLevels=desc.ArraySize=1;
      desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
      ComPtr<ID3D11Texture2D> resource;CHECK(device->CreateTexture2D(&desc,&source,&resource)==S_OK);target=resource;
      CHECK(device->CreateUnorderedAccessView(target.Get(),nullptr,&view)==S_OK);
      desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      CHECK(device->CreateTexture2D(&desc,nullptr,&resource)==S_OK);staging=resource;
    }
  }
  void reset() const {
    std::array<UINT,64> data{};for (unsigned i=0;i<data.size();++i) data[i]=initial(i);
    immediate->UpdateSubresource(target.Get(),0,nullptr,data.data(),64,256);
  }
  void clear(ID3D11DeviceContext* context) const {
    if (kind<3) context->ClearUnorderedAccessViewUint(view.Get(),integer);
    else context->ClearUnorderedAccessViewFloat(view.Get(),floating);
  }
  void read(std::vector<unsigned char>& bytes,bool execute) const {
    immediate->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK(immediate->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)==S_OK && mapped.pData);
    CHECK(buffer || mapped.RowPitch>=64);
    for (unsigned i=0;i<count;++i) {
      const auto data=static_cast<const unsigned char*>(mapped.pData)+(buffer ? i*4 : (i/16)*mapped.RowPitch+(i%16)*4);
      UINT actual;std::memcpy(&actual,data,4);
      const UINT clear=kind==0 ? integer[0] : kind<3 ? integer[i%4] : floatBits[i%4];
      literal(kind+2,i,actual,execute ? clear : initial(i));word(bytes,actual);
    }
    immediate->Unmap(staging.Get(),0);
  }
};
struct Bundle {
  DepthStencil depth;
  std::array<Uav,5> uav{Uav(0),Uav(1),Uav(2),Uav(3),Uav(4)};
  void reset() const { depth.reset();for (const auto& item : uav) item.reset(); }
  void clear(ID3D11DeviceContext* context,UINT flags=3) const {
    depth.clear(context,flags);for (const auto& item : uav) item.clear(context);
  }
  void releaseViews() { depth.releaseViews();for (auto& item : uav) item.view.Reset(); }
  void snapshot(bool execute,UINT aspects=3,bool uavExecute=true) const {
    immediate->SetPredication(nullptr,FALSE);std::vector<unsigned char> bytes;
    depth.read(bytes,execute,aspects);for (const auto& item : uav) item.read(bytes,execute && uavExecute);
    CHECK(bytes.size()==880);char name[80];CHECK(std::snprintf(name,sizeof(name),"predicate-view-clear-%03u.bytes",cases)>0);
    std::ofstream output(name,std::ios::binary);CHECK(output.is_open());
    output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());output.close();CHECK(!output.fail());
    rawBytes+=static_cast<unsigned>(bytes.size());++cases;
  }
};
static ComPtr<ID3D11Predicate> predicate(bool hint) {
  D3D11_QUERY_DESC desc{D3D11_QUERY_OCCLUSION_PREDICATE,hint ? D3D11_QUERY_MISC_PREDICATEHINT : 0u};
  ComPtr<ID3D11Predicate> result;CHECK(device->CreatePredicate(&desc,&result)==S_OK);return result;
}
static void issue(ID3D11DeviceContext* context,const Pipeline& pipeline,ID3D11Predicate* query,bool visible) {
  context->SetPredication(nullptr,FALSE);pipeline.bind(context);context->Begin(query);
  if (visible) context->Draw(3,0);context->End(query);
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
    Bundle resource;auto query=predicate(hint);issue(immediate.Get(),pipeline,query.Get(),visible);
    immediate->SetPredication(query.Get(),value);resource.clear(immediate.Get());binding(immediate.Get(),query.Get(),value);
    resource.snapshot(hint || visible!=bool(value));
  }
  for (BOOL value : {FALSE,TRUE}) {
    Bundle resource;immediate->SetPredication(nullptr,value);resource.clear(immediate.Get());binding(immediate.Get(),nullptr,value);
    resource.snapshot(true);
  }
  constexpr UINT aspects[][2]{{0,0},{1,0},{2,0},{3,0},{3,1},{3,2},{3,3}};
  for (const auto& test : aspects) {
    Bundle resource;immediate->SetPredication(nullptr,FALSE);resource.depth.clear(immediate.Get(),test[0],test[1]);
    resource.snapshot(true,test[0]&~test[1],false);
  }
  for (bool nested : {false,true}) for (BOOL restore : {FALSE,TRUE}) {
    Bundle unbound,first,explicitNull,last,afterNested,afterExecution;
    ComPtr<ID3D11DeviceContext> deferred;CHECK(device->CreateDeferredContext(0,&deferred)==S_OK);
    binding(deferred.Get(),nullptr,FALSE);unbound.clear(deferred.Get(),1);
    auto query=predicate(false);issue(deferred.Get(),pipeline,query.Get(),false);
    deferred->SetPredication(query.Get(),FALSE);first.clear(deferred.Get());
    deferred->SetPredication(nullptr,-17);binding(deferred.Get(),nullptr,-17);explicitNull.clear(deferred.Get(),2);
    issue(deferred.Get(),pipeline,query.Get(),true);deferred->SetPredication(query.Get(),FALSE);last.clear(deferred.Get());
    deferred->SetPredication(nullptr,FALSE);ComPtr<ID3D11CommandList> list;
    CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);binding(deferred.Get(),nullptr,FALSE);
    auto parent=predicate(false);issue(immediate.Get(),pipeline,parent.Get(),false);
    if (nested) {
      deferred->SetPredication(parent.Get(),FALSE);deferred->ExecuteCommandList(list.Get(),restore);list.Reset();
      binding(deferred.Get(),restore ? parent.Get() : nullptr,FALSE);afterNested.clear(deferred.Get());
      CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);binding(deferred.Get(),nullptr,FALSE);
    }
    query.Reset();unbound.releaseViews();first.releaseViews();explicitNull.releaseViews();last.releaseViews();afterNested.releaseViews();deferred.Reset();
    for (unsigned replay=0;replay<3;++replay) {
      immediate->SetPredication(nullptr,FALSE);
      unbound.reset();first.reset();explicitNull.reset();last.reset();afterNested.reset();afterExecution.reset();
      immediate->SetPredication(parent.Get(),FALSE);immediate->ExecuteCommandList(list.Get(),restore);
      binding(immediate.Get(),restore ? parent.Get() : nullptr,FALSE);afterExecution.clear(immediate.Get());
      unbound.snapshot(true,1);first.snapshot(false);explicitNull.snapshot(true,2);last.snapshot(true);
      afterExecution.snapshot(!restore);if (nested) afterNested.snapshot(!restore);
    }
    list.Reset();
  }
  immediate->ClearState();immediate->Flush();CHECK(cases==83 && fields==19256 && rawBytes==73040);
  std::printf("D3D11 predicate view clear native PASS checks=%u cases=83 fields=19256 bytes=73040 immediate_matrix=8 null=2 aspects=7 deferred_replays=12 default_null=1 explicit_null=1 historical_ends=2 nested_restore=2 parent_restore=2 raw_files=83 ordinary_runtime_admission=0 predication_complete=0\n",checks);
}
