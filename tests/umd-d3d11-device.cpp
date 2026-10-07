#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_d3d11_desc.h"
#include "../src/umd/umd_result.h"
#include "../src/umd/umd_shader11.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_interface.h>
#include <dxbc/dxbc_signature.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

// Compile these checks against the actual SDK ABI: several DDI flag values
// intentionally differ from the public D3D11 resource API.
constexpr bool descriptorContract() {
  D3D10DDI_MIPINFO shape = {};
  shape.TexelWidth = 64;
  D3D11DDIARG_CREATERESOURCE native = {};
  native.pMipInfoList = &shape; native.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
  native.Usage = D3D10_DDI_USAGE_DEFAULT;
  native.MipLevels = native.ArraySize = native.SampleDesc.Count = 1;
  native.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE | D3D11_DDI_BIND_UNORDERED_ACCESS;
  native.MiscFlags = D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED; native.ByteStride = 4;
  D3D11_BUFFER_DESC desc = {};
  if (!dxvk::umd::buffer11Desc(native, desc) || desc.ByteWidth != 64
      || desc.BindFlags != (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)
      || desc.MiscFlags != D3D11_RESOURCE_MISC_BUFFER_STRUCTURED || desc.StructureByteStride != 4) return false;
  native.MapFlags = D3D10_DDI_CPU_ACCESS_READ | D3D10_DDI_CPU_ACCESS_WRITE;
  if (!dxvk::umd::buffer11Desc(native, desc)
      || desc.CPUAccessFlags != (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)) return false;
  native.MiscFlags |= D3D11_DDI_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  if (dxvk::umd::buffer11Desc(native, desc)) return false;
  native.MiscFlags = D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED; native.ByteStride = 3;
  if (dxvk::umd::buffer11Desc(native, desc)) return false;
  native.ByteStride = 4; native.BindFlags |= 0x80000000u;
  return !dxvk::umd::buffer11Desc(native, desc);
}
static_assert(descriptorContract(), "native D3D11 buffer flag translation/range contract");

static unsigned checks, errors, alternateErrors, backendCalls;
static DWORD callerThread;
static HRESULT lastError = S_OK, backendResult = S_OK;
static const LUID expectedLuid = {0x23457891, -54};
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "D3D11 DDI failure line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
static void ok() { CHECK(lastError == S_OK); }
static void failure(HRESULT expected) { CHECK(lastError == dxvk::umd::ddiResult(expected)); lastError = S_OK; }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(hr)); ++errors; lastError = hr;
}
static void APIENTRY alternateError(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  ++alternateErrors; error(runtime, hr);
}

// This source-linked fixture substitutes Microsoft's WARP backend only. The
// production UMD retains its embedded exact-LUID DXVK/Turnip factory.
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!std::memcmp(&luid, &expectedLuid, sizeof(luid)) && !runtime);
  ++backendCalls; *device = nullptr; *context = nullptr;
  if (backendResult != S_OK) return backendResult;
  level = dxvk::umd::implementationFeatureLevel(level);
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) createdContext = *context;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

struct Storage {
  static constexpr uint64_t canary = 0x7294b52dabcdfefaull;
  std::vector<uint64_t> words;
  explicit Storage(SIZE_T size) : words((size + 7) / 8 + 1, 0) { words.back() = canary; }
  void* data() { return words.data(); }
  void check() { CHECK(words.back() == canary); }
  bool empty() const { for (size_t i = 0; i + 1 < words.size(); ++i) if (words[i]) return false; return true; }
};
template<typename Table> struct TableOutput { Table table; uint64_t canary = Storage::canary; };

struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  D3D11DDI_CORELAYER_DEVICECALLBACKS callbacks{};
  TableOutput<D3D11DDI_DEVICEFUNCS> output{};
  ComPtr<ID3D11DeviceContext> context;
  explicit Fixture(D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0) {
    callbacks.pfnSetErrorCb = error;
    CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&callbacks}, &callbacks, &output.table, level) == S_OK);
    CHECK(output.canary == Storage::canary); context = createdContext; createdContext.Reset();
    CHECK(output.table.pfnCreateResource && output.table.pfnSetRenderTargets && output.table.pfnCreateComputeShader
      && output.table.pfnDispatch && output.table.pfnCopyStructureCount && output.table.pfnRelocateDeviceFuncs);
    CHECK(!output.table.pfnCommandListExecute && !output.table.pfnCreateDeferredContext
      && !output.table.pfnCreateCommandList && !output.table.pfnRecycleCreateCommandList);
  }
  ~Fixture() {
    context.Reset(); output.table.pfnDestroyDevice(device); storage.check();
    CHECK(output.canary == Storage::canary);
  }
};
struct Buffer {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  Buffer(Fixture& f, UINT bytes, UINT bind, UINT misc = 0, UINT stride = 0,
      const void* initial = nullptr, bool staging = false)
  : fixture(f), storage([&] { D3D11DDIARG_CREATERESOURCE desc{};
      return f.output.table.pfnCalcPrivateResourceSize(f.device, &desc); }()), handle{storage.data()} {
    D3D10DDI_MIPINFO shape = {}; shape.TexelWidth = shape.PhysicalWidth = bytes;
    shape.TexelHeight = shape.PhysicalHeight = shape.TexelDepth = shape.PhysicalDepth = 1;
    D3D10_DDIARG_SUBRESOURCE_UP data = {const_cast<void*>(initial), bytes, bytes};
    D3D11DDIARG_CREATERESOURCE args = {}; args.pMipInfoList = &shape;
    args.pInitialDataUP = initial ? &data : nullptr; args.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
    args.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags = bind; args.MiscFlags = misc; args.ByteStride = stride;
    args.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    args.MipLevels = args.ArraySize = args.SampleDesc.Count = 1;
    f.output.table.pfnCreateResource(f.device, &args, handle, {}); ok(); storage.check();
  }
  ~Buffer() { fixture.output.table.pfnDestroyResource(fixture.device, handle); storage.check(); }
  std::vector<uint32_t> read(UINT count) {
    Buffer staging(fixture, count * 4, 0, 0, 0, nullptr, true);
    fixture.output.table.pfnResourceCopy(fixture.device, staging.handle, handle); ok();
    D3D10DDI_MAPPED_SUBRESOURCE mapped = {};
    fixture.output.table.pfnStagingResourceMap(fixture.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &mapped); ok();
    CHECK(mapped.pData); std::vector<uint32_t> result(count);
    std::memcpy(result.data(), mapped.pData, count * 4);
    fixture.output.table.pfnStagingResourceUnmap(fixture.device, staging.handle, 0); ok();
    return result;
  }
};
struct Uav {
  Fixture& fixture;
  Storage storage;
  D3D11DDI_HUNORDEREDACCESSVIEW handle;
  Uav(Fixture& f, Buffer& buffer, UINT count, DXGI_FORMAT format, UINT flags = 0)
  : fixture(f), storage([&] { D3D11DDIARG_CREATEUNORDEREDACCESSVIEW args{};
      return f.output.table.pfnCalcPrivateUnorderedAccessViewSize(f.device, &args); }()), handle{storage.data()} {
    D3D11DDIARG_CREATEUNORDEREDACCESSVIEW args = {};
    args.hDrvResource = buffer.handle; args.Format = format; args.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
    args.Buffer = {0, count, flags};
    f.output.table.pfnCreateUnorderedAccessView(f.device, &args, handle, {}); ok(); storage.check();
  }
  ~Uav() { fixture.output.table.pfnDestroyUnorderedAccessView(fixture.device, handle); storage.check(); }
};
struct Srv {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HSHADERRESOURCEVIEW handle;
  Srv(Fixture& f, Buffer& buffer, UINT count)
  : fixture(f), storage([&] { D3D11DDIARG_CREATESHADERRESOURCEVIEW args{};
      return f.output.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args); }()), handle{storage.data()} {
    D3D11DDIARG_CREATESHADERRESOURCEVIEW args = {}; args.hDrvResource = buffer.handle;
    args.ResourceDimension = D3D10DDIRESOURCE_BUFFER; args.Format = DXGI_FORMAT_UNKNOWN;
    args.Buffer.FirstElement = 0; args.Buffer.NumElements = count;
    f.output.table.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok(); storage.check();
  }
  ~Srv() { fixture.output.table.pfnDestroyShaderResourceView(fixture.device, handle); storage.check(); }
};
static std::vector<uint32_t> compile(const char* source, const char* profile = "cs_5_0", ComPtr<ID3DBlob>* original = nullptr) {
  ComPtr<ID3DBlob> binary, diagnostic;
  const HRESULT hr = D3DCompile(source, std::strlen(source), "umd-d3d11-fixture", nullptr, nullptr, "main", profile, 0, 0, &binary, &diagnostic);
  if (FAILED(hr) && diagnostic) std::fprintf(stderr, "%s\n", static_cast<const char*>(diagnostic->GetBufferPointer()));
  CHECK(hr == S_OK);
  if (original) *original = binary;
  auto bytes = static_cast<const unsigned char*>(binary->GetBufferPointer());
  const size_t size = binary->GetBufferSize(); CHECK(size >= 32);
  auto word = [&](size_t offset) { CHECK(offset + 4 <= size); uint32_t value; std::memcpy(&value, bytes + offset, 4); return value; };
  const UINT chunks = word(28);
  for (UINT i = 0; i < chunks; ++i) {
    const UINT offset = word(32 + i * 4); const UINT fourcc = word(offset);
    if (fourcc != 0x58454853 && fourcc != 0x52444853) continue;
    const UINT codeSize = word(offset + 4); CHECK(!(codeSize % 4) && codeSize <= size - offset - 8);
    std::vector<uint32_t> code(codeSize / 4); std::memcpy(code.data(), bytes + offset + 8, codeSize); return code;
  }
  CHECK(false); return {};
}
struct Compute {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HSHADER handle;
  Compute(Fixture& f, const char* source)
  : fixture(f), storage(f.output.table.pfnCalcPrivateShaderSize(f.device, nullptr, nullptr)), handle{storage.data()} {
    auto code = compile(source);
    f.output.table.pfnCreateComputeShader(f.device, code.data(), handle, {}); ok(); storage.check();
    // The compiled blob/raw token storage may disappear immediately.
    std::fill(code.begin(), code.end(), 0xcccccccc);
  }
  ~Compute() { fixture.output.table.pfnDestroyShader(fixture.device, handle); storage.check(); }
};
static void inspectUav(Fixture& f, ID3D11UnorderedAccessView* expected) {
  ComPtr<ID3D11UnorderedAccessView> bound;
  f.context->CSGetUnorderedAccessViews(0, 1, &bound); CHECK(bound.Get() == expected);
}
static void textureMinLod(Fixture& f) {
  auto& table = f.output.table;
  D3D10DDI_MIPINFO shapes[2] = {};
  for (UINT i = 0; i < 2; ++i) {
    shapes[i].TexelWidth = shapes[i].PhysicalWidth = 4u >> i;
    shapes[i].TexelHeight = shapes[i].PhysicalHeight = 4u >> i;
    shapes[i].TexelDepth = shapes[i].PhysicalDepth = 1;
  }
  D3D11DDIARG_CREATERESOURCE args = {};
  args.pMipInfoList = shapes; args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  args.Usage = D3D10_DDI_USAGE_DEFAULT; args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
  args.MiscFlags = D3D11_DDI_RESOURCE_MISC_RESOURCE_CLAMP;
  args.Format = DXGI_FORMAT_R8G8B8A8_UNORM; args.MipLevels = 2;
  args.ArraySize = args.SampleDesc.Count = 1;
  Storage resourceStorage(table.pfnCalcPrivateResourceSize(f.device, &args));
  D3D10DDI_HRESOURCE resource{resourceStorage.data()};
  table.pfnCreateResource(f.device, &args, resource, {}); ok(); resourceStorage.check();
  D3D11DDIARG_CREATESHADERRESOURCEVIEW viewArgs = {};
  viewArgs.hDrvResource = resource; viewArgs.Format = args.Format;
  viewArgs.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  viewArgs.Tex2D.MipLevels = 2; viewArgs.Tex2D.ArraySize = 1;
  Storage viewStorage(table.pfnCalcPrivateShaderResourceViewSize(f.device, &viewArgs));
  D3D10DDI_HSHADERRESOURCEVIEW view{viewStorage.data()};
  table.pfnCreateShaderResourceView(f.device, &viewArgs, view, {}); ok();
  table.pfnCsSetShaderResources(f.device, 0, 1, &view); ok();
  ComPtr<ID3D11ShaderResourceView> observed;
  f.context->CSGetShaderResources(0, 1, &observed); CHECK(observed);
  ComPtr<ID3D11Resource> backend; observed->GetResource(&backend); CHECK(backend);
  table.pfnSetResourceMinLOD(f.device, resource, 1.25f); ok();
  CHECK(f.context->GetResourceMinLOD(backend.Get()) == 1.25f);
  table.pfnSetResourceMinLOD(f.device, resource, 3.0f); failure(E_INVALIDARG);
  CHECK(f.context->GetResourceMinLOD(backend.Get()) == 1.25f);
  Buffer buffer(f, 4, 0);
  table.pfnSetResourceMinLOD(f.device, buffer.handle, 0); failure(E_INVALIDARG);
  D3D10DDI_HSHADERRESOURCEVIEW empty{};
  table.pfnCsSetShaderResources(f.device, 0, 1, &empty); ok();
  observed.Reset(); backend.Reset();
  table.pfnDestroyShaderResourceView(f.device, view); viewStorage.check();
  table.pfnDestroyResource(f.device, resource); resourceStorage.check();
}
static void computeAndCounters(Fixture& f) {
  auto& table = f.output.table;
  std::array<UINT, 16> input{}; for (UINT i = 0; i < input.size(); ++i) input[i] = i + 7;
  const std::array<UINT, 4> constants = {13, 0, 0, 0};
  Buffer source(f, 64, D3D10_DDI_BIND_SHADER_RESOURCE, D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED, 4, input.data());
  Buffer destination(f, 64, D3D11_DDI_BIND_UNORDERED_ACCESS, D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED, 4);
  Buffer constant(f, 16, D3D10_DDI_BIND_CONSTANT_BUFFER, 0, 0, constants.data());
  Srv srv(f, source, 16); Uav uav(f, destination, 16, DXGI_FORMAT_UNKNOWN);
  Compute shader(f, "StructuredBuffer<uint> src:register(t0);RWStructuredBuffer<uint> dst:register(u0);"
    "cbuffer C:register(b0){uint bias;}[numthreads(4,1,1)]void main(uint3 id:SV_DispatchThreadID){dst[id.x]=src[id.x]*3+bias;}");
  table.pfnCsSetShader(f.device, shader.handle);
  table.pfnCsSetShaderResources(f.device, 0, 1, &srv.handle);
  table.pfnCsSetConstantBuffers(f.device, 0, 1, &constant.handle);
  table.pfnCsSetUnorderedAccessViews(f.device, 0, 1, &uav.handle, nullptr); ok();
  table.pfnDispatch(f.device, 4, 1, 1); ok();
  auto result = destination.read(16);
  for (UINT i = 0; i < result.size(); ++i) CHECK(result[i] == input[i] * 3 + constants[0]);
  const UINT indirectData[3] = {4, 1, 1};
  Buffer indirect(f, 12, 0, D3D11_DDI_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0, indirectData);
  const UINT clear[4] = {0, 8, 9, 10}; table.pfnClearUnorderedAccessViewUint(f.device, uav.handle, clear);
  table.pfnDispatchIndirect(f.device, indirect.handle, 0); ok();
  result = destination.read(16); for (UINT i = 0; i < result.size(); ++i) CHECK(result[i] == input[i] * 3 + constants[0]);
  table.pfnDispatchIndirect(f.device, indirect.handle, 4); failure(E_INVALIDARG);
  table.pfnDispatch(f.device, 65536, 1, 1); failure(E_INVALIDARG);
  ComPtr<ID3D11UnorderedAccessView> bound; f.context->CSGetUnorderedAccessViews(0, 1, &bound); CHECK(bound);
  table.pfnCsSetUnorderedAccessViews(f.device, UINT(-1), 2, &uav.handle, nullptr); failure(E_INVALIDARG); inspectUav(f, bound.Get());
  table.pfnCsSetShaderWithIfaces(f.device, shader.handle, 0, nullptr, nullptr); ok();
  table.pfnCsSetShader(f.device, {}); table.pfnDispatch(f.device, 0, 1, 1); ok();

  Buffer appended(f, 64, D3D11_DDI_BIND_UNORDERED_ACCESS, D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED, 4);
  Uav append(f, appended, 16, DXGI_FORMAT_UNKNOWN, D3D11_DDI_BUFFER_UAV_FLAG_APPEND);
  Buffer count(f, 4, 0);
  Compute appendShader(f, "AppendStructuredBuffer<uint> dst:register(u0);[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){dst.Append(id.x+100);}");
  UINT initialCount = 0;
  table.pfnCsSetShader(f.device, appendShader.handle);
  table.pfnCsSetUnorderedAccessViews(f.device, 0, 1, &append.handle, &initialCount);
  table.pfnDispatch(f.device, 4, 1, 1);
  table.pfnCopyStructureCount(f.device, count.handle, 0, append.handle); ok(); CHECK(count.read(1)[0] == 4);
  initialCount = UINT(-1); table.pfnCsSetUnorderedAccessViews(f.device, 0, 1, &append.handle, &initialCount);
  table.pfnDispatch(f.device, 2, 1, 1); table.pfnCopyStructureCount(f.device, count.handle, 0, append.handle); ok(); CHECK(count.read(1)[0] == 6);
  initialCount = 7; table.pfnCsSetUnorderedAccessViews(f.device, 0, 1, &append.handle, &initialCount);
  table.pfnCopyStructureCount(f.device, count.handle, 0, append.handle); ok(); CHECK(count.read(1)[0] == 7);
  table.pfnCopyStructureCount(f.device, count.handle, 4, append.handle); failure(E_INVALIDARG);
  table.pfnCsSetUnorderedAccessViews(f.device, 0, 1, &append.handle, nullptr); failure(E_INVALIDARG);

  Buffer raw(f, 64, D3D11_DDI_BIND_UNORDERED_ACCESS, D3D11_DDI_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS);
  Uav rawView(f, raw, 16, DXGI_FORMAT_R32_TYPELESS, D3D11_DDI_BUFFER_UAV_FLAG_RAW);
  const UINT rawClear[4] = {0x13572468, 1, 2, 3}; table.pfnClearUnorderedAccessViewUint(f.device, rawView.handle, rawClear); ok();
  for (UINT value : raw.read(16)) CHECK(value == rawClear[0]);
  Buffer typed(f, 64, D3D11_DDI_BIND_UNORDERED_ACCESS);
  Uav typedView(f, typed, 16, DXGI_FORMAT_R32_FLOAT);
  const FLOAT floatClear[4] = {0.25f, 1, 2, 3}; table.pfnClearUnorderedAccessViewFloat(f.device, typedView.handle, floatClear); ok();
  for (UINT value : typed.read(16)) CHECK(value == 0x3e800000);

  // A foreign view in the tail must not partially replace the valid head.
  Fixture foreign;
  Buffer foreignBuffer(foreign, 64, D3D11_DDI_BIND_UNORDERED_ACCESS);
  Uav foreignView(foreign, foreignBuffer, 16, DXGI_FORMAT_R32_UINT);
  std::array<D3D11DDI_HUNORDEREDACCESSVIEW, 2> mixed = {rawView.handle, foreignView.handle};
  f.context->CSGetUnorderedAccessViews(0, 1, &bound);
  table.pfnCsSetUnorderedAccessViews(f.device, 0, 2, mixed.data(), nullptr); failure(E_INVALIDARG); inspectUav(f, bound.Get());
}

