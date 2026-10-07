// SPDX-License-Identifier: MIT
// Actual typed legacy shader callbacks and draws, with a controlled WARP
// factory. Typed DDIs own the target; public WARP state supplies the reference.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_contract.h"
#include "../src/umd/umd_result.h"
#include "umd-probe-shaders.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
using Microsoft::WRL::ComPtr;
using dxvk::umd::ShaderStage;
static unsigned checks, callbacks, replacementCallbacks, backendCalls, draws, pixels, shaderPrograms;
static DWORD caller;
static HRESULT lastError = S_OK;
static const char* operation = "initialization";
static const LUID expectedLuid{0x410041, -41};
static ComPtr<ID3D11Device> createdBackend;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, \
  "D3D10 shader failure line %d: %s error=%08lx\n", __LINE__, #value, \
  static_cast<unsigned long>(lastError)); std::abort(); } } while (0)
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  std::fprintf(stderr, "D3D10_SHADER_CALLBACK operation=%s hr=%08lx thread=%lu runtime=%p\n",
    operation, static_cast<unsigned long>(hr), static_cast<unsigned long>(GetCurrentThreadId()), runtime.handle);
  CHECK(runtime.handle && GetCurrentThreadId() == caller && FAILED(hr)); ++callbacks; lastError = hr;
}
static void APIENTRY replacementError(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  ++replacementCallbacks; error(runtime, hr);
}
static void expect(HRESULT hr = S_OK) { CHECK(lastError == dxvk::umd::ddiResult(hr)); lastError = S_OK; }
static void checkpoint(const char* label, uint32_t version = 0) {
  operation = label;
  std::fprintf(stderr, "D3D10_SHADER_CHECKPOINT operation=%s version=%08x last_error=%08lx thread=%lu\n",
    operation, version, static_cast<unsigned long>(lastError), static_cast<unsigned long>(GetCurrentThreadId()));
}
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL logical,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!std::memcmp(&luid, &expectedLuid, sizeof(luid)) && !runtime);
  CHECK(logical == D3D_FEATURE_LEVEL_10_0 || logical == D3D_FEATURE_LEVEL_10_1);
  ++backendCalls; const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) createdBackend = *device;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }
