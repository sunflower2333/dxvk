// SPDX-License-Identifier: MIT
// Typed historical DDIs against original FXC and the public D3D10.1 runtime.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_contract.h"
#include "../src/umd/umd_result.h"
#include "umd-probe-shaders.h"
#include <d3d10_1.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <type_traits>
using Microsoft::WRL::ComPtr;
static unsigned checks, programs, frames, scenes;
static HRESULT lastError=S_OK;
static DWORD caller;
static ComPtr<ID3D11Device> created;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"D3D10 depth line %d: %s hr=%08lx\n",__LINE__,#x,static_cast<unsigned long>(lastError)); std::abort(); } } while (0)
static void APIENTRY error(D3D10DDI_HRTCORELAYER,HRESULT hr) {
  CHECK(GetCurrentThreadId()==caller&&FAILED(hr));lastError=hr;
}
static void expect() { CHECK(lastError==S_OK);lastError=S_OK; }
HRESULT dxvk::umd::createDevice(const LUID&,D3D_FEATURE_LEVEL logical,ID3D11Device** device,
    ID3D11DeviceContext** context,const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime&&(logical==D3D_FEATURE_LEVEL_10_0||logical==D3D_FEATURE_LEVEL_10_1));
  const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
  const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if(hr==S_OK)created=*device;return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush();return S_OK; }
