// SPDX-License-Identifier: Zlib
// Explicit matching-core CS probe. Local validation compiles only; no UMD admission.
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
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"predicate dispatch native failure case=%u line=%d expression=%s\n",cases,__LINE__,#v);std::exit(1); } } while (0)
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
struct Compute {
  ComPtr<ID3D11ComputeShader> shader;
  explicit Compute(UINT seed) {
    HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");CHECK(compiler);
    auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler,"D3DCompile"));CHECK(compile);
    char source[700];CHECK(std::snprintf(source,sizeof(source),R"(
RWBuffer<uint> output : register(u0);
AppendStructuredBuffer<uint> logged : register(u1);
[numthreads(1,1,1)] void cs(uint3 tid : SV_DispatchThreadID) {
  uint index=tid.x+3*tid.y+6*tid.z;
  output[index]=0x%08xu|index;
  logged.Append(0xa11ec0deu);
}
)",seed)>0);
    ComPtr<ID3DBlob> code,errors;
    CHECK(compile(source,std::strlen(source),nullptr,nullptr,nullptr,"cs","cs_5_0",0,0,&code,&errors)==S_OK);
    CHECK(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)==S_OK);
  }
};
static UINT initial(unsigned i) { return 0x7e91c523u^(i*0x1357acdfu); }
struct Resources {
  ComPtr<ID3D11Buffer> output,append,count,args,outputStage,appendStage,countStage;
  ComPtr<ID3D11UnorderedAccessView> outputView,appendView;
  std::array<UINT,3> groups{3,2,2};
  static constexpr std::array<UINT,4> counterInitial{0x13b58ad0u,0x456a12ceu,0x917cd53bu,0xfb7542aau};
  explicit Resources(int zero=-1) {
    if (zero>=0) groups[zero]=0;
    std::array<UINT,24> data{};for (unsigned i=0;i<data.size();++i) data[i]=initial(i);
    D3D11_SUBRESOURCE_DATA source{data.data(),0,0};
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=96;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    CHECK(device->CreateBuffer(&desc,&source,&output)==S_OK);
    D3D11_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_UINT;
    view.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;view.Buffer.NumElements=24;
    CHECK(device->CreateUnorderedAccessView(output.Get(),&view,&outputView)==S_OK);
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.StructureByteStride=4;
    CHECK(device->CreateBuffer(&desc,&source,&append)==S_OK);
    view.Format=DXGI_FORMAT_UNKNOWN;view.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_APPEND;
    CHECK(device->CreateUnorderedAccessView(append.Get(),&view,&appendView)==S_OK);
    desc={};desc.ByteWidth=16;source.pSysMem=counterInitial.data();
    CHECK(device->CreateBuffer(&desc,&source,&count)==S_OK);
    const UINT argumentWords[]{0,groups[0],groups[1],groups[2],0};
    desc.ByteWidth=20;desc.MiscFlags=D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;source.pSysMem=argumentWords;
    CHECK(device->CreateBuffer(&desc,&source,&args)==S_OK);
    desc={};desc.ByteWidth=96;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(device->CreateBuffer(&desc,nullptr,&outputStage)==S_OK);
    CHECK(device->CreateBuffer(&desc,nullptr,&appendStage)==S_OK);
    desc.ByteWidth=16;CHECK(device->CreateBuffer(&desc,nullptr,&countStage)==S_OK);
  }
  void reset() const {
    std::array<UINT,24> data{};for (unsigned i=0;i<data.size();++i) data[i]=initial(i);
    immediate->UpdateSubresource(output.Get(),0,nullptr,data.data(),0,0);
    immediate->UpdateSubresource(append.Get(),0,nullptr,data.data(),0,0);
    immediate->UpdateSubresource(count.Get(),0,nullptr,counterInitial.data(),0,0);
  }
  void bind(ID3D11DeviceContext* context,const Compute& compute) const {
    context->CSSetShader(compute.shader.Get(),nullptr,0);
    ID3D11UnorderedAccessView* views[]{outputView.Get(),appendView.Get()};const UINT counts[]{~0u,5};
    context->CSSetUnorderedAccessViews(0,2,views,counts);
  }
  void state(ID3D11DeviceContext* context,const Compute& compute,bool bound=true) const {
    ComPtr<ID3D11ComputeShader> actual;context->CSGetShader(&actual,nullptr,nullptr);
    CHECK(actual.Get()==(bound ? compute.shader.Get() : nullptr));
    ID3D11UnorderedAccessView* views[2]{};context->CSGetUnorderedAccessViews(0,2,views);
    CHECK(views[0]==(bound ? outputView.Get() : nullptr) && views[1]==(bound ? appendView.Get() : nullptr));
    for (auto view : views) if (view) view->Release();
  }
  void dispatch(ID3D11DeviceContext* context,bool indirect) const {
    if (indirect) context->DispatchIndirect(args.Get(),4);
    else context->Dispatch(groups[0],groups[1],groups[2]);
  }
  void captureCounter(ID3D11DeviceContext* context) const {
    context->SetPredication(nullptr,FALSE);
    context->CopyStructureCount(count.Get(),4,appendView.Get());
  }
  void releaseRecorded() { outputView.Reset();appendView.Reset();args.Reset(); }
  void snapshot(bool execute,UINT seed=0xd1700000u) const {
    immediate->SetPredication(nullptr,FALSE);
    std::array<UINT,52> actual{};
    auto read=[&] (ID3D11Buffer* source,ID3D11Buffer* stage,unsigned offset,unsigned length) {
      immediate->CopyResource(stage,source);D3D11_MAPPED_SUBRESOURCE mapped{};
      CHECK(immediate->Map(stage,0,D3D11_MAP_READ,0,&mapped)==S_OK && mapped.pData);
      std::memcpy(actual.data()+offset,mapped.pData,length*4);immediate->Unmap(stage,0);
    };
    read(output.Get(),outputStage.Get(),0,24);read(append.Get(),appendStage.Get(),24,24);read(count.Get(),countStage.Get(),48,4);
    char name[80];CHECK(std::snprintf(name,sizeof(name),"predicate-dispatch-%03u.words",cases)>0);
    std::ofstream file(name,std::ios::binary);CHECK(file.is_open());
    file.write(reinterpret_cast<const char*>(actual.data()),sizeof(actual));file.close();CHECK(!file.fail());
    for (unsigned i=0;i<actual.size();++i) {
      const UINT expected=i<24 ? (execute && i<12 ? seed|i : initial(i)) : i<48 ?
        (execute && i>=29 && i<41 ? 0xa11ec0deu : initial(i-24)) : i==49 ? (execute ? 17u : 5u) : counterInitial[i-48];
      if (actual[i]!=expected) std::fprintf(stderr,"predicate dispatch word case=%u index=%u actual=%08x expected=%08x\n",cases,i,actual[i],expected);
      CHECK(actual[i]==expected);++words;
    }
    ++cases;
  }
};
constexpr std::array<UINT,4> Resources::counterInitial;
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
  Pipeline pipeline;Compute parentCompute(0xd2700000u);
  for (bool indirect : {false,true}) {
    {
      Compute compute(0xd1700000u);
      for (bool visible : {false,true}) for (BOOL value : {FALSE,TRUE}) for (bool hint : {false,true}) {
        Resources resource;auto query=predicate(hint);issue(immediate.Get(),pipeline,query.Get(),visible);
        immediate->SetPredication(query.Get(),value);resource.bind(immediate.Get(),compute);resource.dispatch(immediate.Get(),indirect);
        binding(immediate.Get(),query.Get(),value);resource.state(immediate.Get(),compute);
        resource.captureCounter(immediate.Get());resource.snapshot(hint || visible!=bool(value));
      }
      for (BOOL value : {FALSE,TRUE}) {
        Resources resource;immediate->SetPredication(nullptr,value);resource.bind(immediate.Get(),compute);
        resource.dispatch(immediate.Get(),indirect);binding(immediate.Get(),nullptr,value);resource.state(immediate.Get(),compute);
        resource.captureCounter(immediate.Get());resource.snapshot(true);
      }
      for (int zero=0;zero<3;++zero) {
        Resources resource(zero);immediate->SetPredication(nullptr,FALSE);resource.bind(immediate.Get(),compute);
        resource.dispatch(immediate.Get(),indirect);resource.state(immediate.Get(),compute);resource.captureCounter(immediate.Get());resource.snapshot(false);
      }
      immediate->ClearState();
    }
    for (bool nested : {false,true}) for (BOOL restore : {FALSE,TRUE}) {
      Compute recordedCompute(0xd1700000u);Resources unbound,first,explicitNull,last,afterNested,afterExecution;
      ComPtr<ID3D11DeviceContext> deferred;CHECK(device->CreateDeferredContext(0,&deferred)==S_OK);
      binding(deferred.Get(),nullptr,FALSE);unbound.bind(deferred.Get(),recordedCompute);unbound.dispatch(deferred.Get(),indirect);unbound.captureCounter(deferred.Get());
      auto query=predicate(false);issue(deferred.Get(),pipeline,query.Get(),false);
      deferred->SetPredication(query.Get(),FALSE);first.bind(deferred.Get(),recordedCompute);first.dispatch(deferred.Get(),indirect);
      binding(deferred.Get(),query.Get(),FALSE);first.state(deferred.Get(),recordedCompute);first.captureCounter(deferred.Get());
      deferred->SetPredication(nullptr,-17);binding(deferred.Get(),nullptr,-17);explicitNull.bind(deferred.Get(),recordedCompute);
      explicitNull.dispatch(deferred.Get(),indirect);explicitNull.captureCounter(deferred.Get());
      issue(deferred.Get(),pipeline,query.Get(),true);deferred->SetPredication(query.Get(),FALSE);
      last.bind(deferred.Get(),recordedCompute);last.dispatch(deferred.Get(),indirect);last.captureCounter(deferred.Get());
      ComPtr<ID3D11CommandList> list;CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);
      binding(deferred.Get(),nullptr,FALSE);last.state(deferred.Get(),recordedCompute,false);
      auto parent=predicate(false);issue(immediate.Get(),pipeline,parent.Get(),false);
      if (nested) {
        afterNested.bind(deferred.Get(),parentCompute);deferred->SetPredication(parent.Get(),FALSE);
        deferred->ExecuteCommandList(list.Get(),restore);list.Reset();
        binding(deferred.Get(),restore ? parent.Get() : nullptr,FALSE);afterNested.state(deferred.Get(),parentCompute,restore!=FALSE);
        afterNested.bind(deferred.Get(),parentCompute);afterNested.dispatch(deferred.Get(),indirect);afterNested.captureCounter(deferred.Get());
        CHECK(deferred->FinishCommandList(FALSE,&list)==S_OK);binding(deferred.Get(),nullptr,FALSE);
      }
      query.Reset();unbound.releaseRecorded();first.releaseRecorded();explicitNull.releaseRecorded();last.releaseRecorded();afterNested.releaseRecorded();
      recordedCompute.shader.Reset();deferred.Reset();
      for (unsigned replay=0;replay<3;++replay) {
        immediate->SetPredication(nullptr,FALSE);unbound.reset();first.reset();explicitNull.reset();last.reset();afterNested.reset();afterExecution.reset();
        afterExecution.bind(immediate.Get(),parentCompute);immediate->SetPredication(parent.Get(),FALSE);immediate->ExecuteCommandList(list.Get(),restore);
        binding(immediate.Get(),restore ? parent.Get() : nullptr,FALSE);afterExecution.state(immediate.Get(),parentCompute,restore!=FALSE);
        afterExecution.bind(immediate.Get(),parentCompute);afterExecution.dispatch(immediate.Get(),indirect);afterExecution.captureCounter(immediate.Get());
        unbound.snapshot(true);first.snapshot(false);explicitNull.snapshot(true);last.snapshot(true);
        afterExecution.snapshot(!restore,0xd2700000u);if (nested) afterNested.snapshot(!restore,0xd2700000u);
      }
      list.Reset();immediate->ClearState();
    }
  }
  immediate->ClearState();immediate->Flush();CHECK(cases==158 && words==8216);
  std::printf("D3D11 predicate dispatch native PASS checks=%u cases=158 words=8216 direct=79 indirect=79 immediate_matrix=16 null=4 zero_dimensions=6 deferred_replays=24 default_null=1 explicit_null=1 historical_ends=2 nested_restore=2 parent_restore=2 cs_state=1 counters=1 raw_files=158 ordinary_runtime_admission=0 predication_complete=0\n",checks);
}