struct Storage {
  std::unique_ptr<void, decltype(&std::free)> bytes;
  explicit Storage(SIZE_T size) : bytes(std::calloc(1, size), &std::free) { CHECK(size && bytes); }
};
static void retain(const char* name, const void* data, size_t bytes) {
  CHECK(bytes <= MAXDWORD);
  const HANDLE file = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  CHECK(file != INVALID_HANDLE_VALUE); DWORD written = 0;
  CHECK(WriteFile(file, data, DWORD(bytes), &written, nullptr) && written == bytes);
  CHECK(CloseHandle(file));
}
static constexpr char source[] = R"(
Texture2D<float> sampled : register(t0);
Texture2DMS<float> multisampled : register(t1);
SamplerState sampler0 : register(s0);
struct Varyings { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 query : TEXCOORD1; };
Varyings vs_main(uint id : SV_VertexID) {
  Varyings v; v.uv = float2((id << 1) & 2, id & 2);
  v.position = float4(v.uv * float2(2,-2) + float2(-1,1), 0, 1);
  v.query = float4(0.125,0.25,0.5,1); return v;
}
float4 queryTextures() {
  uint width, height, samples; multisampled.GetDimensions(width, height, samples);
  float4 g = sampled.Gather(sampler0, float2(0.5,0.5));
  return float4(g.xy, float(samples) / 4.0, 1);
}
Varyings vs_queries(uint id : SV_VertexID) { Varyings v = vs_main(id); v.query = queryTextures(); return v; }
[maxvertexcount(3)] void gs_main(triangle Varyings v[3], inout TriangleStream<Varyings> stream) {
  [unroll] for (uint i = 0; i < 3; ++i) stream.Append(v[i]); stream.RestartStrip();
}
[maxvertexcount(3)] void gs_queries(triangle Varyings v[3], inout TriangleStream<Varyings> stream) {
  [unroll] for (uint i = 0; i < 3; ++i) { Varyings value = v[i]; value.query = queryTextures(); stream.Append(value); }
  stream.RestartStrip();
}
float4 ps_base(Varyings v) : SV_Target { return v.query; }
float4 ps_gather() : SV_Target {
  float4 g = sampled.Gather(sampler0, float2(0.5,0.5)); return float4(g.xy, (g.z + g.w) * 0.5, 1);
}
float4 ps_lod(float4 p : SV_Position) : SV_Target {
  float lod = sampled.CalculateLevelOfDetail(sampler0, p.xy / 16.0);
  return float4(saturate(lod + 4.0) / 4.0, 0.25, 0, 1);
}
float4 ps_position() : SV_Target { return float4(multisampled.GetSamplePosition(0) + 0.5, 0, 1); }
float4 ps_info() : SV_Target {
  uint width, height, samples; multisampled.GetDimensions(width, height, samples);
  return float4(float(samples) / 4.0, 0.25, 0, 1);
}
float4 ps_index(uint sampleIndex : SV_SampleIndex) : SV_Target { return float4(float(sampleIndex) / 3.0, 0.25, 0, 1); }
float4 ps_interpolation(float4 position : SV_Position, sample float2 uv : TEXCOORD0) : SV_Target {
  return float4(frac(uv.x * 16.0) < 0.5 ? 1.0 : 0.0, 0.25, 0, 1);
}
)";
struct Compiled {
  ShaderStage stage;
  std::vector<uint32_t> code;
  ComPtr<ID3DBlob> original;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs, outputs;
  D3D10DDIARG_STAGE_IO_SIGNATURES signature{};
  Compiled(ShaderStage s, const char* entry, const char* profile) : stage(s) {
    CHECK(compileHlslTokens(source, entry, profile, code, &original));
    CHECK((code[0] & 0xff) == (profile[5] == '1' ? 0x41u : 0x40u));
    char containerName[128], tokensName[128]; const unsigned program = ++shaderPrograms;
    CHECK(std::snprintf(containerName, sizeof(containerName), "sm41-fxc-%03u-%s-%s.dxbc", program, profile, entry) > 0);
    CHECK(std::snprintf(tokensName, sizeof(tokensName), "sm41-fxc-%03u-%s-%s.tokens", program, profile, entry) > 0);
    retain(containerName, original->GetBufferPointer(), original->GetBufferSize());
    retain(tokensName, code.data(), code.size() * sizeof(uint32_t));
    std::printf("D3D10_SHADER_FXC program=%u profile=%s entry=%s original=%s tokens=%s\n",
      program, profile, entry, containerName, tokensName);
    ComPtr<ID3D11ShaderReflection> reflection;
    CHECK(D3DReflect(original->GetBufferPointer(), original->GetBufferSize(),
      __uuidof(ID3D11ShaderReflection), &reflection) == S_OK);
    D3D11_SHADER_DESC desc{}; CHECK(reflection->GetDesc(&desc) == S_OK);
    for (bool input : {true, false}) {
      auto& entries = input ? inputs : outputs;
      for (UINT i = 0; i < (input ? desc.InputParameters : desc.OutputParameters); ++i) {
        D3D11_SIGNATURE_PARAMETER_DESC parameter{};
        CHECK((input ? reflection->GetInputParameterDesc(i, &parameter)
          : reflection->GetOutputParameterDesc(i, &parameter)) == S_OK);
        D3D10DDIARG_SIGNATURE_ENTRY native{};
        native.SystemValue = static_cast<D3D10_SB_NAME>(parameter.SystemValueType == D3D_NAME_TARGET
          ? D3D_NAME_UNDEFINED : parameter.SystemValueType);
        native.Register = parameter.Register; native.Mask = parameter.Mask; entries.push_back(native);
      }
    }
    signature.NumInputSignatureEntries = UINT(inputs.size()); signature.pInputSignature = inputs.data();
    signature.NumOutputSignatureEntries = UINT(outputs.size()); signature.pOutputSignature = outputs.data();
  }
};
template<typename Table> struct Fixture {
  Storage memory{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{memory.bytes.get()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};
  Table table{};
  uint64_t canary = 0x41abcdef12345678ull;
  ComPtr<ID3D11Device> backend;
  ComPtr<ID3D11DeviceContext> context;
  Fixture() {
    core.pfnSetErrorCb = error;
    HRESULT hr = E_FAIL;
    checkpoint("CreateDdiTestDevice");
    if constexpr (std::is_same_v<Table, D3D10DDI_DEVICEFUNCS>)
      hr = VioGpuDxvkCreateDdiTestDevice(&expectedLuid, device, {&core}, &core, &table);
    else hr = VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&core}, &core, &table);
    CHECK(hr == S_OK && createdBackend); backend = createdBackend; createdBackend.Reset();
    backend->GetImmediateContext(&context); CHECK(context); expect();
  }
  ~Fixture() { context->ClearState(); context.Reset(); backend.Reset(); table.pfnDestroyDevice(device); CHECK(canary == 0x41abcdef12345678ull); }
  void create(const Compiled& c, D3D10DDI_HSHADER shader) {
    checkpoint(c.stage == ShaderStage::Vertex ? "CreateVertexShader"
      : c.stage == ShaderStage::Geometry ? "CreateGeometryShader" : "CreatePixelShader", c.code[0]);
    if (c.stage == ShaderStage::Vertex) table.pfnCreateVertexShader(device, c.code.data(), shader, {}, &c.signature);
    else if (c.stage == ShaderStage::Geometry) table.pfnCreateGeometryShader(device, c.code.data(), shader, {}, &c.signature);
    else table.pfnCreatePixelShader(device, c.code.data(), shader, {}, &c.signature);
    checkpoint("CreateShaderReturned", c.code[0]);
  }
  void reject41() {
    core.pfnSetErrorCb = replacementError;
    for (auto stage : {ShaderStage::Vertex, ShaderStage::Geometry, ShaderStage::Pixel}) {
      const char* profile = stage == ShaderStage::Vertex ? "vs_4_1" : stage == ShaderStage::Geometry ? "gs_4_1" : "ps_4_1";
      Compiled code(stage, stage == ShaderStage::Vertex ? "vs_main" : stage == ShaderStage::Geometry ? "gs_main" : "ps_base", profile);
      Storage object(table.pfnCalcPrivateShaderSize(device, code.code.data(), &code.signature));
      const D3D10DDI_HSHADER handle{object.bytes.get()}; create(code, handle); expect(E_INVALIDARG);
      table.pfnDestroyShader(device, handle); expect();
    }
  }
};
template<typename Table> struct NativeShader {
  Fixture<Table>& owner;
  Storage memory;
  D3D10DDI_HSHADER handle;
  NativeShader(Fixture<Table>& f, const Compiled& code) : owner(f),
    memory(f.table.pfnCalcPrivateShaderSize(f.device, code.code.data(), &code.signature)), handle{memory.bytes.get()} {
    f.create(code, handle); expect();
  }
  ~NativeShader() { owner.table.pfnDestroyShader(owner.device, handle); expect(); }
};
template<typename Table> struct NativeTarget {
  Fixture<Table>& owner;
  std::unique_ptr<Storage> resourceMemory, viewMemory;
  D3D10DDI_HRESOURCE resource{};
  D3D10DDI_HRENDERTARGETVIEW handle{};
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<ID3D11RenderTargetView> view;
  NativeTarget(Fixture<Table>& f, UINT samples) : owner(f) {
    D3D10DDI_MIPINFO mip{16, 16, 1, 16, 16, 1};
    D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = samples;
    desc.MipLevels = desc.ArraySize = 1;
    resourceMemory = std::make_unique<Storage>(f.table.pfnCalcPrivateResourceSize(f.device, &desc));
    resource = {resourceMemory->bytes.get()};
    checkpoint("CreateTargetResource"); f.table.pfnCreateResource(f.device, &desc, resource, {}); expect();
    D3D10DDIARG_CREATERENDERTARGETVIEW args{};
    args.hDrvResource = resource; args.Format = desc.Format;
    args.ResourceDimension = desc.ResourceDimension; args.Tex2D.ArraySize = 1;
    viewMemory = std::make_unique<Storage>(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args));
    handle = {viewMemory->bytes.get()};
    checkpoint("CreateTargetView"); f.table.pfnCreateRenderTargetView(f.device, &args, handle, {}); expect();
    bind();
    // Inspect the actual bound backend view through public COM, never private UMD storage.
    f.context->OMGetRenderTargets(1, &view, nullptr); CHECK(view);
    ComPtr<ID3D11Resource> backendResource; view->GetResource(&backendResource);
    CHECK(backendResource && backendResource.As(&texture) == S_OK && texture);
    D3D11_TEXTURE2D_DESC actual{}; texture->GetDesc(&actual);
    CHECK(actual.Width == 16 && actual.Height == 16 && actual.MipLevels == 1 && actual.ArraySize == 1
      && actual.Format == desc.Format && actual.SampleDesc.Count == samples && actual.SampleDesc.Quality == 0);
  }
  void bind() {
    checkpoint("SetRenderTargets"); owner.table.pfnSetRenderTargets(owner.device, &handle, 1, 0, {}); expect();
  }
  void clear(FLOAT* color) {
    checkpoint("ClearRenderTargetView"); owner.table.pfnClearRenderTargetView(owner.device, handle, color); expect();
  }
  ~NativeTarget() {
    checkpoint("UnbindRenderTargets"); owner.table.pfnSetRenderTargets(owner.device, nullptr, 0, 0, {}); expect();
    view.Reset(); texture.Reset();
    checkpoint("DestroyTargetView"); owner.table.pfnDestroyRenderTargetView(owner.device, handle); expect();
    checkpoint("DestroyTargetResource"); owner.table.pfnDestroyResource(owner.device, resource); expect();
  }
};
struct RenderInputs {
  ComPtr<ID3D11Texture2D> texture, multi;
  ComPtr<ID3D11ShaderResourceView> sampled, multisampled;
  ComPtr<ID3D11SamplerState> sampler;
  ComPtr<ID3D11RasterizerState> raster;
  explicit RenderInputs(ID3D11Device* device) {
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = desc.Height = 2; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const FLOAT values[]{0.125f, 0.25f, 0.5f, 0.875f}; const D3D11_SUBRESOURCE_DATA initial{values, 8, 16};
    CHECK(device->CreateTexture2D(&desc, &initial, &texture) == S_OK);
    CHECK(device->CreateShaderResourceView(texture.Get(), nullptr, &sampled) == S_OK);
    desc.SampleDesc.Count = 4; desc.Usage = D3D11_USAGE_DEFAULT;
    CHECK(device->CreateTexture2D(&desc, nullptr, &multi) == S_OK);
    CHECK(device->CreateShaderResourceView(multi.Get(), nullptr, &multisampled) == S_OK);
    D3D11_SAMPLER_DESC samplerDesc{}; samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX; CHECK(device->CreateSamplerState(&samplerDesc, &sampler) == S_OK);
    D3D11_RASTERIZER_DESC rasterDesc{}; rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE; rasterDesc.DepthClipEnable = rasterDesc.MultisampleEnable = TRUE;
    CHECK(device->CreateRasterizerState(&rasterDesc, &raster) == S_OK);
  }
  void bind(ID3D11DeviceContext* context) {
    const D3D11_VIEWPORT viewport{0, 0, 16, 16, 0, 1}; context->RSSetViewports(1, &viewport);
    context->RSSetState(raster.Get()); context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11ShaderResourceView* views[]{sampled.Get(), multisampled.Get()}; ID3D11SamplerState* samplers[]{sampler.Get()};
    context->VSSetShaderResources(0, 2, views); context->GSSetShaderResources(0, 2, views); context->PSSetShaderResources(0, 2, views);
    context->VSSetSamplers(0, 1, samplers); context->GSSetSamplers(0, 1, samplers); context->PSSetSamplers(0, 1, samplers);
  }
};
static std::array<uint32_t, 256> readback(ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Texture2D* target, UINT samples) {
  D3D11_TEXTURE2D_DESC desc{}; target->GetDesc(&desc); desc.SampleDesc.Count = 1; desc.BindFlags = 0;
  ComPtr<ID3D11Texture2D> resolved, staging;
  CHECK(device->CreateTexture2D(&desc, nullptr, &resolved) == S_OK);
  if (samples == 1) context->CopyResource(resolved.Get(), target);
  else context->ResolveSubresource(resolved.Get(), 0, target, 0, desc.Format);
  desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  CHECK(device->CreateTexture2D(&desc, nullptr, &staging) == S_OK); context->CopyResource(staging.Get(), resolved.Get());
  D3D11_MAPPED_SUBRESOURCE mapped{}; CHECK(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped) == S_OK);
  std::array<uint32_t, 256> result{};
  for (UINT y = 0; y < 16; ++y) std::memcpy(result.data() + y * 16,
    static_cast<const char*>(mapped.pData) + size_t(y) * mapped.RowPitch, 16 * sizeof(uint32_t));
  context->Unmap(staging.Get(), 0); return result;
}
template<typename Table> void scene(Fixture<Table>& f, const char* pixelEntry, bool model41,
    bool geometry, UINT samples = 1, bool vertexQueries = false, bool geometryQueries = false) {
  Compiled vs(ShaderStage::Vertex, vertexQueries ? "vs_queries" : "vs_main", model41 ? "vs_4_1" : "vs_4_0");
  Compiled gs(ShaderStage::Geometry, geometryQueries ? "gs_queries" : "gs_main", model41 ? "gs_4_1" : "gs_4_0");
  Compiled ps(ShaderStage::Pixel, pixelEntry, model41 ? "ps_4_1" : "ps_4_0");
  if (!std::strcmp(pixelEntry, "ps_interpolation")) {
    // D3D10 links shared register locations. Keep unused position first so
    // the sample-interpolated TEXCOORD0 stays at the producer's register1.
    CHECK(ps.inputs.size() == 2 && ps.inputs[0].SystemValue == D3D10_SB_NAME_POSITION
      && ps.inputs[0].Register == 0 && ps.inputs[0].Mask == 15);
    CHECK(ps.inputs[1].SystemValue == D3D10_SB_NAME_UNDEFINED
      && ps.inputs[1].Register == 1 && ps.inputs[1].Mask == 3);
    for (const Compiled* producer : {&vs, &gs}) {
      CHECK(producer->outputs.size() == 3
        && producer->outputs[0].SystemValue == D3D10_SB_NAME_POSITION
        && producer->outputs[0].Register == 0 && producer->outputs[0].Mask == 15);
      CHECK(producer->outputs[1].SystemValue == D3D10_SB_NAME_UNDEFINED
        && producer->outputs[1].Register == 1 && producer->outputs[1].Mask == 3);
    }
    unsigned declarations = 0;
    for (size_t offset = 2; offset < ps.code.size();) {
      const uint32_t token = ps.code[offset], count = (token >> 24) & 0x7f;
      CHECK(count && count <= ps.code.size() - offset);
      if ((token & 0x7ff) == 98) {
        CHECK(count == 3 && ((token >> 11) & 15) == 6
          && ps.code[offset + 1] == 0x00101012 && ps.code[offset + 2] == 1);
        ++declarations;
      }
      offset += count;
    }
    CHECK(declarations == 1);
    std::printf("D3D10_SHADER_INTERPOLATION producer_register=1 consumer_register=1 mask=1 sample_mode=6 unused_position=1\n");
  }
  NativeShader<Table> vertex(f, vs), pixel(f, ps), geom(f, gs);
  RenderInputs inputs(f.backend.Get());
  NativeTarget<Table> target(f, samples);
  FLOAT clear[]{0, 0, 0, 0};
  inputs.bind(f.context.Get()); target.bind(); target.clear(clear);
  const D3D10_DDI_VIEWPORT viewport{0, 0, 16, 16, 0, 1};
  checkpoint("SetViewports"); f.table.pfnSetViewports(f.device, 1, 0, &viewport); expect();
  checkpoint("IaSetTopology"); f.table.pfnIaSetTopology(f.device, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST); expect();
  checkpoint("VsSetShader", vs.code[0]); f.table.pfnVsSetShader(f.device, vertex.handle);
  checkpoint("PsSetShader", ps.code[0]); f.table.pfnPsSetShader(f.device, pixel.handle);
  checkpoint("GsSetShader", gs.code[0]);
  f.table.pfnGsSetShader(f.device, geometry ? geom.handle : D3D10DDI_HSHADER{}); expect();
  checkpoint("Draw"); f.table.pfnDraw(f.device, 3, 0); expect(); ++draws;
  const auto actual = readback(f.backend.Get(), f.context.Get(), target.texture.Get(), samples);
  f.table.pfnVsSetShader(f.device, {}); f.table.pfnPsSetShader(f.device, {}); f.table.pfnGsSetShader(f.device, {}); expect();
  ComPtr<ID3D11VertexShader> refVs; ComPtr<ID3D11PixelShader> refPs; ComPtr<ID3D11GeometryShader> refGs;
  CHECK(f.backend->CreateVertexShader(vs.original->GetBufferPointer(), vs.original->GetBufferSize(), nullptr, &refVs) == S_OK);
  CHECK(f.backend->CreatePixelShader(ps.original->GetBufferPointer(), ps.original->GetBufferSize(), nullptr, &refPs) == S_OK);
  if (geometry) CHECK(f.backend->CreateGeometryShader(gs.original->GetBufferPointer(), gs.original->GetBufferSize(), nullptr, &refGs) == S_OK);
  inputs.bind(f.context.Get()); ID3D11RenderTargetView* views[]{target.view.Get()};
  f.context->OMSetRenderTargets(1, views, nullptr); f.context->ClearRenderTargetView(target.view.Get(), clear);
  f.context->VSSetShader(refVs.Get(), nullptr, 0); f.context->PSSetShader(refPs.Get(), nullptr, 0);
  f.context->GSSetShader(refGs.Get(), nullptr, 0); f.context->Draw(3, 0);
  const auto reference = readback(f.backend.Get(), f.context.Get(), target.texture.Get(), samples);
  for (size_t i = 0; i < actual.size(); ++i) {
    CHECK(actual[i] == reference[i] && (reference[i] >> 24) == 255); ++pixels;
    if (!std::strcmp(pixelEntry, "ps_index") || !std::strcmp(pixelEntry, "ps_interpolation"))
      CHECK((reference[i] & 255) >= 127 && (reference[i] & 255) <= 128);
  }
  f.context->ClearState();
  std::printf("D3D10_SHADER_SCENE ps=%s model=%s gs=%u samples=%u pixels=256 reference_equal=1\n",
    pixelEntry, model41 ? "4.1" : "4.0", unsigned(geometry), samples);
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  caller = GetCurrentThreadId();
  retain("sm41-original.hlsl", source, sizeof(source) - 1);
  CHECK(dxvk::umd::runtimeMissingD3D10Requirements() && dxvk::umd::runtimeMissingD3D10_1Requirements());
  { Fixture<D3D10DDI_DEVICEFUNCS> f; scene(f, "ps_base", false, false); scene(f, "ps_base", false, true); f.reject41(); }
  { Fixture<D3D10_1DDI_DEVICEFUNCS> f;
    scene(f, "ps_base", false, false); scene(f, "ps_base", false, true);
    for (bool geometry : {false, true}) {
      for (const char* entry : {"ps_gather", "ps_lod", "ps_position", "ps_info"}) scene(f, entry, true, geometry);
      scene(f, "ps_index", true, geometry, 4); scene(f, "ps_interpolation", true, geometry, 4);
    }
    scene(f, "ps_base", true, false, 1, true); scene(f, "ps_base", true, true, 1, false, true);
  }
  CHECK(backendCalls == 2 && callbacks == 3 && replacementCallbacks == 3
    && draws == 18 && pixels == 4608 && shaderPrograms == 57);
  std::printf("native D3D10/10.1 shaders verified checks=%u callbacks=%u draws=%u pixels=%u hardware_admission=0\n",
    checks, callbacks, draws, pixels);
}