static void independentBlend10_1() {
  Storage storage(VioGpuDxvkPrivateDeviceSize()); D3D10DDI_HDEVICE device{storage.data()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks{}; callbacks.pfnSetErrorCb = error;
  TableOutput<D3D10_1DDI_DEVICEFUNCS> output{};
  CHECK(VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&callbacks}, &callbacks, &output.table) == S_OK);
  auto context = createdContext; createdContext.Reset();
  D3D10_1_DDI_BLEND_DESC args{}; args.IndependentBlendEnable = TRUE;
  for (auto& target : args.RenderTarget) {
    target.SrcBlend = target.SrcBlendAlpha = D3D10_DDI_BLEND_ONE;
    target.DestBlend = target.DestBlendAlpha = D3D10_DDI_BLEND_ZERO;
    target.BlendOp = target.BlendOpAlpha = D3D10_DDI_BLEND_OP_ADD; target.RenderTargetWriteMask = 15;
  }
  args.RenderTarget[1].RenderTargetWriteMask = 2;
  Storage blendStorage(output.table.pfnCalcPrivateBlendStateSize(device, &args));
  D3D10DDI_HBLENDSTATE blend{blendStorage.data()}; output.table.pfnCreateBlendState(device, &args, blend, {}); ok();
  const FLOAT factors[4] = {1, 1, 1, 1}; output.table.pfnSetBlendState(device, blend, factors, UINT(-1)); ok();
  ComPtr<ID3D11BlendState> observed; context->OMGetBlendState(&observed, nullptr, nullptr); CHECK(observed);
  D3D11_BLEND_DESC desc{}; observed->GetDesc(&desc); CHECK(desc.IndependentBlendEnable && desc.RenderTarget[0].RenderTargetWriteMask == 15 && desc.RenderTarget[1].RenderTargetWriteMask == 2);
  output.table.pfnRelocateDeviceFuncs(device, &output.table); ok();
  const UINT callbacksBefore = alternateErrors;
  callbacks.pfnSetErrorCb = alternateError;
  output.table.pfnRelocateDeviceFuncs(device, nullptr); failure(E_INVALIDARG);
  CHECK(alternateErrors == callbacksBefore + 1);
  output.table.pfnDestroyBlendState(device, blend); blendStorage.check(); observed.Reset(); context.Reset();
  output.table.pfnDestroyDevice(device); storage.check(); CHECK(output.canary == Storage::canary);
}