struct Storage {
  std::unique_ptr<void,decltype(&std::free)> bytes;
  explicit Storage(SIZE_T size):bytes(std::calloc(1,size),&std::free){CHECK(size&&bytes);}
};
static void retain(const char* name,const void* bytes,size_t size) {
  CHECK(size<=MAXDWORD);
  HANDLE file=CreateFileA(name,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  CHECK(file!=INVALID_HANDLE_VALUE);DWORD written=0;
  CHECK(WriteFile(file,bytes,DWORD(size),&written,nullptr)&&written==size);CHECK(CloseHandle(file));
}
static constexpr char source[]=R"(
float4 vs(uint vertex:SV_VertexID):SV_Position {
  float2 xy=float2((vertex<<1)&2,vertex&2)*float2(2,-2)+float2(-1,1);
  return float4(xy,0.25+0.125*(xy.x+1),1);
}
float ps_depth():SV_Depth { return 0.625; }
void ps_empty() {}
void ps_discard() { discard; }
)";
struct Program {
  std::vector<UINT> tokens;
  ComPtr<ID3DBlob> binary;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs,outputs;
  D3D10DDIARG_STAGE_IO_SIGNATURES sig{};
  Program(const char* entry,const char* profile) {
    CHECK(compileHlslTokens(source,entry,profile,tokens,&binary));
    char name[128];CHECK(std::snprintf(name,sizeof(name),"depth-fxc-%02u-%s-%s.dxbc",++programs,entry,profile)>0);
    retain(name,binary->GetBufferPointer(),binary->GetBufferSize());
    CHECK(std::snprintf(name,sizeof(name),"depth-fxc-%02u-%s-%s.tokens",programs,entry,profile)>0);
    retain(name,tokens.data(),tokens.size()*4);
    ComPtr<ID3D11ShaderReflection> reflection;
    CHECK(D3DReflect(binary->GetBufferPointer(),binary->GetBufferSize(),__uuidof(ID3D11ShaderReflection),&reflection)==S_OK);
    D3D11_SHADER_DESC desc{};CHECK(reflection->GetDesc(&desc)==S_OK);
    for(bool input:{true,false})for(UINT i=0;i<(input?desc.InputParameters:desc.OutputParameters);++i) {
      D3D11_SIGNATURE_PARAMETER_DESC p{};
      CHECK((input?reflection->GetInputParameterDesc(i,&p):reflection->GetOutputParameterDesc(i,&p))==S_OK);
      if(p.Register==UINT(-1))continue; // SV_Depth is a dedicated operand.
      CHECK(UINT(p.SystemValueType)<=10);
      (input?inputs:outputs).push_back({D3D10_SB_NAME(p.SystemValueType),p.Register,p.Mask});
    }
    sig={inputs.data(),UINT(inputs.size()),outputs.data(),UINT(outputs.size())};
  }
};
using Counters=std::array<UINT64,10>;
static UINT expectedDepth(UINT mode,UINT x) {
  const float value=mode==1?0.625f:(mode==0||mode==2?0.25f+float(2*x+1)/128.0f:1.0f);
  UINT bits;std::memcpy(&bits,&value,4);return bits;
}
static UINT64 expectedOcclusion(UINT mode) { return mode==3||mode==4?0:256; }
static void checkCounters(const Counters& c,UINT mode) {
  CHECK(c[0]==3&&c[1]==1&&c[2]>0&&c[3]==0&&c[4]==0);
  if(mode==0||mode>=4)CHECK(c[7]==0);
  else if(mode==1||mode==2)CHECK(c[7]>0);
  // A static discard has no surviving OM samples establishing a positive
  // lower bound; retain its PS counter without inventing one.
  CHECK(c[8]==expectedOcclusion(mode)&&c[9]==0);
}
static void retainScene(UINT model,UINT mode,const char* role,const std::vector<UINT>& pixels,const Counters& counters) {
  CHECK(pixels.size()==256);for(UINT y=0;y<16;++y)for(UINT x=0;x<16;++x)CHECK(pixels[y*16+x]==expectedDepth(mode,x));
  checkCounters(counters,mode);char name[128];
  CHECK(std::snprintf(name,sizeof(name),"depth-model%u-case%u-%s.bin",model,mode,role)>0);
  retain(name,pixels.data(),pixels.size()*4);
  CHECK(std::snprintf(name,sizeof(name),"depth-model%u-case%u-%s-query.bin",model,mode,role)>0);
  retain(name,counters.data(),sizeof(counters));++frames;
}
static std::vector<UINT> readback11(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Resource* resource) {
  ComPtr<ID3D11Texture2D> texture;CHECK(resource->QueryInterface(IID_PPV_ARGS(&texture))==S_OK);
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=desc.MiscFlags=0;
  desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> staging;CHECK(device->CreateTexture2D(&desc,nullptr,&staging)==S_OK);
  context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};
  CHECK(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)==S_OK&&map.pData&&map.RowPitch>=64);
  std::vector<UINT> data(256);for(UINT y=0;y<16;++y)std::memcpy(data.data()+y*16,static_cast<const char*>(map.pData)+size_t(y)*map.RowPitch,64);
  context->Unmap(staging.Get(),0);return data;
}
template<class Table>struct Native {
  Storage memory{VioGpuDxvkPrivateDeviceSize()};D3D10DDI_HDEVICE device{memory.bytes.get()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};Table f{};
  ComPtr<ID3D11Device> backend;ComPtr<ID3D11DeviceContext> context;ComPtr<ID3D11Resource> image;
  std::vector<Storage> shaders;std::vector<D3D10DDI_HSHADER> shaderHandles;
  Storage resourceMemory{1},viewMemory{1},rasterMemory{1},depthMemory{1},occlusionMemory{1},pipelineMemory{1};
  D3D10DDI_HRESOURCE resource{};D3D10DDI_HDEPTHSTENCILVIEW depth{};
  D3D10DDI_HRASTERIZERSTATE raster{};D3D10DDI_HDEPTHSTENCILSTATE depthState{};
  D3D10DDI_HQUERY occlusion{},pipeline{};
  Native() {
    core.pfnSetErrorCb=error;LUID luid{};HRESULT hr;
    if constexpr(std::is_same_v<Table,D3D10DDI_DEVICEFUNCS>)hr=VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core,&f);
    else hr=VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{},&core,&f);
    CHECK(hr==S_OK&&created);backend=created;created.Reset();backend->GetImmediateContext(&context);expect();
    D3D10DDI_MIPINFO mip{16,16,1,16,16,1};D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList=&mip;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;desc.Format=DXGI_FORMAT_R32_TYPELESS;
    desc.Usage=D3D10_DDI_USAGE_DEFAULT;desc.BindFlags=D3D10_DDI_BIND_DEPTH_STENCIL;
    desc.SampleDesc.Count=desc.ArraySize=desc.MipLevels=1;
    resourceMemory=Storage(f.pfnCalcPrivateResourceSize(device,&desc));resource={resourceMemory.bytes.get()};
    f.pfnCreateResource(device,&desc,resource,{});expect();
    D3D10DDIARG_CREATEDEPTHSTENCILVIEW vd{};vd.hDrvResource=resource;vd.Format=DXGI_FORMAT_D32_FLOAT;
    vd.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;vd.Tex2D.ArraySize=1;
    viewMemory=Storage(f.pfnCalcPrivateDepthStencilViewSize(device,&vd));depth={viewMemory.bytes.get()};
    f.pfnCreateDepthStencilView(device,&vd,depth,{});f.pfnSetRenderTargets(device,nullptr,0,0,depth);expect();
    ComPtr<ID3D11DepthStencilView> realized;ID3D11DepthStencilView* raw=nullptr;
    context->OMGetRenderTargets(0,nullptr,&raw);realized.Attach(raw);CHECK(realized);realized->GetResource(&image);
    D3D10_DDI_RASTERIZER_DESC rd{};rd.FillMode=D3D10_DDI_FILL_SOLID;rd.CullMode=D3D10_DDI_CULL_NONE;rd.DepthClipEnable=TRUE;
    rasterMemory=Storage(f.pfnCalcPrivateRasterizerStateSize(device,&rd));raster={rasterMemory.bytes.get()};
    f.pfnCreateRasterizerState(device,&rd,raster,{});f.pfnSetRasterizerState(device,raster);expect();
    D3D10_DDI_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D10_DDI_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D10_DDI_COMPARISON_ALWAYS;
    dd.StencilReadMask=dd.StencilWriteMask=0xff;
    dd.FrontFace={D3D10_DDI_STENCIL_OP_KEEP,D3D10_DDI_STENCIL_OP_KEEP,D3D10_DDI_STENCIL_OP_KEEP,D3D10_DDI_COMPARISON_ALWAYS};dd.BackFace=dd.FrontFace;
    depthMemory=Storage(f.pfnCalcPrivateDepthStencilStateSize(device,&dd));depthState={depthMemory.bytes.get()};
    f.pfnCreateDepthStencilState(device,&dd,depthState,{});f.pfnSetDepthStencilState(device,depthState,0);expect();
    auto query=[&](D3D10DDI_QUERY type,Storage& storage,D3D10DDI_HQUERY& out) {
      D3D10DDIARG_CREATEQUERY q{type,0};storage=Storage(f.pfnCalcPrivateQuerySize(device,&q));out={storage.bytes.get()};
      f.pfnCreateQuery(device,&q,out,{});expect();
    };
    query(D3D10DDI_QUERY_OCCLUSION,occlusionMemory,occlusion);
    query(D3D10DDI_QUERY_PIPELINESTATS,pipelineMemory,pipeline);
    f.pfnIaSetTopology(device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);expect();
  }
  D3D10DDI_HSHADER shader(const Program& p,bool vertex) {
    shaders.emplace_back(f.pfnCalcPrivateShaderSize(device,p.tokens.data(),&p.sig));D3D10DDI_HSHADER h{shaders.back().bytes.get()};
    shaderHandles.push_back(h);
    if(vertex)f.pfnCreateVertexShader(device,p.tokens.data(),h,{},&p.sig);
    else f.pfnCreatePixelShader(device,p.tokens.data(),h,{},&p.sig);expect();return h;
  }
  void queryData(D3D10DDI_HQUERY q,void* output,UINT bytes) {
    const auto deadline=GetTickCount64()+5000;
    do { lastError=S_OK;f.pfnQueryGetData(device,q,output,bytes,0);
      if(lastError!=DXGI_DDI_ERR_WASSTILLDRAWING){expect();return;}
      CHECK(GetTickCount64()<deadline);Sleep(1);
    }while(true);
  }
  std::vector<UINT> draw(UINT mode,D3D10DDI_HSHADER vertex,const std::array<D3D10DDI_HSHADER,3>& pixels,Counters& counters) {
    f.pfnClearDepthStencilView(device,depth,D3D10_DDI_CLEAR_DEPTH,1,0);
    f.pfnSetRenderTargets(device,nullptr,0,0,mode==5?D3D10DDI_HDEPTHSTENCILVIEW{}:depth);
    const D3D10_DDI_VIEWPORT viewport{0,0,16,16,0,1};
    f.pfnSetViewports(device,mode==4?0:1,mode==4?1:0,mode==4?nullptr:&viewport);
    f.pfnVsSetShader(device,vertex);f.pfnPsSetShader(device,mode>=1&&mode<=3?pixels[mode-1]:D3D10DDI_HSHADER{});expect();
    f.pfnQueryBegin(device,occlusion);f.pfnQueryBegin(device,pipeline);f.pfnDraw(device,3,0);expect();
    f.pfnQueryEnd(device,pipeline);f.pfnQueryEnd(device,occlusion);expect();
    D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS p{};queryData(pipeline,&p,sizeof(p));queryData(occlusion,&counters[8],sizeof(UINT64));
    counters[0]=p.IAVertices;counters[1]=p.IAPrimitives;counters[2]=p.VSInvocations;counters[3]=p.GSInvocations;
    counters[4]=p.GSPrimitives;counters[5]=p.CInvocations;counters[6]=p.CPrimitives;counters[7]=p.PSInvocations;
    const HRESULT removed=backend->GetDeviceRemovedReason();CHECK(removed==S_OK);counters[9]=UINT(removed);
    return readback11(backend.Get(),context.Get(),image.Get());
  }
  ~Native() {
    context->ClearState();for(auto h:shaderHandles){f.pfnDestroyShader(device,h);expect();}
    f.pfnDestroyQuery(device,pipeline);f.pfnDestroyQuery(device,occlusion);
    f.pfnDestroyDepthStencilState(device,depthState);f.pfnDestroyRasterizerState(device,raster);
    f.pfnDestroyDepthStencilView(device,depth);f.pfnDestroyResource(device,resource);expect();
    image.Reset();context.Reset();backend.Reset();f.pfnDestroyDevice(device);expect();
  }
};
struct Public {
  ComPtr<ID3D10Device1> device;ComPtr<ID3D10Texture2D> image,staging;ComPtr<ID3D10DepthStencilView> depth;
  ComPtr<ID3D10VertexShader> vertex;std::array<ComPtr<ID3D10PixelShader>,3> pixels;
  ComPtr<ID3D10RasterizerState> raster;ComPtr<ID3D10DepthStencilState> depthState;
  ComPtr<ID3D10Query> occlusion,pipeline;
  Public(bool model41,const Program& vs,const std::array<Program*,3>& ps) {
    const auto level=model41?D3D10_FEATURE_LEVEL_10_1:D3D10_FEATURE_LEVEL_10_0;
    CHECK(D3D10CreateDevice1(nullptr,D3D10_DRIVER_TYPE_WARP,nullptr,0,level,D3D10_1_SDK_VERSION,&device)==S_OK);
    CHECK(device->GetFeatureLevel()==level);
    CHECK(device->CreateVertexShader(vs.binary->GetBufferPointer(),vs.binary->GetBufferSize(),&vertex)==S_OK);
    for(UINT i=0;i<3;++i)CHECK(device->CreatePixelShader(ps[i]->binary->GetBufferPointer(),ps[i]->binary->GetBufferSize(),&pixels[i])==S_OK);
    D3D10_TEXTURE2D_DESC td{};td.Width=td.Height=16;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_R32_TYPELESS;td.Usage=D3D10_USAGE_DEFAULT;td.BindFlags=D3D10_BIND_DEPTH_STENCIL;
    CHECK(device->CreateTexture2D(&td,nullptr,&image)==S_OK);
    D3D10_DEPTH_STENCIL_VIEW_DESC vd{};vd.Format=DXGI_FORMAT_D32_FLOAT;vd.ViewDimension=D3D10_DSV_DIMENSION_TEXTURE2D;
    CHECK(device->CreateDepthStencilView(image.Get(),&vd,&depth)==S_OK);
    td.Usage=D3D10_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D10_CPU_ACCESS_READ;
    CHECK(device->CreateTexture2D(&td,nullptr,&staging)==S_OK);
    D3D10_RASTERIZER_DESC rd{};rd.FillMode=D3D10_FILL_SOLID;rd.CullMode=D3D10_CULL_NONE;rd.DepthClipEnable=TRUE;
    CHECK(device->CreateRasterizerState(&rd,&raster)==S_OK);device->RSSetState(raster.Get());
    D3D10_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D10_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D10_COMPARISON_ALWAYS;
    dd.StencilReadMask=dd.StencilWriteMask=0xff;
    dd.FrontFace={D3D10_STENCIL_OP_KEEP,D3D10_STENCIL_OP_KEEP,D3D10_STENCIL_OP_KEEP,D3D10_COMPARISON_ALWAYS};dd.BackFace=dd.FrontFace;
    CHECK(device->CreateDepthStencilState(&dd,&depthState)==S_OK);device->OMSetDepthStencilState(depthState.Get(),0);
    D3D10_QUERY_DESC q{D3D10_QUERY_OCCLUSION,0};CHECK(device->CreateQuery(&q,&occlusion)==S_OK);
    q.Query=D3D10_QUERY_PIPELINE_STATISTICS;CHECK(device->CreateQuery(&q,&pipeline)==S_OK);
    device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);device->VSSetShader(vertex.Get());
  }
  void queryData(ID3D10Query* query,void* output,UINT bytes) {
    const auto deadline=GetTickCount64()+5000;
    do { const HRESULT hr=query->GetData(output,bytes,0);if(hr==S_OK)return;CHECK(hr==S_FALSE&&GetTickCount64()<deadline);Sleep(1); }while(true);
  }
  std::vector<UINT> draw(UINT mode,Counters& counters) {
    device->ClearDepthStencilView(depth.Get(),D3D10_CLEAR_DEPTH,1,0);
    device->OMSetRenderTargets(0,nullptr,mode==5?nullptr:depth.Get());
    const D3D10_VIEWPORT viewport{0,0,16,16,0,1};device->RSSetViewports(mode==4?0:1,mode==4?nullptr:&viewport);
    device->PSSetShader(mode>=1&&mode<=3?pixels[mode-1].Get():nullptr);
    occlusion->Begin();pipeline->Begin();device->Draw(3,0);pipeline->End();occlusion->End();
    D3D10_QUERY_DATA_PIPELINE_STATISTICS p{};queryData(pipeline.Get(),&p,sizeof(p));queryData(occlusion.Get(),&counters[8],sizeof(UINT64));
    counters[0]=p.IAVertices;counters[1]=p.IAPrimitives;counters[2]=p.VSInvocations;counters[3]=p.GSInvocations;
    counters[4]=p.GSPrimitives;counters[5]=p.CInvocations;counters[6]=p.CPrimitives;counters[7]=p.PSInvocations;
    const HRESULT removed=device->GetDeviceRemovedReason();CHECK(removed==S_OK);counters[9]=UINT(removed);
    device->CopyResource(staging.Get(),image.Get());D3D10_MAPPED_TEXTURE2D map{};
    CHECK(staging->Map(0,D3D10_MAP_READ,0,&map)==S_OK&&map.pData&&map.RowPitch>=64);
    std::vector<UINT> data(256);for(UINT y=0;y<16;++y)std::memcpy(data.data()+y*16,static_cast<const char*>(map.pData)+size_t(y)*map.RowPitch,64);
    staging->Unmap(0);return data;
  }
  ~Public(){device->ClearState();}
};
template<class Table>static void model(bool model41) {
  const char* vertexProfile=model41?"vs_4_1":"vs_4_0";const char* pixelProfile=model41?"ps_4_1":"ps_4_0";
  Program vs("vs",vertexProfile),depth("ps_depth",pixelProfile),empty("ps_empty",pixelProfile),discard("ps_discard",pixelProfile);
  Native<Table> native;auto vertex=native.shader(vs,true);
  std::array<D3D10DDI_HSHADER,3> pixels={native.shader(depth,false),native.shader(empty,false),native.shader(discard,false)};
  Public reference(model41,vs,{&depth,&empty,&discard});
  for(UINT mode=0;mode<6;++mode) {
    Counters nativeCounters{},publicCounters{};const auto actual=native.draw(mode,vertex,pixels,nativeCounters);
    const auto original=reference.draw(mode,publicCounters);CHECK(actual==original);
    retainScene(model41?41:40,mode,"native",actual,nativeCounters);
    retainScene(model41?41:40,mode,"public",original,publicCounters);++scenes;
  }
}
int main() {
  caller=GetCurrentThreadId();retain("depth-original.hlsl",source,sizeof(source)-1);
  model<D3D10DDI_DEVICEFUNCS>(false);model<D3D10_1DDI_DEVICEFUNCS>(true);
  CHECK(programs==8&&frames==24&&scenes==12);
  std::printf("D3D10 depth PASS checks=%u scenes=12 pixels=3072 words=3072 original_frames=24 queries=24 fxc_programs=8 hardware_admission=0\n",checks);
}
