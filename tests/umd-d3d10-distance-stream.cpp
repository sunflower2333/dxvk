// SPDX-License-Identifier: MIT
// Clip/cull stream output against original FXC and the public D3D10.1 runtime.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_stream_output.h"
#include "../src/umd/umd_result.h"
#include "umd-probe-shaders.h"
#include <d3d10_1.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <type_traits>
using Microsoft::WRL::ComPtr;
static unsigned checks, programs, buffers, scenes;
static HRESULT lastError=S_OK;
static DWORD caller;
static ComPtr<ID3D11Device> created;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"D3D10 distance SO line %d: %s hr=%08lx\n",__LINE__,#x,static_cast<unsigned long>(lastError)); std::abort(); } } while (0)
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
struct V {
  float4 p:SV_Position; float4 clip0:SV_ClipDistance0;
  float2 clip1:SV_ClipDistance1; float2 cull:SV_CullDistance0;
  uint data:DATA0;
};
V make(uint id,uint alternate) {
  V o;o.p=float4(float(id),0,0,1);o.clip0=float4(-1,-2,-3,-4);
  // IDs 0..2 retain negative zero; the input-dependent bits prevent FXC constant folding.
  o.clip1=float2(asfloat(0x80000000u | (id & 0xfffffffcu)),float(id)+0.5+8*alternate);
  o.cull=float2(-0.25-float(id)-8*alternate,2+float(id)+8*alternate);
  o.data=0x7fc01234+8*alternate+id;return o;
}
V vs(uint id:SV_VertexID) { return make(id,0); }
V vs_alternate(uint id:SV_VertexID) { return make(id,1); }
[maxvertexcount(3)] void gs(triangle V input[3],inout TriangleStream<V> stream) {
  [unroll]for(uint i=0;i<3;++i){V o=input[i];o.clip1.y+=16;o.cull.x-=16;o.cull.y+=32;o.data^=0x01000000;stream.Append(o);}stream.RestartStrip();
}
)";
struct Program {
  std::vector<UINT> tokens;
  ComPtr<ID3DBlob> binary;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs,outputs;
  D3D10DDIARG_STAGE_IO_SIGNATURES sig{};
  Program(const char* entry,const char* profile) {
    CHECK(compileHlslTokens(source,entry,profile,tokens,&binary));
    char name[128];CHECK(std::snprintf(name,sizeof(name),"distance-fxc-%02u-%s-%s.dxbc",++programs,entry,profile)>0);
    retain(name,binary->GetBufferPointer(),binary->GetBufferSize());
    CHECK(std::snprintf(name,sizeof(name),"distance-fxc-%02u-%s-%s.tokens",programs,entry,profile)>0);
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
using Words=std::array<UINT,64>;
using Counters=std::array<UINT64,6>;
static UINT floatBits(float value){UINT bits;std::memcpy(&bits,&value,4);return bits;}
static Words expected(bool geometry,bool sparse,UINT alternate) {
  Words result;result.fill(0xcccccccc);
  for(UINT id=0;id<3;++id) {
    const std::array<UINT,5> values={0x80000000,floatBits(float(id)+0.5f+8*alternate+(geometry?16:0)),
      floatBits(-0.25f-float(id)-8*alternate-(geometry?16:0)),floatBits(2+float(id)+8*alternate+(geometry?32:0)),
      (0x7fc01234+8*alternate+id)^(geometry?0x01000000:0)};
    UINT word=id*8+1;for(UINT component=0;component<4;++component)if(!sparse||component%2==0)result[word++]=values[component];
    result[word]=values[4];
  }
  return result;
}
static void retainScene(UINT model,bool geometry,bool sparse,UINT alternate,const char* role,const Words& data,const Counters& counters) {
  char name[128];CHECK(std::snprintf(name,sizeof(name),"distance-model%u-gs%u-sparse%u-alternate%u-%s.bin",model,UINT(geometry),UINT(sparse),alternate,role)>0);
  retain(name,data.data(),sizeof(data));
  CHECK(std::snprintf(name,sizeof(name),"distance-model%u-gs%u-sparse%u-alternate%u-%s-query.bin",model,UINT(geometry),UINT(sparse),alternate,role)>0);
  retain(name,counters.data(),sizeof(counters));++buffers;
}
static void checkScene(UINT model,bool geometry,bool sparse,UINT alternate,const char* role,const Words& data,const Counters& counters) {
  const auto oracle=expected(geometry,sparse,alternate);
  for(UINT word=0;word<data.size();++word)if(data[word]!=oracle[word])
    std::fprintf(stderr,"D3D10 distance SO model=%u gs=%u sparse=%u alternate=%u role=%s word=%u actual=%08x expected=%08x\n",
      model,UINT(geometry),UINT(sparse),alternate,role,word,data[word],oracle[word]);
  CHECK(data==oracle);
  CHECK(counters[0]==3&&counters[1]==1&&counters[2]==0&&counters[3]==0&&counters[4]==1&&counters[5]==1);
}
static std::array<D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY,3> declaration(const Program& p,bool sparse) {
  const D3D10DDIARG_SIGNATURE_ENTRY *clip0=nullptr,*clip1=nullptr,*cull=nullptr,*data=nullptr;
  for(const auto& entry:p.outputs) {
    if(entry.SystemValue==D3D10_SB_NAME_CLIP_DISTANCE){if(entry.Mask==15)clip0=&entry;else clip1=&entry;}
    else if(entry.SystemValue==D3D10_SB_NAME_CULL_DISTANCE)cull=&entry;
    else if(entry.SystemValue==D3D10_SB_NAME_UNDEFINED)data=&entry;
  }
  CHECK(clip0&&clip1&&cull&&data&&clip0->Register<clip1->Register);
  CHECK(clip1->Register==cull->Register&&clip1->Mask==3&&cull->Mask==12&&data->Mask==1);
  return {{{0,D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP,1},{0,clip1->Register,BYTE(sparse?5:15)},{0,data->Register,1}}};
}
static std::array<D3D10_SO_DECLARATION_ENTRY,4> publicDeclaration(bool sparse) {
  return {{{nullptr,0,0,1,0},{"SV_ClipDistance",1,0,BYTE(sparse?1:2),0},
    {"SV_CullDistance",0,0,BYTE(sparse?1:2),0},{"DATA",0,0,1,0}}};
}
static void policy() {
  D3D10DDIARG_SIGNATURE_ENTRY entries[]={{D3D10_SB_NAME_CLIP_DISTANCE,1,15},{D3D10_SB_NAME_POSITION,0,15},
    {D3D10_SB_NAME_CULL_DISTANCE,2,12},{D3D10_SB_NAME_CLIP_DISTANCE,2,3},{D3D10_SB_NAME_UNDEFINED,3,1}};
  D3D10DDIARG_STAGE_IO_SIGNATURES signature{nullptr,0,entries,5};std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
  CHECK(dxvk::umd::streamOutputPassthroughSignature(signature,outputs)&&outputs.size()==5);
  CHECK(outputs[2].scalar==dxvk::umd::ShaderScalar::Float32&&outputs[4].scalar==dxvk::umd::ShaderScalar::Uint32);
  D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY entry{0,2,15};D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT so{nullptr,&entry,1,16};
  struct dxvk::umd::StreamOutput out;CHECK(dxvk::umd::streamOutputDeclaration(so,signature,out)&&out.entries.size()==2);
  CHECK(!std::strcmp(out.entries[0].SemanticName,"SV_ClipDistance")&&out.entries[0].SemanticIndex==1&&out.entries[0].StartComponent==0&&out.entries[0].ComponentCount==2);
  CHECK(!std::strcmp(out.entries[1].SemanticName,"SV_CullDistance")&&out.entries[1].SemanticIndex==0&&out.entries[1].StartComponent==0&&out.entries[1].ComponentCount==2);
  for(UINT mode=0;mode<5;++mode) {
    auto bad=signature;auto changed=std::array<D3D10DDIARG_SIGNATURE_ENTRY,5>{entries[0],entries[1],entries[2],entries[3],entries[4]};bad.pOutputSignature=changed.data();
    if(mode==0)changed[2].Mask=14; // Overlap.
    if(mode==1)changed[2].Mask=0;
    if(mode==2)changed[2].SystemValue=D3D10_SB_NAME_VERTEX_ID;
    if(mode==3)changed[0].Register=32;
    if(mode==4)changed[4]={D3D10_SB_NAME_CULL_DISTANCE,3,1}; // Nine distances/three registers.
    CHECK(!dxvk::umd::streamOutputPassthroughSignature(bad,outputs)&&outputs.empty());
  }
  entries[2].Mask=14;CHECK(!dxvk::umd::streamOutputDeclaration(so,signature,out)&&out.entries.empty());
}
template<class Table>struct Native {
  Storage memory{VioGpuDxvkPrivateDeviceSize()};D3D10DDI_HDEVICE device{memory.bytes.get()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};Table f{};ComPtr<ID3D11Device> backend;
  std::vector<Storage> resources,shaders,queries;std::vector<D3D10DDI_HRESOURCE> resourceHandles;
  std::vector<D3D10DDI_HSHADER> shaderHandles;std::vector<D3D10DDI_HQUERY> queryHandles;
  D3D10DDI_HRESOURCE output{},staging{};D3D10DDI_HQUERY statistics{},pipeline{};
  Native(){core.pfnSetErrorCb=error;LUID luid{};HRESULT hr;
    if constexpr(std::is_same_v<Table,D3D10DDI_DEVICEFUNCS>)hr=VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core,&f);
    else hr=VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{},&core,&f);
    CHECK(hr==S_OK&&created);backend=created;created.Reset();expect();
    auto buffer=[&](bool stage){D3D10DDI_MIPINFO mip{256,1,1,256,1,1};Words clear;clear.fill(0xcccccccc);
      D3D10_DDIARG_SUBRESOURCE_UP initial{clear.data(),256,256};D3D10DDIARG_CREATERESOURCE d{};
      d.pMipInfoList=&mip;d.pInitialDataUP=&initial;d.ResourceDimension=D3D10DDIRESOURCE_BUFFER;
      d.Usage=stage?D3D10_DDI_USAGE_STAGING:D3D10_DDI_USAGE_DEFAULT;d.BindFlags=stage?0:D3D10_DDI_BIND_STREAM_OUTPUT;
      d.MapFlags=stage?D3D10_DDI_CPU_ACCESS_READ:0;d.SampleDesc.Count=d.MipLevels=d.ArraySize=1;
      resources.emplace_back(f.pfnCalcPrivateResourceSize(device,&d));D3D10DDI_HRESOURCE h{resources.back().bytes.get()};resourceHandles.push_back(h);
      f.pfnCreateResource(device,&d,h,{});expect();return h;};
    output=buffer(false);staging=buffer(true);
    auto query=[&](D3D10DDI_QUERY type){D3D10DDIARG_CREATEQUERY q{type,0};queries.emplace_back(f.pfnCalcPrivateQuerySize(device,&q));
      D3D10DDI_HQUERY h{queries.back().bytes.get()};queryHandles.push_back(h);f.pfnCreateQuery(device,&q,h,{});expect();return h;};
    statistics=query(D3D10DDI_QUERY_STREAMOUTPUTSTATS);pipeline=query(D3D10DDI_QUERY_PIPELINESTATS);
    f.pfnIaSetTopology(device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);expect();
  }
  D3D10DDI_HSHADER vertex(const Program& p){shaders.emplace_back(f.pfnCalcPrivateShaderSize(device,p.tokens.data(),&p.sig));
    D3D10DDI_HSHADER h{shaders.back().bytes.get()};shaderHandles.push_back(h);f.pfnCreateVertexShader(device,p.tokens.data(),h,{},&p.sig);expect();return h;}
  D3D10DDI_HSHADER stream(const Program& p,bool geometry,bool sparse){auto entries=declaration(p,sparse);
    D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT so{geometry?p.tokens.data():nullptr,entries.data(),UINT(entries.size()),32};
    shaders.emplace_back(f.pfnCalcPrivateGeometryShaderWithStreamOutput(device,&so,&p.sig));D3D10DDI_HSHADER h{shaders.back().bytes.get()};
    shaderHandles.push_back(h);f.pfnCreateGeometryShaderWithStreamOutput(device,&so,h,{},&p.sig);expect();return h;}
  void queryData(D3D10DDI_HQUERY q,void* data,UINT size){const auto deadline=GetTickCount64()+5000;
    do{lastError=S_OK;f.pfnQueryGetData(device,q,data,size,0);if(lastError!=DXGI_DDI_ERR_WASSTILLDRAWING){expect();return;}
      CHECK(GetTickCount64()<deadline);Sleep(1);}while(true);}
  Words draw(D3D10DDI_HSHADER vertex,D3D10DDI_HSHADER stream,Counters& counters){f.pfnVsSetShader(device,vertex);f.pfnGsSetShader(device,stream);
    const UINT offset=0;f.pfnSoSetTargets(device,1,0,&output,&offset);f.pfnQueryBegin(device,statistics);f.pfnQueryBegin(device,pipeline);
    f.pfnDraw(device,3,0);expect();f.pfnQueryEnd(device,pipeline);f.pfnQueryEnd(device,statistics);f.pfnSoSetTargets(device,0,0,nullptr,nullptr);expect();
    D3D10_DDI_QUERY_DATA_SO_STATISTICS s{};D3D10_DDI_QUERY_DATA_PIPELINE_STATISTICS p{};queryData(statistics,&s,sizeof(s));queryData(pipeline,&p,sizeof(p));
    counters={p.IAVertices,p.IAPrimitives,p.PSInvocations,UINT(backend->GetDeviceRemovedReason()),s.NumPrimitivesWritten,s.PrimitivesStorageNeeded};
    f.pfnResourceCopy(device,staging,output);D3D10DDI_MAPPED_SUBRESOURCE map{};f.pfnStagingResourceMap(device,staging,0,D3D10_DDI_MAP_READ,0,&map);expect();CHECK(map.pData);
    Words words;std::memcpy(words.data(),map.pData,sizeof(words));f.pfnStagingResourceUnmap(device,staging,0);expect();return words;}
  ~Native(){ComPtr<ID3D11DeviceContext> context;backend->GetImmediateContext(&context);context->ClearState();
    for(auto h:shaderHandles){f.pfnDestroyShader(device,h);expect();}for(auto h:queryHandles){f.pfnDestroyQuery(device,h);expect();}
    for(auto h:resourceHandles){f.pfnDestroyResource(device,h);expect();}context.Reset();backend.Reset();f.pfnDestroyDevice(device);expect();}
};
struct Public {
  ComPtr<ID3D10Device1> device;std::array<ComPtr<ID3D10VertexShader>,2> vertices;
  ComPtr<ID3D10Buffer> output,staging;ComPtr<ID3D10Query> statistics,pipeline;
  Public(bool model41,const std::array<Program*,2>& vs){const auto level=model41?D3D10_FEATURE_LEVEL_10_1:D3D10_FEATURE_LEVEL_10_0;
    CHECK(D3D10CreateDevice1(nullptr,D3D10_DRIVER_TYPE_WARP,nullptr,0,level,D3D10_1_SDK_VERSION,&device)==S_OK&&device->GetFeatureLevel()==level);
    for(UINT i=0;i<2;++i)CHECK(device->CreateVertexShader(vs[i]->binary->GetBufferPointer(),vs[i]->binary->GetBufferSize(),&vertices[i])==S_OK);
    Words clear;clear.fill(0xcccccccc);D3D10_SUBRESOURCE_DATA initial{clear.data(),256,256};D3D10_BUFFER_DESC d{};
    d.ByteWidth=256;d.Usage=D3D10_USAGE_DEFAULT;d.BindFlags=D3D10_BIND_STREAM_OUTPUT;CHECK(device->CreateBuffer(&d,&initial,&output)==S_OK);
    d.Usage=D3D10_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D10_CPU_ACCESS_READ;CHECK(device->CreateBuffer(&d,nullptr,&staging)==S_OK);
    D3D10_QUERY_DESC q{D3D10_QUERY_SO_STATISTICS,0};CHECK(device->CreateQuery(&q,&statistics)==S_OK);
    q.Query=D3D10_QUERY_PIPELINE_STATISTICS;CHECK(device->CreateQuery(&q,&pipeline)==S_OK);device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);}
  ComPtr<ID3D10GeometryShader> stream(const Program& p,bool sparse){const auto entries=publicDeclaration(sparse);ComPtr<ID3D10GeometryShader> shader;
    CHECK(device->CreateGeometryShaderWithStreamOutput(p.binary->GetBufferPointer(),p.binary->GetBufferSize(),entries.data(),UINT(entries.size()),32,&shader)==S_OK);return shader;}
  void queryData(ID3D10Query* q,void* data,UINT size){const auto deadline=GetTickCount64()+5000;
    do{const auto hr=q->GetData(data,size,0);if(hr==S_OK)return;CHECK(hr==S_FALSE&&GetTickCount64()<deadline);Sleep(1);}while(true);}
  Words draw(UINT alternate,ID3D10GeometryShader* stream,Counters& counters){device->VSSetShader(vertices[alternate].Get());device->GSSetShader(stream);
    ID3D10Buffer* raw=output.Get();const UINT offset=0;device->SOSetTargets(1,&raw,&offset);statistics->Begin();pipeline->Begin();device->Draw(3,0);
    pipeline->End();statistics->End();device->SOSetTargets(0,nullptr,nullptr);
    D3D10_QUERY_DATA_SO_STATISTICS s{};D3D10_QUERY_DATA_PIPELINE_STATISTICS p{};queryData(statistics.Get(),&s,sizeof(s));queryData(pipeline.Get(),&p,sizeof(p));
    counters={p.IAVertices,p.IAPrimitives,p.PSInvocations,UINT(device->GetDeviceRemovedReason()),s.NumPrimitivesWritten,s.PrimitivesStorageNeeded};
    device->CopyResource(staging.Get(),output.Get());void* mapped=nullptr;CHECK(staging->Map(D3D10_MAP_READ,0,&mapped)==S_OK&&mapped);
    Words words;std::memcpy(words.data(),mapped,sizeof(words));staging->Unmap();return words;}
  ~Public(){device->ClearState();}
};
template<class Table>static void model(bool model41){Program vs("vs",model41?"vs_4_1":"vs_4_0"),alternate("vs_alternate",model41?"vs_4_1":"vs_4_0"),gs("gs",model41?"gs_4_1":"gs_4_0");
  for(bool geometry:{false,true})for(bool sparse:{false,true}){
    Native<Table> native;const std::array<D3D10DDI_HSHADER,2> vertices={native.vertex(vs),native.vertex(alternate)};Public reference(model41,{&vs,&alternate});
    const auto stream=native.stream(geometry?gs:vs,geometry,sparse);const auto publicStream=reference.stream(geometry?gs:vs,sparse);
    for(UINT producer=0;producer<2;++producer){Counters nc{},pc{};const auto actual=native.draw(vertices[producer],stream,nc);
      const auto original=reference.draw(producer,publicStream.Get(),pc);
      retainScene(model41?41:40,geometry,sparse,producer,"native",actual,nc);retainScene(model41?41:40,geometry,sparse,producer,"public",original,pc);
      checkScene(model41?41:40,geometry,sparse,producer,"native",actual,nc);checkScene(model41?41:40,geometry,sparse,producer,"public",original,pc);
      CHECK(actual==original);++scenes;}
  }
}
int main(){caller=GetCurrentThreadId();policy();retain("distance-original.hlsl",source,sizeof(source)-1);
  model<D3D10DDI_DEVICEFUNCS>(false);model<D3D10_1DDI_DEVICEFUNCS>(true);CHECK(programs==6&&scenes==16&&buffers==32);
  std::printf("D3D10 distance SO PASS checks=%u scenes=16 words=1024 original_buffers=32 query_frames=32 fxc_programs=6 hardware_admission=0\n",checks);}