static std::vector<D3D10DDIARG_SIGNATURE_ENTRY> nativeSignature(dxbc_spv::util::ByteReader bytes) {
  using dxbc_spv::dxbc::SignatureSysval;
  dxbc_spv::dxbc::Signature signature(bytes); CHECK(signature);
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> entries;
  for (const auto& entry : signature) {
    UINT system = UINT(entry.getSystemValue());
    switch (entry.getSystemValue()) {
      case SignatureSysval::eQuadEdgeTessFactor: system = 11 + entry.getSemanticIndex(); break;
      case SignatureSysval::eQuadInsideTessFactor: system = 15 + entry.getSemanticIndex(); break;
      case SignatureSysval::eTriEdgeTessFactor: system = 17 + entry.getSemanticIndex(); break;
      case SignatureSysval::eTriInsideTessFactor: system = 20; break;
      case SignatureSysval::eLineDetailTessFactor: system = 21; break;
      case SignatureSysval::eLineDensityTessFactor: system = 22; break;
      default: if (system >= 64) system = 0; break;
    }
    entries.push_back({D3D10_SB_NAME(system), UINT(entry.getRegisterIndex()), BYTE(entry.getComponentMask())});
  }
  return entries;
}
struct GraphicsShader {
  Fixture& fixture;
  dxvk::umd::ShaderStage stage;
  Storage storage;
  D3D10DDI_HSHADER handle;
  ComPtr<ID3DBlob> original;
  dxvk::umd::ShaderCode11 decoded;
  bool alive = true;
  GraphicsShader(Fixture& f, dxvk::umd::ShaderStage s, const char* source,
      const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* stream = nullptr)
  : fixture(f), stage(s), storage(f.output.table.pfnCalcPrivateShaderSize(f.device,nullptr,nullptr)), handle{storage.data()} {
    using dxvk::umd::ShaderStage;
    const char* profiles[] = {"ps_5_0","vs_5_0","gs_5_0","hs_5_0","ds_5_0","cs_5_0"};
    auto code = compile(source,profiles[UINT(stage)],&original);
    CHECK(dxvk::umd::decodeShader11(stage,code.data(),code.size(),decoded));
    dxbc_spv::dxbc::Container container(original->GetBufferPointer(),original->GetBufferSize()); CHECK(container);
    auto inputs = nativeSignature(container.getInputSignatureChunk());
    auto outputs = nativeSignature(container.getOutputSignatureChunk());
    D3D10DDIARG_STAGE_IO_SIGNATURES signature{inputs.data(),UINT(inputs.size()),outputs.data(),UINT(outputs.size())};
    auto& table = f.output.table;
    if (stage == ShaderStage::Hull || stage == ShaderStage::Domain) {
      auto patch = nativeSignature(container.getPatchConstantSignatureChunk());
      D3D11DDIARG_TESSELLATION_IO_SIGNATURES tess{inputs.data(),UINT(inputs.size()),outputs.data(),UINT(outputs.size()),patch.data(),UINT(patch.size())};
      if (stage == ShaderStage::Hull) table.pfnCreateHullShader(f.device,code.data(),handle,{},&tess);
      else table.pfnCreateDomainShader(f.device,code.data(),handle,{},&tess);
    } else if (stage == ShaderStage::Vertex) table.pfnCreateVertexShader(f.device,code.data(),handle,{},&signature);
    else if (stage == ShaderStage::Pixel) table.pfnCreatePixelShader(f.device,code.data(),handle,{},&signature);
    else if (stage == ShaderStage::Compute) table.pfnCreateComputeShader(f.device,code.data(),handle,{});
    else if (stream) {
      auto args = *stream; args.pShaderCode = code.data();
      table.pfnCreateGeometryShaderWithStreamOutput(f.device,&args,handle,{},&signature);
    } else table.pfnCreateGeometryShader(f.device,code.data(),handle,{},&signature);
    ok(); storage.check(); std::fill(code.begin(),code.end(),0xcccccccc);
  }
  ~GraphicsShader() { if (alive) fixture.output.table.pfnDestroyShader(fixture.device,handle); storage.check(); }
  void bind() {
    using dxvk::umd::ShaderStage;
    auto& table = fixture.output.table;
    switch (stage) {
      case ShaderStage::Vertex: table.pfnVsSetShader(fixture.device,handle); break;
      case ShaderStage::Pixel: table.pfnPsSetShader(fixture.device,handle); break;
      case ShaderStage::Geometry: table.pfnGsSetShader(fixture.device,handle); break;
      case ShaderStage::Hull: table.pfnHsSetShader(fixture.device,handle); break;
      case ShaderStage::Domain: table.pfnDsSetShader(fixture.device,handle); break;
      case ShaderStage::Compute: table.pfnCsSetShader(fixture.device,handle); break;
    }
    ok();
  }
};
struct Texture {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  DXGI_FORMAT format;
  Texture(Fixture& f, DXGI_FORMAT fmt, bool staging = false)
  : fixture(f), storage([&] { D3D11DDIARG_CREATERESOURCE args{}; return f.output.table.pfnCalcPrivateResourceSize(f.device,&args); }()), handle{storage.data()}, format(fmt) {
    D3D10DDI_MIPINFO shape{16,16,1,16,16,1};
    D3D11DDIARG_CREATERESOURCE args{}; args.pMipInfoList = &shape;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Format = format;
    args.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags = staging ? 0 : D3D10_DDI_BIND_RENDER_TARGET;
    args.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    args.MipLevels = args.ArraySize = args.SampleDesc.Count = 1;
    f.output.table.pfnCreateResource(f.device,&args,handle,{}); ok(); storage.check();
  }
  ~Texture() { fixture.output.table.pfnDestroyResource(fixture.device,handle); storage.check(); }
  std::array<UINT,4> pixel(UINT x = 8, UINT y = 8) {
    Texture staging(fixture,format,true);
    fixture.output.table.pfnResourceCopy(fixture.device,staging.handle,handle); ok();
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    fixture.output.table.pfnStagingResourceMap(fixture.device,staging.handle,0,D3D10_DDI_MAP_READ,0,&mapped); ok(); CHECK(mapped.pData);
    std::array<UINT,4> value{};
    const UINT bytes = format == DXGI_FORMAT_R8G8B8A8_UNORM ? 4 : 16;
    CHECK(mapped.RowPitch >= 16 * bytes);
    std::memcpy(value.data(),static_cast<const unsigned char*>(mapped.pData) + y*mapped.RowPitch + x*bytes,bytes);
    fixture.output.table.pfnStagingResourceUnmap(fixture.device,staging.handle,0); ok(); return value;
  }
};
struct ColorTarget {
  Fixture& fixture;
  Texture texture;
  Storage storage;
  D3D10DDI_HRENDERTARGETVIEW handle;
  ColorTarget(Fixture& f, DXGI_FORMAT format)
  : fixture(f), texture(f,format), storage([&] { D3D10DDIARG_CREATERENDERTARGETVIEW args{}; return f.output.table.pfnCalcPrivateRenderTargetViewSize(f.device,&args); }()), handle{storage.data()} {
    D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = texture.handle; args.Format = format;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
    f.output.table.pfnCreateRenderTargetView(f.device,&args,handle,{}); ok(); storage.check();
  }
  ~ColorTarget() { fixture.output.table.pfnDestroyRenderTargetView(fixture.device,handle); storage.check(); }
  void bind() {
    fixture.output.table.pfnSetRenderTargets(fixture.device,&handle,1,0,{},nullptr,nullptr,1,0,1,0); ok();
    FLOAT clear[] = {0,0,0,0}; fixture.output.table.pfnClearRenderTargetView(fixture.device,handle,clear); ok();
  }
};
struct GraphicsState {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HRASTERIZERSTATE handle;
  explicit GraphicsState(Fixture& f) : fixture(f), storage(f.output.table.pfnCalcPrivateRasterizerStateSize(f.device,nullptr)), handle{storage.data()} {
    D3D10_DDI_RASTERIZER_DESC args{}; args.FillMode = D3D10_DDI_FILL_SOLID;
    args.CullMode = D3D10_DDI_CULL_NONE; args.DepthClipEnable = TRUE;
    f.output.table.pfnCreateRasterizerState(f.device,&args,handle,{});
    f.output.table.pfnSetRasterizerState(f.device,handle);
    D3D10_DDI_VIEWPORT viewport{0,0,16,16,0,1}; f.output.table.pfnSetViewports(f.device,1,0,&viewport); ok();
  }
  ~GraphicsState() { fixture.output.table.pfnDestroyRasterizerState(fixture.device,handle); storage.check(); }
};
static const char* triangleVertex = R"(
struct V {float4 p:SV_Position; float clip:SV_ClipDistance0; nointerpolation uint value:DATA0;};
V main(uint id:SV_VertexID) { V v;float2 xy=float2((id<<1)&2,id&2);
v.p=float4(xy*float2(2,-2)+float2(-1,1),0,1);v.clip=1;v.value=12345;return v; })";
static void graphicsSm5(Fixture& f) {
  using dxvk::umd::ShaderStage;
  GraphicsState state(f);
  GraphicsShader vertex(f,ShaderStage::Vertex,triangleVertex);
  GraphicsShader pixel(f,ShaderStage::Pixel,"struct V{float4 p:SV_Position;float clip:SV_ClipDistance0;nointerpolation uint value:DATA0;};uint4 main(V v):SV_Target0{return uint4(v.value,77,999,1);}");
  ColorTarget target(f,DXGI_FORMAT_R32G32B32A32_UINT); target.bind(); vertex.bind(); pixel.bind();
  f.output.table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  f.output.table.pfnDraw(f.device,3,0); ok();
  CHECK((target.texture.pixel() == std::array<UINT,4>{12345,77,999,1}));
  GraphicsShader signedPixel(f,ShaderStage::Pixel,"struct V{float4 p:SV_Position;float clip:SV_ClipDistance0;nointerpolation uint value:DATA0;};int4 main(V v):SV_Target0{return int4(-int(v.value),-7,99,1);}");
  ColorTarget signedTarget(f,DXGI_FORMAT_R32G32B32A32_SINT); signedTarget.bind(); signedPixel.bind();
  f.output.table.pfnDraw(f.device,3,0); ok();
  CHECK((signedTarget.texture.pixel() == std::array<UINT,4>{UINT(-12345),UINT(-7),99,1}));
  // A valid shader from the wrong stage is invalidated by this failed bind.
  GraphicsShader wrong(f,ShaderStage::Vertex,triangleVertex);
  f.output.table.pfnPsSetShaderWithIfaces(f.device,wrong.handle,0,nullptr,nullptr); wrong.alive = false; failure(E_INVALIDARG);
  f.output.table.pfnDraw(f.device,3,0); ok();
  CHECK((signedTarget.texture.pixel() == std::array<UINT,4>{UINT(-12345),UINT(-7),99,1}));
  f.output.table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY(65)); failure(E_INVALIDARG);
  f.output.table.pfnDraw(f.device,3,0); ok();
}
template<typename Data> static Data queryResult(Fixture& f, ID3D11Query* query) {
  f.context->Flush(); Data data{}; HRESULT hr = S_FALSE;
  const ULONGLONG start = GetTickCount64();
  do { hr = f.context->GetData(query,&data,sizeof(data),0); if (hr == S_FALSE) SwitchToThread(); }
  while (hr == S_FALSE && GetTickCount64() - start < 10000);
  CHECK(hr == S_OK); return data;
}
static void geometryStreams(Fixture& f) {
  using dxvk::umd::ShaderStage;
  GraphicsShader vertex(f,ShaderStage::Vertex,"struct V{float4 p:SV_Position;uint id:DATA0;};V main(uint id:SV_VertexID){V v;v.p=float4(0,0,0,1);v.id=id;return v;}");
  const char* geometry = R"(
struct V{float4 p:SV_Position;uint id:DATA0;};struct O{float4 p:SV_Position;uint2 value:DATA0;};
[maxvertexcount(3)]void main(point V v[1],inout PointStream<O> s0,inout PointStream<O> s1,inout PointStream<O> s2){
O o;o.p=v[0].p;o.value=uint2(v[0].id,900);s0.Append(o);o.value=uint2(v[0].id+100,901);s1.Append(o);
o.value=uint2(v[0].id+200,902);s2.Append(o);})";
  auto tokens = compile(geometry,"gs_5_0"); dxvk::umd::ShaderCode11 decoded;
  CHECK(dxvk::umd::decodeShader11(ShaderStage::Geometry,tokens.data(),tokens.size(),decoded));
  std::array<D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY,4> entries{};
  for (UINT stream = 0; stream < 3; ++stream) {
    bool found = false;
    for (const auto& entry : decoded.outputs) if (entry.stream == stream && !entry.systemValue) {
      CHECK(!found && entry.mask == 3); found = true; entries[stream] = {stream,stream,entry.registerIndex,entry.mask};
    }
    CHECK(found);
  }
  entries[3] = {0,0,D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP,1};
  const UINT strides[] = {32,16,24};
  D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT args{}; args.pOutputStreamDecl = entries.data(); args.NumEntries = UINT(entries.size());
  args.BufferStridesInBytes = strides; args.NumStrides = 3; args.RasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
  GraphicsShader shader(f,ShaderStage::Geometry,geometry,&args); vertex.bind(); shader.bind();
  std::array<UINT,64> poison; poison.fill(0xa5a55a5a);
  Buffer stream0(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  Buffer stream1(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  Buffer stream2(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  const D3D10DDI_HRESOURCE buffers[] = {stream0.handle,stream1.handle,stream2.handle};
  const UINT offsets[] = {0,0,0}; f.output.table.pfnSoSetTargets(f.device,3,0,buffers,offsets);
  f.output.table.pfnPsSetShader(f.device,{}); f.output.table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST); ok();
  ComPtr<ID3D11Device> backend; f.context->GetDevice(&backend);
  ComPtr<ID3D11Query> query; D3D11_QUERY_DESC queryDesc{D3D11_QUERY_SO_STATISTICS_STREAM1,0};
  CHECK(backend->CreateQuery(&queryDesc,&query) == S_OK);
  f.context->Begin(query.Get()); f.output.table.pfnDraw(f.device,3,0); ok(); f.context->End(query.Get());
  const auto statistics = queryResult<D3D11_QUERY_DATA_SO_STATISTICS>(f,query.Get()); CHECK(statistics.NumPrimitivesWritten == 3);
  std::array<std::vector<UINT>,3> results = {stream0.read(64),stream1.read(64),stream2.read(64)};
  for (UINT stream = 0; stream < 3; ++stream) for (UINT vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
    const UINT index = vertexIndex * strides[stream]/4;
    CHECK(results[stream][index] == vertexIndex + 100*stream && results[stream][index+1] == 900+stream);
    // The explicit stride leaves bytes outside SO's write window untouched.
    CHECK(results[stream][index+3] == poison[0]);
  }
  for (UINT stream = 0; stream < 3; ++stream) CHECK(results[stream].back() == poison[0]);
  f.output.table.pfnSoSetTargets(f.device,0,3,nullptr,nullptr); ok();
  // The same declarations may select a real rasterized stream.
  GraphicsState state(f); ColorTarget target(f,DXGI_FORMAT_R8G8B8A8_UNORM); target.bind();
  args.RasterizedStream = 0; GraphicsShader rasterized(f,ShaderStage::Geometry,geometry,&args);
  GraphicsShader pixel(f,ShaderStage::Pixel,"float4 main():SV_Target0{return float4(0,1,0,1);}"); rasterized.bind(); pixel.bind();
  f.output.table.pfnDraw(f.device,1,0); ok();
  CHECK(target.texture.pixel(7,7)[0] == 0xff00ff00 || target.texture.pixel(8,8)[0] == 0xff00ff00);
}
static void tessellation(Fixture& f) {
  using dxvk::umd::ShaderStage;
  const char* common = "struct V{float4 p:POSITION;};struct P{float e[3]:SV_TessFactor;float i:SV_InsideTessFactor;};";
  const std::string hull = std::string(common) + R"(
P constants(InputPatch<V,3> p,uint id:SV_PrimitiveID){P o;o.e[0]=4;o.e[1]=4;o.e[2]=4;o.i=4;return o;}
[domain("tri")][partitioning("integer")][outputtopology("triangle_cw")][outputcontrolpoints(3)][patchconstantfunc("constants")]
V main(InputPatch<V,3> p,uint id:SV_OutputControlPointID){return p[id];})";
  const std::string domain = std::string(common) + R"(
[domain("tri")]float4 main(P p,float3 uv:SV_DomainLocation,const OutputPatch<V,3> c):SV_Position{
return c[0].p*uv.x+c[1].p*uv.y+c[2].p*uv.z;})";
  GraphicsShader vertex(f,ShaderStage::Vertex,"struct V{float4 p:POSITION;};V main(uint id:SV_VertexID){V v;float2 xy=float2((id<<1)&2,id&2);v.p=float4(xy*float2(2,-2)+float2(-1,1),0,1);return v;}");
  GraphicsShader hs(f,ShaderStage::Hull,hull.c_str()); GraphicsShader ds(f,ShaderStage::Domain,domain.c_str());
  GraphicsShader pixel(f,ShaderStage::Pixel,"float4 main():SV_Target0{return float4(0,1,1,1);}");
  GraphicsState state(f); ColorTarget target(f,DXGI_FORMAT_R8G8B8A8_UNORM); target.bind();
  vertex.bind(); hs.bind(); ds.bind(); pixel.bind(); f.output.table.pfnGsSetShader(f.device,{});
  f.output.table.pfnIaSetTopology(f.device,D3D11_DDI_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST); ok();
  ComPtr<ID3D11Device> backend; f.context->GetDevice(&backend);
  ComPtr<ID3D11Query> query; D3D11_QUERY_DESC args{D3D11_QUERY_PIPELINE_STATISTICS,0}; CHECK(backend->CreateQuery(&args,&query) == S_OK);
  f.context->Begin(query.Get()); f.output.table.pfnDraw(f.device,3,0); ok(); f.context->End(query.Get());
  const auto data = queryResult<D3D11_QUERY_DATA_PIPELINE_STATISTICS>(f,query.Get());
  CHECK(data.HSInvocations && data.DSInvocations >= 3 && data.PSInvocations);
  CHECK(target.texture.pixel()[0] == 0xffffff00);
  f.output.table.pfnDsSetShader(f.device,{}); ok();
  f.output.table.pfnDraw(f.device,3,0); failure(E_INVALIDARG);
  f.output.table.pfnHsSetShader(f.device,{}); f.output.table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST); ok();
}
static void classInterfaces(Fixture& f) {
  using dxvk::umd::ShaderStage;
  const char* source = R"(
interface IOp{uint eval(uint x);};class Add:IOp{uint bias;uint eval(uint x){return x+bias;}};
class Multiply:IOp{uint factor;uint eval(uint x){return x*factor;}};
cbuffer Objects:register(b0){Add addObject;Multiply multiplyObject;};
IOp operation; RWStructuredBuffer<uint> dst:register(u0);
[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){dst[id.x]=operation.eval(id.x+2);})";
  GraphicsShader shader(f,ShaderStage::Compute,source);
  CHECK(shader.decoded.interfaceSlots == 1 && shader.decoded.interfaces.size() == 1);
  dxbc_spv::dxbc::Container container(shader.original->GetBufferPointer(),shader.original->GetBufferSize());
  dxbc_spv::dxbc::InterfaceChunk metadata(container.getInterfaceChunk()); CHECK(metadata);
  const auto classes = metadata.getClassTypes(); const auto slots = metadata.getInterfaceSlots();
  UINT addTable = UINT(-1), multiplyTable = UINT(-1);
  for (auto type = classes.first; type != classes.second; ++type) for (auto slot = slots.first; slot != slots.second; ++slot)
    for (const auto& entry : slot->entries) if (entry.typeId == type->id) {
      if (type->name == "Add") addTable = entry.tableId;
      if (type->name == "Multiply") multiplyTable = entry.tableId;
    }
  CHECK(addTable != UINT(-1) && multiplyTable != UINT(-1) && addTable != multiplyTable);
  std::array<UINT,16> constants{}; constants[4] = 11; constants[8] = 7;
  Buffer cb(f,64,D3D10_DDI_BIND_CONSTANT_BUFFER,0,0,constants.data());
  Buffer destination(f,16,D3D11_DDI_BIND_UNORDERED_ACCESS,D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED,4); Uav view(f,destination,4,DXGI_FORMAT_UNKNOWN);
  auto& table = f.output.table;
  table.pfnCsSetConstantBuffers(f.device,0,1,&cb.handle); table.pfnCsSetUnorderedAccessViews(f.device,0,1,&view.handle,nullptr);
  D3D11DDIARG_POINTERDATA pointer{}; pointer.uCBID = 0; pointer.uCBOffset = 16;
  table.pfnCsSetShaderWithIfaces(f.device,shader.handle,1,&addTable,&pointer); ok();
  ComPtr<ID3D11ComputeShader> observed; ComPtr<ID3D11ClassInstance> instance; UINT count = 1;
  f.context->CSGetShader(&observed,&instance,&count); CHECK(observed && instance && count == 1);
  D3D11_CLASS_INSTANCE_DESC desc{}; instance->GetDesc(&desc); CHECK(desc.ConstantBuffer == 0 && desc.BaseConstantBufferOffset == 1);
  table.pfnDispatch(f.device,4,1,1); ok(); CHECK((destination.read(4) == std::vector<UINT>{13,14,15,16}));
  pointer.uCBOffset = 32; table.pfnCsSetShaderWithIfaces(f.device,shader.handle,1,&multiplyTable,&pointer); ok();
  table.pfnDispatch(f.device,4,1,1); ok(); CHECK((destination.read(4) == std::vector<UINT>{14,21,28,35}));
  // Failure on an unbound shader retires only that handle; a successful old
  // class binding must still execute with its own live instances.
  GraphicsShader invalid(f,ShaderStage::Compute,source);
  pointer.uReserved = 1;
  table.pfnCsSetShaderWithIfaces(f.device,invalid.handle,1,&multiplyTable,&pointer); invalid.alive = false; failure(E_INVALIDARG);
  table.pfnDispatch(f.device,4,1,1); ok(); CHECK((destination.read(4) == std::vector<UINT>{14,21,28,35}));
  table.pfnCsSetShaderWithIfaces(f.device,shader.handle,1,&multiplyTable,&pointer); shader.alive = false; failure(E_INVALIDARG);
  observed.Reset(); instance.Reset(); count = 1;
  f.context->CSGetShader(&observed,&instance,&count); CHECK(!observed && !instance && count == 0);
  table.pfnDispatch(f.device,1,1,1); failure(E_INVALIDARG);
  table.pfnCsSetUnorderedAccessViews(f.device,0,1,nullptr,nullptr); failure(E_INVALIDARG);
  const D3D11DDI_HUNORDEREDACCESSVIEW empty{}; table.pfnCsSetUnorderedAccessViews(f.device,0,1,&empty,nullptr); ok();
}

