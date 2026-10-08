// SPDX-License-Identifier: MIT
// Genuine typed 10/10.1 shader DDIs; WARP substitutes only the private factory.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_contract.h"
#include "../src/umd/umd_result.h"
#include "umd-probe-shaders.h"
#include <array>
#include <cstdlib>
#include <memory>
#include <type_traits>
using Microsoft::WRL::ComPtr;
using dxvk::umd::ShaderStage;
static unsigned checks, programs, frames;
static UINT frontFaces[2];
static HRESULT lastError = S_OK;
static DWORD caller;
static ComPtr<ID3D11Device> created;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"D3D10 system shader line %d: %s hr=%08lx\n",__LINE__,#x,static_cast<unsigned long>(lastError)); std::abort(); } } while (0)
static void APIENTRY error(D3D10DDI_HRTCORELAYER, HRESULT hr) {
  CHECK(GetCurrentThreadId()==caller && FAILED(hr)); lastError=hr;
}
static void expect(HRESULT hr=S_OK) { CHECK(lastError==dxvk::umd::ddiResult(hr)); lastError=S_OK; }
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL logical,
    ID3D11Device** device, ID3D11DeviceContext** context,const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && (logical==D3D_FEATURE_LEVEL_10_0||logical==D3D_FEATURE_LEVEL_10_1));
  const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
  const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (hr==S_OK) created=*device; return hr;
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
struct V { float4 p:SV_Position; float3 clip:SV_ClipDistance0; float cull:SV_CullDistance0; nointerpolation uint v:DATA0; };
V vs(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
  V o;float2 xy=float2((vertex<<1)&2,vertex&2);
  o.p=float4(xy*float2(2,-2)+float2(-1,1),0,1);
  float left=-1+0.5*instance;o.clip=float3(o.p.x,o.p.x-left,left+0.5-o.p.x);o.cull=1;
  o.v=0x11223340+instance;return o;
}
struct G { float4 p:SV_Position;float3 clip:SV_ClipDistance0;float cull:SV_CullDistance0;nointerpolation uint v:DATA0;nointerpolation uint id:SV_PrimitiveID; };
[maxvertexcount(3)] void gs(triangle V input[3],uint primitive:SV_PrimitiveID,inout TriangleStream<G> stream) {
  [unroll]for(uint i=0;i<3;++i){G o;o.p=input[i].p;o.clip=input[i].clip;o.cull=input[i].cull;o.v=input[i].v;o.id=primitive+37;stream.Append(o);}stream.RestartStrip();
}
struct P { uint4 u:SV_Target0;int4 s:SV_Target1;float depth:SV_Depth; };
P ps(G input,bool front:SV_IsFrontFace) {
  P o;o.u=uint4(input.v,input.id+7,front?1:0,0x7fc01234);
  o.s=int4(-int(input.v),-7,99,-1);o.depth=0.25;return o;
}
float4 ps_simple():SV_Target{return float4(1,0,0,1);}
)";
struct Program {
  ShaderStage stage;
  std::vector<UINT> tokens;
  ComPtr<ID3DBlob> binary;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs,outputs;
  D3D10DDIARG_STAGE_IO_SIGNATURES sig{};
  Program(ShaderStage value,const char* entry,const char* profile):stage(value){
    CHECK(compileHlslTokens(source,entry,profile,tokens,&binary));
    char name[128];CHECK(std::snprintf(name,sizeof(name),"system-fxc-%02u-%s-%s.dxbc",++programs,entry,profile)>0);
    retain(name,binary->GetBufferPointer(),binary->GetBufferSize());
    CHECK(std::snprintf(name,sizeof(name),"system-fxc-%02u-%s-%s.tokens",programs,entry,profile)>0);
    retain(name,tokens.data(),tokens.size()*4);
    ComPtr<ID3D11ShaderReflection> reflection;
    CHECK(D3DReflect(binary->GetBufferPointer(),binary->GetBufferSize(),__uuidof(ID3D11ShaderReflection),&reflection)==S_OK);
    D3D11_SHADER_DESC desc{};CHECK(reflection->GetDesc(&desc)==S_OK);
    for(bool input:{true,false})for(UINT i=0;i<(input?desc.InputParameters:desc.OutputParameters);++i){
      D3D11_SIGNATURE_PARAMETER_DESC p{};CHECK((input?reflection->GetInputParameterDesc(i,&p):reflection->GetOutputParameterDesc(i,&p))==S_OK);
      // Dedicated depth/coverage operands are encoded in instructions, not
      // register-file rows in the historical runtime union.
      if(p.Register==UINT(-1))continue;
      if(p.SystemValueType==D3D_NAME_PRIMITIVE_ID)
        CHECK(p.ComponentType==D3D_REGISTER_COMPONENT_UINT32&&p.Mask&&(p.Mask&(p.Mask-1))==0);
      const auto system=p.SystemValueType==D3D_NAME_TARGET?D3D_NAME_UNDEFINED:p.SystemValueType;
      CHECK(UINT(system)<=10);(input?inputs:outputs).push_back({D3D10_SB_NAME(system),p.Register,p.Mask});
    }
    sig={inputs.data(),UINT(inputs.size()),outputs.data(),UINT(outputs.size())};
  }
};
template<class Table>struct Fixture {
  Storage memory{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{memory.bytes.get()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};Table f{};
  ComPtr<ID3D11Device> backend;ComPtr<ID3D11DeviceContext> context;
  std::vector<Storage> resources,views,shaders;
  std::vector<D3D10DDI_HRESOURCE> resourceHandles;
  std::vector<D3D10DDI_HRENDERTARGETVIEW> targetHandles;
  std::vector<D3D10DDI_HSHADER> shaderHandles;
  std::vector<D3D10DDI_HDEPTHSTENCILVIEW> depthHandles;
  Fixture(){core.pfnSetErrorCb=error;LUID luid{};HRESULT hr;
    if constexpr(std::is_same_v<Table,D3D10DDI_DEVICEFUNCS>)hr=VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core,&f);
    else hr=VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{},&core,&f);
    CHECK(hr==S_OK&&created);backend=created;created.Reset();backend->GetImmediateContext(&context);expect();}
  D3D10DDI_HSHADER shader(const Program& program){
    shaders.emplace_back(f.pfnCalcPrivateShaderSize(device,program.tokens.data(),&program.sig));
    D3D10DDI_HSHADER h{shaders.back().bytes.get()};shaderHandles.push_back(h);
    if(program.stage==ShaderStage::Vertex)f.pfnCreateVertexShader(device,program.tokens.data(),h,{},&program.sig);
    else if(program.stage==ShaderStage::Geometry)f.pfnCreateGeometryShader(device,program.tokens.data(),h,{},&program.sig);
    else f.pfnCreatePixelShader(device,program.tokens.data(),h,{},&program.sig);
    expect();return h;
  }
  D3D10DDI_HRESOURCE resource(DXGI_FORMAT format,UINT bind){
    D3D10DDI_MIPINFO mip{16,16,1,16,16,1};D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList=&mip;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;desc.Format=format;
    desc.Usage=D3D10_DDI_USAGE_DEFAULT;desc.BindFlags=bind;desc.SampleDesc.Count=desc.ArraySize=desc.MipLevels=1;
    resources.emplace_back(f.pfnCalcPrivateResourceSize(device,&desc));D3D10DDI_HRESOURCE h{resources.back().bytes.get()};
    resourceHandles.push_back(h);f.pfnCreateResource(device,&desc,h,{});expect();return h;
  }
  D3D10DDI_HRENDERTARGETVIEW target(DXGI_FORMAT format){
    D3D10DDIARG_CREATERENDERTARGETVIEW desc{};desc.hDrvResource=resource(format,D3D10_DDI_BIND_RENDER_TARGET);
    desc.Format=format;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;desc.Tex2D.ArraySize=1;
    views.emplace_back(f.pfnCalcPrivateRenderTargetViewSize(device,&desc));D3D10DDI_HRENDERTARGETVIEW h{views.back().bytes.get()};
    targetHandles.push_back(h);f.pfnCreateRenderTargetView(device,&desc,h,{});expect();return h;
  }
  D3D10DDI_HDEPTHSTENCILVIEW depth(){
    D3D10DDIARG_CREATEDEPTHSTENCILVIEW desc{};desc.hDrvResource=resource(DXGI_FORMAT_R32_TYPELESS,D3D10_DDI_BIND_DEPTH_STENCIL);
    desc.Format=DXGI_FORMAT_D32_FLOAT;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;desc.Tex2D.ArraySize=1;
    views.emplace_back(f.pfnCalcPrivateDepthStencilViewSize(device,&desc));D3D10DDI_HDEPTHSTENCILVIEW h{views.back().bytes.get()};
    depthHandles.push_back(h);f.pfnCreateDepthStencilView(device,&desc,h,{});expect();return h;
  }
  ~Fixture(){context->ClearState();for(auto h:shaderHandles){f.pfnDestroyShader(device,h);expect();}
    for(auto h:depthHandles){f.pfnDestroyDepthStencilView(device,h);expect();}
    for(auto h:targetHandles){f.pfnDestroyRenderTargetView(device,h);expect();}
    for(auto h:resourceHandles){f.pfnDestroyResource(device,h);expect();}
    context.Reset();backend.Reset();f.pfnDestroyDevice(device);expect();}
};
static std::vector<UINT> readback(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Resource* resource,UINT words){
  ComPtr<ID3D11Texture2D> texture;CHECK(resource->QueryInterface(IID_PPV_ARGS(&texture))==S_OK);
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=desc.MiscFlags=0;
  desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> staging;CHECK(device->CreateTexture2D(&desc,nullptr,&staging)==S_OK);
  context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};
  CHECK(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)==S_OK&&map.pData&&map.RowPitch>=16*words*4);
  std::vector<UINT> data(16*16*words);for(UINT y=0;y<16;++y)std::memcpy(data.data()+y*16*words,static_cast<const char*>(map.pData)+size_t(y)*map.RowPitch,16*words*4);
  context->Unmap(staging.Get(),0);return data;
}
template<class Table>static void scene(bool geometry,bool model41){
  Fixture<Table> f;Program vs(ShaderStage::Vertex,"vs",model41?"vs_4_1":"vs_4_0");
  Program gs(ShaderStage::Geometry,"gs",model41?"gs_4_1":"gs_4_0");
  Program ps(ShaderStage::Pixel,"ps",model41?"ps_4_1":"ps_4_0");
  if(geometry){
    // This controlled fixture uses one explicit constant GS/PS interface.
    // Require the original FXC containers to agree before testing DDI linkage.
    auto primitive=[](const std::vector<D3D10DDIARG_SIGNATURE_ENTRY>& entries){
      const D3D10DDIARG_SIGNATURE_ENTRY* found=nullptr;
      for(const auto& entry:entries)if(entry.SystemValue==D3D10_SB_NAME_PRIMITIVE_ID){CHECK(!found);found=&entry;}
      CHECK(found);return found;
    };
    const auto output=primitive(gs.outputs),input=primitive(ps.inputs);
    CHECK(output->Register==input->Register&&output->Mask==input->Mask);
  }
  auto vertex=f.shader(vs),geom=f.shader(gs),pixel=f.shader(ps);
  D3D10DDI_HRENDERTARGETVIEW targets[]={f.target(DXGI_FORMAT_R32G32B32A32_UINT),f.target(DXGI_FORMAT_R32G32B32A32_SINT)};
  auto depth=f.depth();f.f.pfnSetRenderTargets(f.device,targets,2,0,depth);expect();
  ComPtr<ID3D11RenderTargetView> nativeTargets[2];ComPtr<ID3D11DepthStencilView> nativeDepth;
  ID3D11RenderTargetView* fetchedTargets[2]{};ID3D11DepthStencilView* fetchedDepth=nullptr;
  f.context->OMGetRenderTargets(2,fetchedTargets,&fetchedDepth);
  for(UINT i=0;i<2;++i)nativeTargets[i].Attach(fetchedTargets[i]);nativeDepth.Attach(fetchedDepth);
  CHECK(nativeTargets[0]&&nativeTargets[1]&&nativeDepth);
  ComPtr<ID3D11Resource> images[3];nativeTargets[0]->GetResource(&images[0]);nativeTargets[1]->GetResource(&images[1]);nativeDepth->GetResource(&images[2]);
  D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
  raster.FrontCounterClockwise=model41;
  ComPtr<ID3D11RasterizerState> rasterState;CHECK(f.backend->CreateRasterizerState(&raster,&rasterState)==S_OK);
  D3D11_DEPTH_STENCIL_DESC depthState{};depthState.DepthEnable=TRUE;depthState.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depthState.DepthFunc=D3D11_COMPARISON_ALWAYS;
  ComPtr<ID3D11DepthStencilState> depthObject;CHECK(f.backend->CreateDepthStencilState(&depthState,&depthObject)==S_OK);
  f.context->RSSetState(rasterState.Get());f.context->OMSetDepthStencilState(depthObject.Get(),0);
  const D3D10_DDI_VIEWPORT viewport{0,0,16,16,0,1};f.f.pfnSetViewports(f.device,1,0,&viewport);
  f.f.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);expect();
  FLOAT clear[]={0,0,0,0};for(auto target:targets)f.f.pfnClearRenderTargetView(f.device,target,clear);
  f.f.pfnClearDepthStencilView(f.device,depth,D3D10_DDI_CLEAR_DEPTH,1,0);expect();
  f.f.pfnVsSetShader(f.device,vertex);f.f.pfnPsSetShader(f.device,pixel);f.f.pfnGsSetShader(f.device,geometry?geom:D3D10DDI_HSHADER{});expect();
  f.f.pfnDrawInstanced(f.device,3,4,0,0);expect();
  const auto unsignedData=readback(f.backend.Get(),f.context.Get(),images[0].Get(),4);
  const auto signedData=readback(f.backend.Get(),f.context.Get(),images[1].Get(),4);
  const auto depthData=readback(f.backend.Get(),f.context.Get(),images[2].Get(),1);
  const UINT frontFace=unsignedData[8*4+2];CHECK(frontFace<=1);
  if(model41)CHECK(frontFace!=frontFaces[unsigned(geometry)]);else frontFaces[unsigned(geometry)]=frontFace;
  // Compile original FXC containers through the public API for an independent
  // signature/control path on the same renderer, then compare every byte.
  ComPtr<ID3D11VertexShader> referenceVs;ComPtr<ID3D11GeometryShader> referenceGs;ComPtr<ID3D11PixelShader> referencePs;
  CHECK(f.backend->CreateVertexShader(vs.binary->GetBufferPointer(),vs.binary->GetBufferSize(),nullptr,&referenceVs)==S_OK);
  CHECK(f.backend->CreatePixelShader(ps.binary->GetBufferPointer(),ps.binary->GetBufferSize(),nullptr,&referencePs)==S_OK);
  if(geometry)CHECK(f.backend->CreateGeometryShader(gs.binary->GetBufferPointer(),gs.binary->GetBufferSize(),nullptr,&referenceGs)==S_OK);
  ID3D11RenderTargetView* rawTargets[]={nativeTargets[0].Get(),nativeTargets[1].Get()};
  f.context->OMSetRenderTargets(2,rawTargets,nativeDepth.Get());
  for(auto target:rawTargets)f.context->ClearRenderTargetView(target,clear);f.context->ClearDepthStencilView(nativeDepth.Get(),D3D11_CLEAR_DEPTH,1,0);
  f.context->VSSetShader(referenceVs.Get(),nullptr,0);f.context->GSSetShader(referenceGs.Get(),nullptr,0);f.context->PSSetShader(referencePs.Get(),nullptr,0);
  f.context->DrawInstanced(3,4,0,0);
  const std::vector<UINT> originals[]={unsignedData,signedData,depthData};
  const char* names[]={"uint","sint","depth"};
  for(UINT image=0;image<3;++image){const auto reference=readback(f.backend.Get(),f.context.Get(),images[image].Get(),image==2?1:4);
    CHECK(reference==originals[image]);char name[128];
    CHECK(std::snprintf(name,sizeof(name),"system-model%u-gs%u-%s-native.bin",model41?41:40,unsigned(geometry),names[image])>0);retain(name,originals[image].data(),originals[image].size()*4);
    CHECK(std::snprintf(name,sizeof(name),"system-model%u-gs%u-%s-public.bin",model41?41:40,unsigned(geometry),names[image])>0);retain(name,reference.data(),reference.size()*4);++frames;
  }
  for(UINT y=0;y<16;++y)for(UINT x=0;x<16;++x){const size_t p=y*16+x;
    if(x<8){CHECK(unsignedData[p*4]==0&&signedData[p*4]==0&&depthData[p]==0x3f800000);}
    else {const UINT instance=x/4;CHECK(unsignedData[p*4]==0x11223340+instance&&unsignedData[p*4+1]==(geometry?44u:7u)
      &&unsignedData[p*4+2]==frontFace&&unsignedData[p*4+3]==0x7fc01234);
      CHECK(signedData[p*4]==UINT(-int(0x11223340+instance))&&signedData[p*4+1]==UINT(-7)
        &&signedData[p*4+2]==99&&signedData[p*4+3]==UINT(-1)&&depthData[p]==0x3e800000);}}
  ComPtr<ID3D11VertexShader> before;f.context->VSGetShader(&before,nullptr,nullptr);CHECK(before);
  for(UINT invalid=0;invalid<7;++invalid){
    auto tokens=vs.tokens;auto signature=vs.sig;
    if(invalid==0)tokens[0]=(UINT(ShaderStage::Vertex)<<16)|0x50;
    else if(invalid==1)tokens[0]=0x40;
    else if(invalid==2){bool replaced=false;for(size_t offset=2;offset<tokens.size();){
      const UINT op=tokens[offset]&0x7ff,count=(tokens[offset]>>24)&0x7f;CHECK(count&&count<=tokens.size()-offset);
      if(op==96&&tokens[offset+count-1]==8){tokens[offset+count-1]=7;replaced=true;break;}offset+=count;}CHECK(replaced);}
    else if(invalid==3){tokens[2]=(tokens[2]&~0x7ffu)|107u;}
    else if(invalid==4)signature.NumInputSignatureEntries=33;
    else if(invalid==5){signature.NumInputSignatureEntries=1;signature.pInputSignature=nullptr;}
    else {signature.NumOutputSignatureEntries=1;signature.pOutputSignature=nullptr;}
    const SIZE_T bytes=f.f.pfnCalcPrivateShaderSize(f.device,tokens.data(),&signature);Storage failed(bytes+16);
    std::memset(static_cast<char*>(failed.bytes.get())+bytes,0x6b,16);D3D10DDI_HSHADER rejected{failed.bytes.get()};
    f.f.pfnCreateVertexShader(f.device,tokens.data(),rejected,{},&signature);expect(E_INVALIDARG);
    ComPtr<ID3D11VertexShader> after;f.context->VSGetShader(&after,nullptr,nullptr);CHECK(after.Get()==before.Get());
    f.f.pfnDestroyShader(f.device,rejected);expect();
    for(UINT i=0;i<16;++i)CHECK(static_cast<const unsigned char*>(failed.bytes.get())[bytes+i]==0x6b);
  }
  std::printf("D3D10_SYSTEM_SHADER_SCENE model=%u gs=%u pixels=256 words=2304 public_equal=1 instances=4 clip_half_and_quarters=1 depth=0.25 front_ccw=%u front_face=%u\n",model41?41:40,unsigned(geometry),unsigned(model41),frontFace);
}
int main(){caller=GetCurrentThreadId();std::setvbuf(stdout,nullptr,_IONBF,0);retain("system-shaders-original.hlsl",source,sizeof(source)-1);
  CHECK(dxvk::umd::runtimeMissingD3D10Requirements()==0x17f&&dxvk::umd::runtimeMissingD3D10_1Requirements()==0x37f);
  scene<D3D10DDI_DEVICEFUNCS>(false,false);scene<D3D10DDI_DEVICEFUNCS>(true,false);
  scene<D3D10_1DDI_DEVICEFUNCS>(false,true);scene<D3D10_1DDI_DEVICEFUNCS>(true,true);
  CHECK(programs==12&&frames==12);
  std::printf("D3D10 system shader PASS checks=%u scenes=4 pixels=1024 words=9216 original_frames=24 fxc_programs=12 hardware_admission=0\n",checks);
}
