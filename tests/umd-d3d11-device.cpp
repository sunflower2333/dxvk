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
#include <utility>

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
static void (*onError)() = nullptr;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "D3D11 DDI failure line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
static void ok() { CHECK(lastError == S_OK); }
static void failure(HRESULT expected) { CHECK(lastError == dxvk::umd::ddiResult(expected)); lastError = S_OK; }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(hr)); ++errors; lastError = hr;
  if (auto hook = std::exchange(onError, nullptr)) hook();
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
struct NativeQuery {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HQUERY handle;
  bool alive = true;
  NativeQuery(Fixture& f, D3D10DDI_QUERY type, void* external = nullptr)
  : fixture(f), storage(f.output.table.pfnCalcPrivateQuerySize(f.device, nullptr)), handle{external ? external : storage.data()} {
    const D3D10DDIARG_CREATEQUERY args{type,0};
    f.output.table.pfnCreateQuery(f.device,&args,handle,{}); ok(); storage.check();
  }
  ~NativeQuery() { if (alive) fixture.output.table.pfnDestroyQuery(fixture.device,handle); storage.check(); }
  void begin() { fixture.output.table.pfnQueryBegin(fixture.device,handle); ok(); }
  void end() { fixture.output.table.pfnQueryEnd(fixture.device,handle); ok(); }
  template<typename Data> Data result() {
    fixture.output.table.pfnFlush(fixture.device); ok();
    struct Guard { UINT64 before; Data data; UINT64 after; } guarded{};
    guarded.before = guarded.after = Storage::canary;
    std::memset(&guarded.data,0xcd,sizeof(guarded.data));
    std::array<unsigned char,sizeof(Data)> untouched{};
    std::memcpy(untouched.data(),&guarded.data,sizeof(Data));
    const ULONGLONG start = GetTickCount64();
    for (;;) {
      fixture.output.table.pfnQueryGetData(fixture.device,handle,&guarded.data,sizeof(Data),D3D10_DDI_GET_DATA_DO_NOT_FLUSH);
      CHECK(guarded.before == Storage::canary && guarded.after == Storage::canary);
      if (lastError == S_OK) return guarded.data;
      failure(DXGI_DDI_ERR_WASSTILLDRAWING);
      CHECK(!std::memcmp(&guarded.data,untouched.data(),sizeof(Data)));
      CHECK(GetTickCount64() - start < 10000); SwitchToThread();
    }
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
  NativeQuery pipeline(f,D3D11DDI_QUERY_PIPELINESTATS); pipeline.begin();
  table.pfnDispatch(f.device, 4, 1, 1); ok();
  pipeline.end();
  const auto counters = pipeline.result<D3D11_DDI_QUERY_DATA_PIPELINE_STATISTICS>();
  CHECK(counters.CSInvocations == 16 && !counters.HSInvocations && !counters.DSInvocations && !counters.IAVertices);
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

template<typename Table>
static void inputLayoutCapacity(Table& table, D3D10DDI_HDEVICE device,
    ID3D11DeviceContext* context, UINT capacity) {
  std::vector<D3D10DDIARG_INPUT_ELEMENT_DESC> elements(capacity + 1);
  for (UINT i = 0; i < elements.size(); ++i)
    elements[i] = {i, 0, DXGI_FORMAT_R32_FLOAT, D3D10_DDI_INPUT_PER_VERTEX_DATA, 0, i};
  Storage storage(table.pfnCalcPrivateElementLayoutSize(device, nullptr));
  std::fill(storage.words.begin(), storage.words.end() - 1, 0xccccccccccccccccull);
  const auto untouched = storage.words;
  const D3D10DDI_HELEMENTLAYOUT layout{storage.data()};
  D3D10DDIARG_CREATEELEMENTLAYOUT args{elements.data(), capacity + 1};
  table.pfnCreateElementLayout(device, &args, layout, {}); failure(E_INVALIDARG);
  args.NumElements = 1; elements[0].InputRegister = capacity;
  table.pfnCreateElementLayout(device, &args, layout, {}); failure(E_INVALIDARG);
  elements[0].InputRegister = 0; elements[0].InputSlot = capacity;
  table.pfnCreateElementLayout(device, &args, layout, {}); failure(E_INVALIDARG);
  CHECK(storage.words == untouched);
  elements[0].InputSlot = 0; args.NumElements = capacity;
  table.pfnCreateElementLayout(device, &args, layout, {}); ok();
  table.pfnIaSetInputLayout(device, layout); ok();
  ComPtr<ID3D11InputLayout> observed; context->IAGetInputLayout(&observed); CHECK(observed);
  const D3D10DDI_HRESOURCE empty[2]{};
  const UINT stride[2] = {4, 4}, offset[2]{};
  table.pfnIaSetVertexBuffers(device, capacity - 1, 1, empty, stride, offset); ok();
  table.pfnIaSetVertexBuffers(device, capacity, 1, empty, stride, offset); failure(E_INVALIDARG);
  table.pfnIaSetVertexBuffers(device, capacity - 1, 2, empty, stride, offset); failure(E_INVALIDARG);
  CHECK(storage.words == untouched);
  table.pfnDestroyElementLayout(device, layout); ok();
  observed.Reset(); context->IAGetInputLayout(&observed); CHECK(!observed);
  storage.check();
}

static void independentBlend10_1() {
  Storage storage(VioGpuDxvkPrivateDeviceSize()); D3D10DDI_HDEVICE device{storage.data()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks{}; callbacks.pfnSetErrorCb = error;
  TableOutput<D3D10_1DDI_DEVICEFUNCS> output{};
  CHECK(VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&callbacks}, &callbacks, &output.table) == S_OK);
  auto context = createdContext; createdContext.Reset();
  inputLayoutCapacity(output.table, device, context.Get(), D3D10_1_VS_INPUT_REGISTER_COUNT);
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
static UINT semanticRegister(const GraphicsShader& shader, bool input, const char* name, UINT index = 0) {
  dxbc_spv::dxbc::Container container(shader.original->GetBufferPointer(),shader.original->GetBufferSize()); CHECK(container);
  dxbc_spv::dxbc::Signature signature(input ? container.getInputSignatureChunk() : container.getOutputSignatureChunk()); CHECK(signature);
  for (const auto& entry : signature)
    if (!std::strcmp(entry.getSemanticName(),name) && entry.getSemanticIndex() == index) {
      CHECK(entry.getRegisterIndex() >= 0); return UINT(entry.getRegisterIndex());
    }
  CHECK(false); return 0;
}
struct NativeLayout {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HELEMENTLAYOUT handle;
  bool alive = true;
  NativeLayout(Fixture& f, const std::vector<D3D10DDIARG_INPUT_ELEMENT_DESC>& elements, void* external = nullptr)
  : fixture(f), storage(f.output.table.pfnCalcPrivateElementLayoutSize(f.device,nullptr)), handle{external ? external : storage.data()} {
    const D3D10DDIARG_CREATEELEMENTLAYOUT args{elements.data(),UINT(elements.size())};
    f.output.table.pfnCreateElementLayout(f.device,&args,handle,{}); ok(); storage.check();
  }
  ~NativeLayout() { if (alive) fixture.output.table.pfnDestroyElementLayout(fixture.device,handle); storage.check(); }
  void bind() { fixture.output.table.pfnIaSetInputLayout(fixture.device,handle); ok(); }
};

static void inputAssembler11(Fixture& f) {
  using dxvk::umd::ShaderStage;
  auto& table = f.output.table;
  const char* vertexSource = R"(
struct I{float4 color:COLOR0;float2 uv:UV0;uint2 u:U0;int2 s:S0;uint4 packed:PACKED0;uint step:STEP0;uint fixedValue:FIXED0;};
struct O{float4 p:SV_Position;uint4 value:DATA0;uint4 ids:DATA1;};
O main(I i,uint vertex:SV_VertexID,uint instance:SV_InstanceID){O o;o.p=float4(0,0,0,1);
o.value=uint4(uint(i.color.x*255+0.5),uint(i.uv.x*16),i.u.x+i.packed.x,uint(i.s.x+128));
o.ids=uint4(i.step,i.fixedValue,vertex,instance);return o;})";
  const char* geometrySource = R"(
struct O{float4 p:SV_Position;uint4 value:DATA0;uint4 ids:DATA1;};
[maxvertexcount(1)]void main(point O input[1],inout PointStream<O> output){output.Append(input[0]);})";
  GraphicsShader vertex(f,ShaderStage::Vertex,vertexSource);
  auto tokens = compile(geometrySource,"gs_5_0"); dxvk::umd::ShaderCode11 decoded;
  CHECK(dxvk::umd::decodeShader11(ShaderStage::Geometry,tokens.data(),tokens.size(),decoded));
  // Preserve semantic order in the independent public control below.
  GraphicsShader geometrySignature(f,ShaderStage::Geometry,geometrySource);
  const D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY declarations[] = {
    {0,0,semanticRegister(geometrySignature,false,"DATA",0),15},
    {0,0,semanticRegister(geometrySignature,false,"DATA",1),15}};
  const UINT outputStride = 32;
  D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT stream{};
  stream.pOutputStreamDecl = declarations; stream.NumEntries = 2;
  stream.BufferStridesInBytes = &outputStride; stream.NumStrides = 1; stream.RasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
  GraphicsShader geometry(f,ShaderStage::Geometry,geometrySource,&stream);
  const char* names[] = {"COLOR","UV","U","S","PACKED","STEP","FIXED"};
  const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R8G8_UINT,DXGI_FORMAT_R8G8_SINT,DXGI_FORMAT_R10G10B10A2_UINT,DXGI_FORMAT_R16_UINT,DXGI_FORMAT_R8_UINT};
  std::vector<D3D10DDIARG_INPUT_ELEMENT_DESC> elements;
  std::array<D3D11_INPUT_ELEMENT_DESC,7> publicElements{};
  for (UINT i = 0; i < 7; ++i) {
    const UINT slot = i < 5 ? 0 : i == 5 ? 1 : 31;
    const UINT offset = i && i < 5 ? D3D11_APPEND_ALIGNED_ELEMENT : i == 6 ? 1 : 0;
    const auto classification = i < 5 ? D3D10_DDI_INPUT_PER_VERTEX_DATA : D3D10_DDI_INPUT_PER_INSTANCE_DATA;
    const UINT step = i == 5 ? 2 : 0;
    elements.push_back({slot,offset,formats[i],classification,step,semanticRegister(vertex,true,names[i])});
    publicElements[i] = {names[i],0,formats[i],slot,offset,D3D11_INPUT_CLASSIFICATION(classification),step};
  }
  NativeLayout layout(f,elements); layout.bind();
  struct PackedVertex { BYTE color[4]; UINT16 uv[2]; BYTE u[2]; int8_t s[2]; UINT packed; };
  static_assert(sizeof(PackedVertex) == 16);
  const PackedVertex data[] = {{{31,0,0,255},{0x3c00,0},{7,0},{-4,0},0xc038142bu},
    {{47,0,0,255},{0x4000,0},{11,0},{-8,0},0xc038145bu},{{99,0,0,255},{0x4200,0},{13,0},{-16,0},0xc03814f7u},
    {{173,0,0,255},{0x4400,0},{17,0},{-32,0},0xc0381537u}};
  const UINT16 instances[] = {0xffff,100,200,300,400,500,600,700,800};
  const BYTE fixed[] = {0xee,0xbb,90};
  const UINT16 indices[] = {0xffff,2,0,1,0xffff};
  Buffer vertices(f,sizeof(data),D3D10_DDI_BIND_VERTEX_BUFFER,0,0,data);
  Buffer rates(f,sizeof(instances),D3D10_DDI_BIND_VERTEX_BUFFER,0,0,instances);
  Buffer constant(f,sizeof(fixed),D3D10_DDI_BIND_VERTEX_BUFFER,0,0,fixed);
  Buffer index(f,sizeof(indices),D3D10_DDI_BIND_INDEX_BUFFER,0,0,indices);
  const D3D10DDI_HRESOURCE buffers[] = {vertices.handle,rates.handle,constant.handle};
  const UINT strides[] = {sizeof(PackedVertex),2,0}, offsets[] = {0,2,1};
  table.pfnIaSetVertexBuffers(f.device,0,2,buffers,strides,offsets);
  table.pfnIaSetVertexBuffers(f.device,31,1,&constant.handle,&strides[2],&offsets[2]);
  table.pfnIaSetIndexBuffer(f.device,index.handle,DXGI_FORMAT_R16_UINT,2);
  table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST);
  table.pfnPsSetShader(f.device,{}); table.pfnHsSetShader(f.device,{}); table.pfnDsSetShader(f.device,{});
  vertex.bind(); geometry.bind(); ok();
  std::array<UINT,128> poison; poison.fill(0xa5a55a5a);
  Buffer captured(f,512,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  const UINT start = 0; table.pfnSoSetTargets(f.device,1,3,&captured.handle,&start); ok();
  NativeQuery statistics(f,D3D11DDI_QUERY_PIPELINESTATS); statistics.begin();
  table.pfnDrawIndexedInstanced(f.device,2,4,1,1,3); ok(); statistics.end();
  const auto pipeline = statistics.result<D3D11_DDI_QUERY_DATA_PIPELINE_STATISTICS>();
  CHECK(pipeline.IAVertices == 8 && pipeline.IAPrimitives == 8 && pipeline.GSInvocations == 8 && !pipeline.CSInvocations);
  auto result = captured.read(128);
  for (UINT instance = 0; instance < 4; ++instance) for (UINT point = 0; point < 2; ++point) {
    const UINT location = (instance*2+point)*8;
    const PackedVertex& value = data[point+1];
    CHECK(result[location] == value.color[0] && result[location+1] == (point+2)*16);
    CHECK(result[location+2] == value.u[0]+(value.packed & 1023) && result[location+3] == UINT(value.s[0]+128));
    CHECK(result[location+5] == 90);
  }
  for (UINT i = 64; i < 128; ++i) CHECK(result[i] == poison[i]);

  // Compare all IDs and divisor/start-instance fetches against the original
  // app blobs and exact public API parameters, independently of native
  // signature reconstruction and DDI argument forwarding.
  ComPtr<ID3D11Device> backend; f.context->GetDevice(&backend);
  ComPtr<ID3D11VertexShader> referenceVertex; ComPtr<ID3D11GeometryShader> referenceGeometry;
  ComPtr<ID3D11InputLayout> referenceLayout;
  CHECK(backend->CreateVertexShader(vertex.original->GetBufferPointer(),vertex.original->GetBufferSize(),nullptr,&referenceVertex) == S_OK);
  CHECK(backend->CreateInputLayout(publicElements.data(),UINT(publicElements.size()),vertex.original->GetBufferPointer(),vertex.original->GetBufferSize(),&referenceLayout) == S_OK);
  const D3D11_SO_DECLARATION_ENTRY publicStream[] = {{0,"DATA",0,0,4,0},{0,"DATA",1,0,4,0}};
  CHECK(backend->CreateGeometryShaderWithStreamOutput(geometry.original->GetBufferPointer(),geometry.original->GetBufferSize(),
    publicStream,2,&outputStride,1,D3D11_SO_NO_RASTERIZED_STREAM,nullptr,&referenceGeometry) == S_OK);
  Buffer control(f,512,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  table.pfnSoSetTargets(f.device,1,0,&control.handle,&start); ok();
  ComPtr<ID3D11Buffer> referenceBuffer; f.context->SOGetTargets(1,&referenceBuffer); CHECK(referenceBuffer);
  f.context->VSSetShader(referenceVertex.Get(),nullptr,0); f.context->GSSetShader(referenceGeometry.Get(),nullptr,0);
  f.context->IASetInputLayout(referenceLayout.Get());
  f.context->DrawIndexedInstanced(2,4,1,1,3);
  CHECK(control.read(128) == result);
  layout.bind(); vertex.bind(); geometry.bind();
  table.pfnSoSetTargets(f.device,1,0,&captured.handle,&start); ok();

  // A failed foreign bind and a foreign resource tail leave the entire IA
  // state intact; misaligned index offsets do not clear a valid index bind.
  Fixture foreign; NativeLayout foreignLayout(foreign,elements);
  ComPtr<ID3D11InputLayout> before, after; f.context->IAGetInputLayout(&before);
  table.pfnIaSetInputLayout(f.device,foreignLayout.handle); failure(E_INVALIDARG);
  f.context->IAGetInputLayout(&after); CHECK(after.Get() == before.Get());
  Buffer foreignBuffer(foreign,4,D3D10_DDI_BIND_VERTEX_BUFFER);
  const D3D10DDI_HRESOURCE mixed[] = {vertices.handle,foreignBuffer.handle};
  const UINT mixedStrides[] = {sizeof(PackedVertex),4}, mixedOffsets[] = {0,0};
  std::array<ComPtr<ID3D11Buffer>,3> oldBuffers, newBuffers;
  std::array<ID3D11Buffer*,3> raw{}; std::array<UINT,3> oldStrides{},oldOffsets{},newStrides{},newOffsets{};
  f.context->IAGetVertexBuffers(0,3,raw.data(),oldStrides.data(),oldOffsets.data());
  for (UINT i = 0; i < 3; ++i) oldBuffers[i].Attach(raw[i]);
  table.pfnIaSetVertexBuffers(f.device,0,2,mixed,mixedStrides,mixedOffsets); failure(E_INVALIDARG);
  f.context->IAGetVertexBuffers(0,3,raw.data(),newStrides.data(),newOffsets.data());
  for (UINT i = 0; i < 3; ++i) { newBuffers[i].Attach(raw[i]); CHECK(newBuffers[i].Get() == oldBuffers[i].Get()); }
  CHECK(oldStrides == newStrides && oldOffsets == newOffsets);
  table.pfnIaSetIndexBuffer(f.device,index.handle,DXGI_FORMAT_R16_UINT,1); failure(E_INVALIDARG);
  table.pfnDrawIndexedInstanced(f.device,2,4,1,1,3); ok(); CHECK(captured.read(128) == result);
  const UINT wideIndices[] = {0xffffffff,2,0,1,0xffffffff};
  Buffer wideIndex(f,sizeof(wideIndices),D3D10_DDI_BIND_INDEX_BUFFER,0,0,wideIndices);
  table.pfnIaSetIndexBuffer(f.device,wideIndex.handle,DXGI_FORMAT_R32_UINT,4); ok();
  table.pfnSoSetTargets(f.device,1,0,&captured.handle,&start);
  table.pfnDrawIndexedInstanced(f.device,2,4,1,1,3); ok(); CHECK(captured.read(128) == result);
  table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY(32)); failure(E_INVALIDARG);
  D3D11_PRIMITIVE_TOPOLOGY topology{}; f.context->IAGetPrimitiveTopology(&topology); CHECK(topology == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  table.pfnSoSetTargets(f.device,0,4,nullptr,nullptr); table.pfnIaSetInputLayout(f.device,{}); ok();
}

static Fixture* reentrantFixture;
static D3D10DDI_HQUERY reentrantQuery{};
static D3D10DDI_HELEMENTLAYOUT reentrantLayout{};
static void* reentrantPage;
static void retireChildInErrorCallback() {
  CHECK(reentrantFixture && reentrantPage);
  const auto before = errors;
  if (reentrantQuery.pDrvPrivate)
    reentrantFixture->output.table.pfnDestroyQuery(reentrantFixture->device,reentrantQuery);
  else reentrantFixture->output.table.pfnDestroyElementLayout(reentrantFixture->device,reentrantLayout);
  CHECK(errors == before);
  DWORD protection = 0;
  CHECK(VirtualProtect(reentrantPage,4096,PAGE_NOACCESS,&protection));
  // Future errors must use this live, updated slot on the original caller.
  reentrantFixture->callbacks.pfnSetErrorCb = error;
}
static void queryAndLayoutHandles(Fixture& f) {
  auto& table = f.output.table;
  D3D10DDIARG_CREATEQUERY args{D3D10DDI_QUERY(0x7fffffff),0};
  Storage failedQuery(table.pfnCalcPrivateQuerySize(f.device,&args));
  std::fill(failedQuery.words.begin(),failedQuery.words.end()-1,0xccccccccccccccccull);
  const auto untouched = failedQuery.words;
  D3D10DDI_HQUERY queryKey{failedQuery.data()};
  table.pfnCreateQuery(f.device,&args,queryKey,{}); failure(E_INVALIDARG);
  CHECK(failedQuery.words == untouched);
  args.Query = D3D10DDI_QUERY_EVENT;
  table.pfnCreateQuery(f.device,&args,queryKey,{}); ok();
  table.pfnCreateQuery(f.device,&args,queryKey,{}); failure(E_INVALIDARG);
  CHECK(failedQuery.words == untouched);
  table.pfnQueryEnd(f.device,queryKey); ok();
  table.pfnDestroyQuery(f.device,queryKey); ok(); failedQuery.check();

  Fixture foreign;
  NativeQuery foreignQuery(foreign,D3D10DDI_QUERY_EVENT);
  UINT64 value = 0xabcdef1276543298ull;
  table.pfnQueryBegin(f.device,foreignQuery.handle); failure(E_INVALIDARG);
  table.pfnQueryEnd(f.device,foreignQuery.handle); failure(E_INVALIDARG);
  table.pfnQueryGetData(f.device,foreignQuery.handle,&value,sizeof(value),0); failure(E_INVALIDARG);
  CHECK(value == 0xabcdef1276543298ull);
  table.pfnDestroyQuery(f.device,foreignQuery.handle); failure(E_INVALIDARG);
  foreignQuery.end(); CHECK(foreignQuery.result<BOOL>() == TRUE);

  void* page = VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); CHECK(page);
  NativeQuery event(f,D3D10DDI_QUERY_EVENT,page); event.end(); CHECK(event.result<BOOL>() == TRUE);
  table.pfnQueryGetData(f.device,event.handle,nullptr,0,0); ok();
  table.pfnQueryBegin(f.device,event.handle); failure(E_INVALIDARG);
  table.pfnDestroyQuery(f.device,event.handle); event.alive = false; ok();
  DWORD protection = 0; CHECK(VirtualProtect(page,4096,PAGE_NOACCESS,&protection));
  table.pfnQueryBegin(f.device,event.handle); failure(E_INVALIDARG);
  table.pfnQueryEnd(f.device,event.handle); failure(E_INVALIDARG);
  table.pfnQueryGetData(f.device,event.handle,&value,sizeof(value),0); failure(E_INVALIDARG);
  CHECK(value == 0xabcdef1276543298ull);
  table.pfnSetPredication(f.device,event.handle,FALSE); failure(E_INVALIDARG);
  table.pfnDestroyQuery(f.device,event.handle); failure(E_INVALIDARG);
  CHECK(VirtualProtect(page,4096,PAGE_READWRITE,&protection));
  NativeQuery reused(f,D3D10DDI_QUERY_OCCLUSION,page);
  table.pfnQueryGetData(f.device,reused.handle,&value,sizeof(value),0); failure(E_INVALIDARG);
  reused.end(); CHECK(reused.result<UINT64>() == 0); // Legal implicit empty interval.
  reentrantFixture = &f; reentrantQuery = reused.handle; reentrantPage = page;
  f.callbacks.pfnSetErrorCb = alternateError;
  const auto alternateBefore = alternateErrors;
  onError = retireChildInErrorCallback; reused.alive = false;
  table.pfnQueryGetData(f.device,reused.handle,&value,4,0); failure(E_INVALIDARG);
  CHECK(value == 0xabcdef1276543298ull && alternateErrors == alternateBefore+1 && !onError);
  table.pfnQueryEnd(f.device,reused.handle); failure(E_INVALIDARG);
  CHECK(alternateErrors == alternateBefore+1);
  CHECK(VirtualProtect(page,4096,PAGE_READWRITE,&protection));

  std::vector<D3D10DDIARG_INPUT_ELEMENT_DESC> elements{{0,0,DXGI_FORMAT_R16_FLOAT,D3D10_DDI_INPUT_PER_VERTEX_DATA,0,0}};
  D3D10DDIARG_CREATEELEMENTLAYOUT layoutArgs{elements.data(),UINT(elements.size())};
  Storage failedLayout(table.pfnCalcPrivateElementLayoutSize(f.device,&layoutArgs));
  std::fill(failedLayout.words.begin(),failedLayout.words.end()-1,0xccccccccccccccccull);
  const auto untouchedLayout = failedLayout.words;
  elements[0].AlignedByteOffset = 1;
  table.pfnCreateElementLayout(f.device,&layoutArgs,{failedLayout.data()},{}); failure(E_INVALIDARG);
  CHECK(failedLayout.words == untouchedLayout);
  elements[0].AlignedByteOffset = 0;
  table.pfnCreateElementLayout(f.device,&layoutArgs,{failedLayout.data()},{}); ok();
  table.pfnIaSetInputLayout(f.device,{failedLayout.data()}); ok();
  table.pfnCreateElementLayout(f.device,&layoutArgs,{failedLayout.data()},{}); failure(E_INVALIDARG);
  CHECK(failedLayout.words == untouchedLayout);
  table.pfnDestroyElementLayout(f.device,{failedLayout.data()}); ok(); failedLayout.check();
  NativeLayout layout(f,elements,page); layout.bind();
  NativeLayout foreignLayout(foreign,elements);
  ComPtr<ID3D11InputLayout> before, after; f.context->IAGetInputLayout(&before);
  table.pfnDestroyElementLayout(f.device,foreignLayout.handle); failure(E_INVALIDARG);
  table.pfnIaSetInputLayout(f.device,foreignLayout.handle); failure(E_INVALIDARG);
  f.context->IAGetInputLayout(&after); CHECK(before.Get() == after.Get());
  reentrantQuery = {}; reentrantLayout = layout.handle;
  f.callbacks.pfnSetErrorCb = alternateError;
  const auto beforeLayoutCallback = alternateErrors;
  onError = retireChildInErrorCallback; layout.alive = false;
  table.pfnCreateElementLayout(f.device,&layoutArgs,layout.handle,{}); failure(E_INVALIDARG);
  CHECK(alternateErrors == beforeLayoutCallback+1 && !onError);
  f.context->IAGetInputLayout(&after); CHECK(!after);
  table.pfnIaSetInputLayout(f.device,layout.handle); failure(E_INVALIDARG);
  table.pfnDestroyElementLayout(f.device,layout.handle); failure(E_INVALIDARG);
  CHECK(alternateErrors == beforeLayoutCallback+1);
  CHECK(VirtualFree(page,0,MEM_RELEASE));
  reentrantFixture = nullptr; reentrantPage = nullptr; reentrantLayout = {};
}
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
static void geometryStreams(Fixture& f) {
  using dxvk::umd::ShaderStage;
  GraphicsShader vertex(f,ShaderStage::Vertex,"struct V{float4 p:SV_Position;uint id:DATA0;};V main(uint id:SV_VertexID){V v;v.p=float4(0,0,0,1);v.id=id;return v;}");
  const char* geometry = R"(
struct V{float4 p:SV_Position;uint id:DATA0;};struct O{float4 p:SV_Position;uint2 value:DATA0;};
[maxvertexcount(10)]void main(point V v[1],inout PointStream<O> s0,inout PointStream<O> s1,inout PointStream<O> s2,inout PointStream<O> s3){
O o;o.p=v[0].p;o.value=uint2(v[0].id,900);s0.Append(o);
for(uint j=0;j<2;++j){o.value=uint2(v[0].id+100,901+j);s1.Append(o);}
for(uint k=0;k<3;++k){o.value=uint2(v[0].id+200,902+k);s2.Append(o);}
for(uint l=0;l<4;++l){o.value=uint2(v[0].id+300,903+l);s3.Append(o);}})";
  auto tokens = compile(geometry,"gs_5_0"); dxvk::umd::ShaderCode11 decoded;
  CHECK(dxvk::umd::decodeShader11(ShaderStage::Geometry,tokens.data(),tokens.size(),decoded));
  std::array<D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY,5> entries{};
  for (UINT stream = 0; stream < 4; ++stream) {
    bool found = false;
    for (const auto& entry : decoded.outputs) if (entry.stream == stream && !entry.systemValue) {
      CHECK(!found && entry.mask == 3); found = true; entries[stream] = {stream,stream,entry.registerIndex,entry.mask};
    }
    CHECK(found);
  }
  entries[4] = {0,0,D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP,1};
  const UINT strides[] = {32,16,24,16};
  D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT args{}; args.pOutputStreamDecl = entries.data(); args.NumEntries = UINT(entries.size());
  args.BufferStridesInBytes = strides; args.NumStrides = 4; args.RasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
  GraphicsShader shader(f,ShaderStage::Geometry,geometry,&args); vertex.bind(); shader.bind();
  std::array<UINT,64> poison; poison.fill(0xa5a55a5a);
  Buffer stream0(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  Buffer stream1(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  Buffer stream2(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  Buffer stream3(f,256,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  const D3D10DDI_HRESOURCE buffers[] = {stream0.handle,stream1.handle,stream2.handle,stream3.handle};
  const UINT offsets[] = {0,0,0,0}; f.output.table.pfnSoSetTargets(f.device,4,0,buffers,offsets);
  f.output.table.pfnPsSetShader(f.device,{}); f.output.table.pfnIaSetTopology(f.device,D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST); ok();
  NativeQuery stats0(f,D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM0), stats1(f,D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM1);
  NativeQuery stats2(f,D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM2), stats3(f,D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM3);
  NativeQuery over0(f,D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM0), over1(f,D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM1);
  NativeQuery over2(f,D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM2), over3(f,D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM3);
  NativeQuery aggregate(f,D3D10DDI_QUERY_STREAMOVERFLOWPREDICATE);
  const std::array<NativeQuery*,9> queries = {&stats0,&stats1,&stats2,&stats3,&over0,&over1,&over2,&over3,&aggregate};
  for (auto query : queries) query->begin();
  f.output.table.pfnDraw(f.device,3,0); ok();
  for (auto query : queries) query->end();
  for (UINT stream = 0; stream < 4; ++stream) {
    const auto statistics = queries[stream]->result<D3D10_DDI_QUERY_DATA_SO_STATISTICS>();
    CHECK(statistics.NumPrimitivesWritten == 3*(stream+1) && statistics.PrimitivesStorageNeeded == statistics.NumPrimitivesWritten);
    CHECK(queries[4+stream]->result<BOOL>() == FALSE);
  }
  CHECK(aggregate.result<BOOL>() == FALSE);
  std::array<std::vector<UINT>,4> results = {stream0.read(64),stream1.read(64),stream2.read(64),stream3.read(64)};
  for (UINT stream = 0; stream < 4; ++stream) for (UINT vertexIndex = 0; vertexIndex < 3*(stream+1); ++vertexIndex) {
    const UINT index = vertexIndex * strides[stream]/4;
    CHECK(results[stream][index] == vertexIndex/(stream+1) + 100*stream && results[stream][index+1] == 900+stream+vertexIndex%(stream+1));
    // The explicit stride leaves bytes outside SO's write window untouched.
    CHECK(results[stream][index+2] == poison[0]);
    CHECK(results[stream][index+3] == poison[0]);
  }
  for (UINT stream = 0; stream < 4; ++stream) CHECK(results[stream].back() == poison[0]);
  Buffer tiny(f,24,D3D10_DDI_BIND_STREAM_OUTPUT,0,0,poison.data());
  const D3D10DDI_HRESOURCE overflowBuffers[] = {stream0.handle,stream1.handle,tiny.handle,stream3.handle};
  f.output.table.pfnSoSetTargets(f.device,4,0,overflowBuffers,offsets); ok();
  for (auto query : queries) query->begin();
  f.output.table.pfnDraw(f.device,3,0); ok();
  for (auto query : queries) query->end();
  CHECK(over0.result<BOOL>() == FALSE && over1.result<BOOL>() == FALSE);
  CHECK(over2.result<BOOL>() == TRUE && over3.result<BOOL>() == FALSE && aggregate.result<BOOL>() == TRUE);
  const auto overflowStatistics = stats2.result<D3D10_DDI_QUERY_DATA_SO_STATISTICS>();
  CHECK(overflowStatistics.NumPrimitivesWritten == 1 && overflowStatistics.PrimitivesStorageNeeded == 9);
  // Predication consumes the aggregate just read, and a bound predicate
  // cannot be ended or destroyed until it is unbound.
  f.output.table.pfnSetPredication(f.device,aggregate.handle,TRUE); ok();
  f.output.table.pfnQueryEnd(f.device,aggregate.handle); failure(E_INVALIDARG);
  f.output.table.pfnDestroyQuery(f.device,aggregate.handle); failure(E_INVALIDARG);
  f.output.table.pfnSetPredication(f.device,{},FALSE); ok();
  f.output.table.pfnSoSetTargets(f.device,0,4,nullptr,nullptr); ok();
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
  NativeQuery query(f,D3D11DDI_QUERY_PIPELINESTATS); query.begin();
  f.output.table.pfnDraw(f.device,3,0); ok(); query.end();
  const auto data = query.result<D3D11_DDI_QUERY_DATA_PIPELINE_STATISTICS>();
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
    inputLayoutCapacity(table, f.device, f.context.Get(), D3D11_VS_INPUT_REGISTER_COUNT);
    textureMinLod(f);
    computeAndCounters(f);
    table.pfnCsSetShader(f.device, {}); ok();
    inputAssembler11(f);
    queryAndLayoutHandles(f);
    graphicsSm5(f);
    geometryStreams(f);
    tessellation(f);
    classInterfaces(f);
  }
  {
    Fixture low(D3D_FEATURE_LEVEL_10_0);
    inputLayoutCapacity(low.output.table, low.device, low.context.Get(), D3D10_VS_INPUT_REGISTER_COUNT);
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
  std::printf("typed D3D10.1/D3D11 fixture PASS checks=%u callbacks=%u SM5 graphics/queries/packed IA/streams/tessellation/classes WARP controls; native Turnip/runtime acceptance remains gated\n", checks, errors);
}