int main() {
  callerThread = GetCurrentThreadId();
  independentBlend10_1();
  {
    Fixture f;
    auto& table = f.output.table;
    const auto before = backendCalls; const auto snapshot = f.output;
    CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, f.device, {&f.callbacks}, &f.callbacks, &table, D3D_FEATURE_LEVEL_11_0) == E_INVALIDARG);
    CHECK(backendCalls == before && !std::memcmp(&snapshot, &f.output, sizeof(snapshot)));
    const UINT callbacksBefore = alternateErrors;
    f.callbacks.pfnSetErrorCb = alternateError;
    table.pfnCsSetSamplers(f.device, UINT(-1), 1, nullptr); failure(E_INVALIDARG); CHECK(alternateErrors == callbacksBefore + 1);
    Storage invalidShader(table.pfnCalcPrivateTessellationShaderSize(f.device, nullptr, nullptr));
    table.pfnCreateHullShader(f.device, nullptr, {invalidShader.data()}, {}, nullptr); failure(E_INVALIDARG); CHECK(invalidShader.empty()); invalidShader.check();
    table.pfnRelocateDeviceFuncs(f.device, &table); ok();
    textureMinLod(f);
    computeAndCounters(f);
    table.pfnCsSetShader(f.device, {}); ok();
    graphicsSm5(f);
    geometryStreams(f);
    tessellation(f);
    classInterfaces(f);
  }
  {
    Fixture low(D3D_FEATURE_LEVEL_10_0);
    Storage rejected(low.output.table.pfnCalcPrivateShaderSize(low.device, nullptr, nullptr));
    auto tokens = compile("[numthreads(1,1,1)]void main(){}");
    low.output.table.pfnCreateComputeShader(low.device, tokens.data(), {rejected.data()}, {}); failure(DXGI_ERROR_UNSUPPORTED);
    CHECK(rejected.empty()); rejected.check();
  }
  Storage failed(VioGpuDxvkPrivateDeviceSize()); D3D10DDI_HDEVICE device{failed.data()};
  D3D11DDI_CORELAYER_DEVICECALLBACKS callbacks{}; callbacks.pfnSetErrorCb = error;
  TableOutput<D3D11DDI_DEVICEFUNCS> output{}; std::memset(&output.table, 0xcc, sizeof(output.table)); const auto snapshot = output;
  backendResult = S_FALSE;
  CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&callbacks}, &callbacks, &output.table, D3D_FEATURE_LEVEL_11_0) == E_FAIL);
  CHECK(!std::memcmp(&snapshot, &output, sizeof(output))); failed.check(); backendResult = S_OK;
  CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&callbacks}, &callbacks, &output.table, D3D_FEATURE_LEVEL_11_0) == S_OK);
  createdContext.Reset(); output.table.pfnDestroyDevice(device); failed.check();
  std::printf("typed D3D10.1/D3D11 fixture PASS checks=%u callbacks=%u SM5 graphics/streams/tessellation/classes WARP controls; native Turnip/runtime acceptance remains gated\n", checks, errors);
}
