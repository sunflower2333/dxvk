// SPDX-License-Identifier: MIT
#pragma once
#include "umd-kmt-compute-transport.h"
#include "umd-kmt-callback-policy.h"
#include "umd_build_config.h"
#include <d3dcompiler.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dxvk::umd::probe::graphics {

constexpr UINT RuntimeVersion = (D3D11_0_DDI_BUILD_VERSION << 16) | DXGI_RESOLVE_SHARED_RESOURCE;
static_assert(IS_DXGI1_1_BASE_FUNCTIONS(D3D11_0_DDI_INTERFACE_VERSION, RuntimeVersion));
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D11_0_DDI_INTERFACE_VERSION, RuntimeVersion - 1));

struct Failure { const char* stage; HRESULT result; };
inline void require(bool value, const char* stage, HRESULT result = E_FAIL) {
  if (!value) throw Failure{stage, result};
}

template<typename T> struct Guarded {
  static_assert(alignof(T) <= 16 && 16 % alignof(T) == 0);
  std::array<unsigned char, 16> before;
  T value{};
  std::array<unsigned char, 16> after;
  Guarded() { before.fill(0xa5); after.fill(0xa5); }
  bool intact() const {
    static_assert(offsetof(Guarded, value) == 16);
    static_assert(offsetof(Guarded, after) == offsetof(Guarded, value) + sizeof(T));
    for (size_t i = 0; i < before.size(); ++i)
      if (before[i] != 0xa5 || after[i] != 0xa5) return false;
    return true;
  }
};

class Storage {
public:
  explicit Storage(SIZE_T size) : m_size(size) {
    require(size && size <= 16 * 1024 * 1024, "private-storage-size");
    m_data.reset(new unsigned char[size + 32]);
    std::memset(m_data.get(), 0xa5, size + 32);
    std::memset(data(), 0, size);
  }
  void* data() const { return m_data.get() + 16; }
  bool intact() const {
    for (SIZE_T i = 0; i < 16; ++i)
      if (m_data[i] != 0xa5 || m_data[m_size + 16 + i] != 0xa5) return false;
    return true;
  }
  bool zero() const {
    for (SIZE_T i = 0; i < m_size; ++i) if (m_data[i + 16]) return false;
    return true;
  }
private:
  SIZE_T m_size;
  std::unique_ptr<unsigned char[]> m_data;
};

inline void writeOriginal(const std::wstring& directory, const std::wstring& name, const void* data, size_t bytes) {
  require(bytes <= MAXDWORD, "original-output-size");
  const std::wstring path = directory + L"\\" + name;
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "original-output-create", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0;
  const BOOL saved = WriteFile(file, data, DWORD(bytes), &written, nullptr);
  const DWORD error = saved ? ERROR_SUCCESS : GetLastError();
  const BOOL closed = CloseHandle(file);
  const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
  require(saved && written == bytes && closed, "original-output-write", error ? HRESULT_FROM_WIN32(error)
    : closeError ? HRESULT_FROM_WIN32(closeError) : E_FAIL);
}

inline bool parseLuid(const WCHAR* text, LUID& luid) {
  if (std::wcslen(text) != sizeof(LUID) * 2) return false;
  unsigned char bytes[sizeof(LUID)]{};
  for (unsigned i = 0; i < sizeof(LUID) * 2; ++i) {
    const WCHAR c = text[i];
    unsigned digit;
    if (c >= L'0' && c <= L'9') digit = unsigned(c - L'0');
    else if (c >= L'a' && c <= L'f') digit = unsigned(c - L'a' + 10);
    else if (c >= L'A' && c <= L'F') digit = unsigned(c - L'A' + 10);
    else return false;
    if (!(i & 1)) bytes[i / 2] = static_cast<unsigned char>(digit << 4);
    else bytes[i / 2] |= static_cast<unsigned char>(digit);
  }
  std::memcpy(&luid, bytes, sizeof(luid));
  return luid.LowPart || luid.HighPart;
}

inline void printLuid(const LUID& luid) {
  const auto bytes = reinterpret_cast<const unsigned char*>(&luid);
  for (unsigned i = 0; i < sizeof(LUID); ++i) std::printf("%02x", unsigned(bytes[i]));
}

template<typename T> struct Release { void operator()(T* p) const { if (p) p->Release(); } };
template<typename T> using ComOwner = std::unique_ptr<T, Release<T>>;

struct ShaderSource {
  std::vector<UINT> tokens;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs, outputs;
  struct Output { UINT stream, index, mask; std::string semantic; UINT semanticIndex; };
  std::vector<Output> originalOutputs;
  UINT outputRegister(const char* semantic, UINT stream = 0) const {
    for (const auto& entry : originalOutputs)
      if (entry.stream == stream && entry.semanticIndex == 0 && entry.semantic == semantic) return entry.index;
    throw Failure{"original-output-semantic", E_INVALIDARG};
  }
};

// Reflect the immutable original FXC container, rather than constructing a
// signature from assumptions about register allocation. The DDI receives the
// original SHEX and exact SDK signature layout; no public D3D device is used.
inline ShaderSource compileShader(const std::wstring& directory, const WCHAR* name, const char* source,
    const char* profile, UINT programType) {
  writeOriginal(directory, std::wstring(name) + L".hlsl", source, std::strlen(source));
  ID3DBlob* binary = nullptr;
  ID3DBlob* diagnostics = nullptr;
  const HRESULT hr = D3DCompile(source, std::strlen(source), "typed11-real-kmt-graphics", nullptr, nullptr,
    "main", profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &binary, &diagnostics);
  const ComOwner<ID3DBlob> binaryOwner(binary), diagnosticsOwner(diagnostics);
  if (diagnostics) {
    writeOriginal(directory, std::wstring(name) + L".compiler.txt", diagnostics->GetBufferPointer(), diagnostics->GetBufferSize());
    std::fprintf(stderr, "%.*s\n", int(diagnostics->GetBufferSize()), static_cast<const char*>(diagnostics->GetBufferPointer()));
  }
  require(hr == S_OK && binary, "original-shader-compile", hr == S_OK ? E_FAIL : hr);
  const auto bytes = static_cast<const unsigned char*>(binary->GetBufferPointer());
  const size_t size = binary->GetBufferSize();
  writeOriginal(directory, std::wstring(name) + L".dxbc", bytes, size);
  require(size >= 32 && size <= MAXDWORD, "original-dxbc-size");
  auto word = [&](size_t offset) {
    require(offset <= size - 4, "original-dxbc-word-range");
    UINT value; std::memcpy(&value, bytes + offset, 4); return value;
  };
  require(word(0) == 0x43425844 && word(20) == 1 && word(24) == size, "original-dxbc-header");
  const UINT chunks = word(28);
  require(chunks && chunks <= (size - 32) / 4, "original-dxbc-chunks");
  ShaderSource shader;
  for (UINT i = 0; i < chunks; ++i) {
    const UINT offset = word(32 + size_t(i) * 4);
    require(offset >= 32 + size_t(chunks) * 4 && offset <= size - 8 && !(offset % 4), "original-dxbc-chunk-range");
    const UINT length = word(offset + 4);
    require(length <= size - offset - 8, "original-dxbc-chunk-size");
    if (word(offset) != 0x58454853) continue;
    require(shader.tokens.empty() && length >= 8 && !(length % 4), "original-shex-shape");
    shader.tokens.resize(length / 4); std::memcpy(shader.tokens.data(), bytes + offset + 8, length);
  }
  require(shader.tokens.size() >= 2 && shader.tokens[0] == ((programType << 16) | 0x50)
    && shader.tokens[1] == shader.tokens.size(), "original-shex-profile");
  writeOriginal(directory, std::wstring(name) + L".shex", shader.tokens.data(), shader.tokens.size() * sizeof(UINT));
  ID3D11ShaderReflection* reflection = nullptr;
  const HRESULT reflected = D3DReflect(bytes, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&reflection));
  const ComOwner<ID3D11ShaderReflection> reflectionOwner(reflection);
  require(reflected == S_OK && reflection, "original-shader-reflect", reflected);
  D3D11_SHADER_DESC description{};
  require(reflection->GetDesc(&description) == S_OK, "original-reflection-desc");
  for (unsigned output = 0; output < 2; ++output) {
    const UINT count = output ? description.OutputParameters : description.InputParameters;
    auto& target = output ? shader.outputs : shader.inputs;
    for (UINT i = 0; i < count; ++i) {
      D3D11_SIGNATURE_PARAMETER_DESC entry{};
      const HRESULT queried = output ? reflection->GetOutputParameterDesc(i, &entry) : reflection->GetInputParameterDesc(i, &entry);
      require(queried == S_OK && entry.Mask && entry.Mask < 16 && entry.Register < 32, "original-reflected-signature", queried);
      const UINT system = UINT(entry.SystemValueType) >= 64 ? 0 : UINT(entry.SystemValueType);
      target.push_back({D3D10_SB_NAME(system), entry.Register, entry.Mask});
      const UINT fields[] = {output, i, entry.Stream, entry.Register, entry.Mask, UINT(entry.SystemValueType),
        UINT(entry.ComponentType), entry.SemanticIndex};
      writeOriginal(directory, std::wstring(name) + (output ? L".output-" : L".input-") + std::to_wstring(i)
        + L".signature-u32", fields, sizeof(fields));
      writeOriginal(directory, std::wstring(name) + (output ? L".output-" : L".input-") + std::to_wstring(i)
        + L".semantic", entry.SemanticName, std::strlen(entry.SemanticName));
      if (output) shader.originalOutputs.push_back({entry.Stream, entry.Register, entry.Mask, entry.SemanticName, entry.SemanticIndex});
    }
  }
  std::printf("D3D11_KMT_ORIGINAL_SHADER profile=%s words=%zu inputs=%zu outputs=%zu\n",
    profile, shader.tokens.size(), shader.inputs.size(), shader.outputs.size());
  return shader;
}

class Session {
public:
  explicit Session(KmtComputeTransport& transport) : m_transport(transport) {
    transport.callbacks(m_adapterCallbacks, m_kernelCallbacks, m_coreCallbacks);
  }
  ~Session() { const auto hr = close(); if (hr != S_OK) std::printf("D3D11_KMT_GRAPHICS_CLEANUP hr=%08lx\n", static_cast<unsigned long>(hr)); }
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  void create() {
    require(VioGpuDxvkOpenAdapter10_2ForTest(nullptr) == E_INVALIDARG, "modern-open-null");
    D3D10DDIARG_OPENADAPTER opened{};
    opened.hRTAdapter = m_transport.runtimeAdapter(); opened.pAdapterCallbacks = &m_adapterCallbacks;
    opened.pAdapterFuncs_2 = &m_adapterFunctions.value;
    const HRESULT openedHr = VioGpuDxvkOpenAdapter10_2ForTest(&opened);
    require(openedHr == S_OK && opened.hAdapter.pDrvPrivate, "modern-adapter-open", openedHr);
    m_adapter = opened.hAdapter;
    require(m_adapterFunctions.intact() && m_adapterFunctions.value.pfnCalcPrivateDeviceSize
      && m_adapterFunctions.value.pfnCreateDevice && m_adapterFunctions.value.pfnCloseAdapter, "modern-adapter-table");
    D3D10DDIARG_CALCPRIVATEDEVICESIZE size{};
    size.Interface = D3D11_0_DDI_INTERFACE_VERSION; size.Version = RuntimeVersion;
    size.Flags = UINT(D3D11DDI_3DPIPELINELEVEL_11_0) << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
    m_storage = std::make_unique<Storage>(m_adapterFunctions.value.pfnCalcPrivateDeviceSize(m_adapter, &size));
    m_device = {m_storage->data()};
    D3D10DDIARG_CREATEDEVICE args{};
    args.Interface = size.Interface; args.Version = size.Version; args.Flags = size.Flags;
    args.hDrvDevice = m_device; args.hRTDevice = m_transport.runtimeDevice(); args.hRTCoreLayer = m_transport.runtimeCore();
    args.pKTCallbacks = &m_kernelCallbacks; args.p11UMCallbacks = &m_coreCallbacks;
    args.p11DeviceFuncs = &m_functions.value; args.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &m_dxgi.value;
    const HRESULT hr = m_adapterFunctions.value.pfnCreateDevice(m_adapter, &args);
    require(hr == S_OK, "typed11-device-create", hr);
    m_alive = true;
    require(guards() && api().pfnDestroyDevice && api().pfnCreateResource && api().pfnDestroyResource
      && api().pfnResourceCopy && api().pfnStagingResourceMap && api().pfnStagingResourceUnmap
      && api().pfnDestroyShader && api().pfnCreateVertexShader && api().pfnCreateGeometryShaderWithStreamOutput
      && api().pfnCalcPrivateResourceSize && api().pfnCalcPrivateShaderSize
      && api().pfnCalcPrivateGeometryShaderWithStreamOutput && api().pfnVsSetShader && api().pfnGsSetShader
      && api().pfnPsSetShader && api().pfnHsSetShader && api().pfnDsSetShader
      && api().pfnIaSetInputLayout && api().pfnIaSetTopology && api().pfnFlush
      && api().pfnSoSetTargets && api().pfnDraw && api().pfnCalcPrivateQuerySize && api().pfnQueryGetData
      && api().pfnCreateQuery && api().pfnDestroyQuery && api().pfnQueryBegin && api().pfnQueryEnd,
      "typed11-device-slots");
    std::printf("D3D11_KMT_GRAPHICS_DEVICE interface=%08x version=%08x flags=%08x\n", args.Interface, args.Version, args.Flags);
  }
  D3D11DDI_DEVICEFUNCS& api() { return m_functions.value; }
  D3D10DDI_HDEVICE device() const { return m_device; }
  KmtComputeTransport& transport() { return m_transport; }
  template<typename Fn> void call(Fn&& fn, const char* stage) {
    m_transport.beginDdi(); fn(); check(stage);
  }
  template<typename Fn> SIZE_T size(Fn&& fn, const char* stage) {
    m_transport.beginDdi(); const SIZE_T value = fn(); check(stage); return value;
  }
  void check(const char* stage) {
    const HRESULT hr = m_transport.lastError();
    require(hr == S_OK && guards() && m_transport.counts.coreErrors == m_acceptedErrors,
      stage, hr == S_OK ? E_FAIL : hr);
  }
  void acceptExpectedError(HRESULT expectedHr, const char* stage) {
    const HRESULT observed = m_transport.lastError();
    unsigned next = m_acceptedErrors;
    require(guards() && admitExpectedCoreCallback(m_acceptedErrors, m_transport.counts.coreErrors,
      int32_t(expectedHr), int32_t(observed), next), stage, observed == S_OK ? E_FAIL : observed);
    m_acceptedErrors = next;
    std::printf("D3D11_KMT_EXPECTED_CORE_ERROR stage=%s hr=%08lx total=%u delta=1\n",
      stage, static_cast<unsigned long>(observed), m_acceptedErrors);
  }
  template<typename Fn> HRESULT negative(Fn&& fn, HRESULT expectedHr, const char* stage) {
    require(m_transport.counts.coreErrors == m_acceptedErrors && guards(), "before-named-negative");
    m_transport.beginDdi(); fn(); acceptExpectedError(expectedHr, stage);
    return m_transport.lastError();
  }
  unsigned acceptedErrors() const { return m_acceptedErrors; }
  void cleanupResult(bool intact) noexcept {
    const HRESULT hr = m_transport.lastError();
    if (m_cleanup == S_OK && (hr != S_OK || !intact || !guards()
        || m_transport.counts.coreErrors != m_acceptedErrors)) m_cleanup = hr == S_OK ? E_FAIL : hr;
  }
  HRESULT close() noexcept {
    HRESULT hr = m_cleanup;
    auto keep = [&](HRESULT error) { if (hr == S_OK && error != S_OK) hr = error; };
    if (m_alive) {
      m_transport.beginDdi();
      if (api().pfnDestroyDevice) { api().pfnDestroyDevice(m_device); keep(m_transport.lastError()); }
      else keep(E_FAIL);
      m_alive = false;
    }
    if (m_adapter.pDrvPrivate) {
      keep(m_adapterFunctions.value.pfnCloseAdapter ? m_adapterFunctions.value.pfnCloseAdapter(m_adapter) : E_FAIL);
      m_adapter = {};
    }
    if (!guards() || m_transport.counts.coreErrors != m_acceptedErrors) keep(E_FAIL);
    return hr;
  }
private:
  KmtComputeTransport& m_transport;
  D3DDDI_ADAPTERCALLBACKS m_adapterCallbacks{};
  D3DDDI_DEVICECALLBACKS m_kernelCallbacks{};
  // Runtime-owned live table, held until after device teardown.
  D3D11DDI_CORELAYER_DEVICECALLBACKS m_coreCallbacks{};
  Guarded<D3D10_2DDI_ADAPTERFUNCS> m_adapterFunctions;
  Guarded<D3D11DDI_DEVICEFUNCS> m_functions;
  Guarded<DXGI1_1_DDI_BASE_FUNCTIONS> m_dxgi;
  std::unique_ptr<Storage> m_storage;
  D3D10DDI_HADAPTER m_adapter{};
  D3D10DDI_HDEVICE m_device{};
  bool m_alive = false;
  HRESULT m_cleanup = S_OK;
  unsigned m_acceptedErrors = 0;
  bool guards() const {
    return m_adapterFunctions.intact() && m_functions.intact() && m_dxgi.intact()
      && (!m_storage || m_storage->intact());
  }
};

class Resource {
public:
  Resource(Session& session, const D3D11DDIARG_CREATERESOURCE& desc) : owner(session),
    storage(session.size([&] { return session.api().pfnCalcPrivateResourceSize(session.device(), &desc); }, "resource-private-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateResource(owner.device(), &desc, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("resource-create"); require(storage.intact(), "resource-private-guard"); }
    catch (...) { close(); throw; }
  }
  ~Resource() { close(); }
  Resource(const Resource&) = delete;
  Resource& operator=(const Resource&) = delete;
  void close() noexcept {
    if (mapped) {
      owner.transport().beginDdi();
      if (mappedDynamic) owner.api().pfnDynamicResourceUnmap(owner.device(), handle, mappedSubresource);
      else owner.api().pfnStagingResourceUnmap(owner.device(), handle, mappedSubresource);
      owner.cleanupResult(storage.intact()); mapped = false;
    }
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyResource(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  D3D10DDI_MAPPED_SUBRESOURCE map(UINT subresource) {
    require(!mapped, "resource-not-already-mapped");
    Guarded<D3D10DDI_MAPPED_SUBRESOURCE> result;
    owner.transport().beginDdi(); owner.api().pfnStagingResourceMap(owner.device(), handle, subresource,
      D3D10_DDI_MAP_READ, 0, &result.value);
    mapped = owner.transport().lastError() == S_OK; mappedSubresource = subresource; mappedDynamic = false;
    owner.check("resource-staging-map"); require(result.intact() && result.value.pData && storage.intact(), "resource-map-guards");
    return result.value;
  }
  D3D10DDI_MAPPED_SUBRESOURCE discard() {
    require(!mapped && owner.api().pfnDynamicResourceMapDiscard && owner.api().pfnDynamicResourceUnmap,
      "resource-discard-map-slots");
    Guarded<D3D10DDI_MAPPED_SUBRESOURCE> result;
    owner.transport().beginDdi(); owner.api().pfnDynamicResourceMapDiscard(owner.device(), handle, 0,
      D3D10_DDI_MAP_WRITE_DISCARD, 0, &result.value);
    mapped = owner.transport().lastError() == S_OK; mappedSubresource = 0; mappedDynamic = true;
    owner.check("resource-discard-map"); require(result.intact() && result.value.pData && storage.intact(), "resource-map-guards");
    return result.value;
  }
  void unmap() {
    require(mapped, "resource-is-mapped");
    owner.transport().beginDdi();
    if (mappedDynamic) owner.api().pfnDynamicResourceUnmap(owner.device(), handle, mappedSubresource);
    else owner.api().pfnStagingResourceUnmap(owner.device(), handle, mappedSubresource);
    mapped = owner.transport().lastError() != S_OK;
    owner.check("resource-staging-unmap"); require(storage.intact(), "resource-private-guard");
  }
  Session& owner;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
private:
  bool alive = false, mapped = false, mappedDynamic = false;
  UINT mappedSubresource = 0;
};

inline D3D11DDIARG_CREATERESOURCE bufferDescription(D3D10DDI_MIPINFO& shape, UINT bytes, UINT binds,
    const D3D10_DDIARG_SUBRESOURCE_UP* initial = nullptr, bool staging = false) {
  shape = {bytes, 1, 1, bytes, 1, 1};
  D3D11DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList = &shape; desc.pInitialDataUP = initial;
  desc.ResourceDimension = D3D10DDIRESOURCE_BUFFER; desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
  desc.BindFlags = staging ? 0 : binds; desc.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
  desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
  return desc;
}

inline std::vector<UINT> readBuffer(Session& session, Resource& resource, UINT words) {
  require(words && words <= MAXDWORD / sizeof(UINT), "buffer-readback-size");
  D3D10DDI_MIPINFO shape{};
  Resource staging(session, bufferDescription(shape, words * UINT(sizeof(UINT)), 0, nullptr, true));
  session.call([&] { session.api().pfnResourceCopy(session.device(), staging.handle, resource.handle); }, "buffer-copy-readback");
  session.call([&] { session.api().pfnFlush(session.device()); }, "buffer-copy-flush");
  const auto map = staging.map(0);
  std::vector<UINT> result(words);
  std::memcpy(result.data(), map.pData, result.size() * sizeof(UINT));
  staging.unmap();
  return result;
}

class Query {
  static SIZE_T privateSize(Session& session, D3D10DDI_QUERY type) {
    const D3D10DDIARG_CREATEQUERY desc{type, 0};
    return session.size([&] { return session.api().pfnCalcPrivateQuerySize(session.device(), &desc); }, "query-private-size");
  }
public:
  Query(Session& session, D3D10DDI_QUERY type) : owner(session),
    storage(privateSize(session, type)),
    handle{storage.data()} {
    const D3D10DDIARG_CREATEQUERY desc{type, 0};
    owner.transport().beginDdi(); owner.api().pfnCreateQuery(owner.device(), &desc, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("query-create"); require(storage.intact(), "query-private-guard"); }
    catch (...) { close(); throw; }
  }
  ~Query() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyQuery(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  Query(const Query&) = delete;
  Query& operator=(const Query&) = delete;
  void begin() { owner.call([&] { owner.api().pfnQueryBegin(owner.device(), handle); }, "query-begin"); }
  void end() { owner.call([&] { owner.api().pfnQueryEnd(owner.device(), handle); }, "query-end"); }
  template<typename T> T result() {
    Guarded<T> output;
    std::memset(&output.value, 0xcd, sizeof(T));
    const T poison = output.value;
    owner.call([&] { owner.api().pfnFlush(owner.device()); }, "query-flush");
    const ULONGLONG start = GetTickCount64();
    for (;;) {
      owner.transport().beginDdi(); owner.api().pfnQueryGetData(owner.device(), handle, &output.value, sizeof(T), D3D10_DDI_GET_DATA_DO_NOT_FLUSH);
      require(output.intact() && storage.intact(), "query-output-guards");
      if (owner.transport().lastError() == S_OK) { owner.check("query-complete-without-error-callback"); return output.value; }
      require(owner.transport().lastError() == DXGI_DDI_ERR_WASSTILLDRAWING
        && !std::memcmp(&poison, &output.value, sizeof(T)), "pending-query-unchanged-output", owner.transport().lastError());
      owner.acceptExpectedError(DXGI_DDI_ERR_WASSTILLDRAWING, "named-query-pending");
      require(GetTickCount64() - start < 10000, "query-result-deadline");
      SwitchToThread();
    }
  }
  Session& owner;
  Storage storage;
  D3D10DDI_HQUERY handle;
private:
  bool alive = false;
};

class Shader {
  static SIZE_T privateSize(Session& session, const ShaderSource& source,
      const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* stream, bool signatureOnly) {
    auto inputs = source.inputs, outputs = source.outputs;
    const D3D10DDIARG_STAGE_IO_SIGNATURES signature{signatureOnly ? nullptr : inputs.data(),
      signatureOnly ? 0 : UINT(inputs.size()), outputs.data(), UINT(outputs.size())};
    if (stream) {
      auto desc = *stream; desc.pShaderCode = signatureOnly ? nullptr : source.tokens.data();
      return session.size([&] { return session.api().pfnCalcPrivateGeometryShaderWithStreamOutput(
        session.device(), &desc, &signature); }, "stream-output-private-size");
    }
    return session.size([&] { return session.api().pfnCalcPrivateShaderSize(
      session.device(), source.tokens.data(), &signature); }, "shader-private-size");
  }
public:
  Shader(Session& session, const ShaderSource& source, UINT programType,
      const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* stream = nullptr, bool signatureOnly = false)
  : owner(session), storage(privateSize(session, source, stream, signatureOnly)), handle{storage.data()}, type(programType) {
    auto inputs = source.inputs, outputs = source.outputs;
    const D3D10DDIARG_STAGE_IO_SIGNATURES signature{inputs.data(), UINT(inputs.size()), outputs.data(), UINT(outputs.size())};
    owner.transport().beginDdi();
    if (type == 1) owner.api().pfnCreateVertexShader(owner.device(), source.tokens.data(), handle, {}, &signature);
    else if (type == 0) owner.api().pfnCreatePixelShader(owner.device(), source.tokens.data(), handle, {}, &signature);
    else if (type == 5) owner.api().pfnCreateComputeShader(owner.device(), source.tokens.data(), handle, {});
    else if (type == 2 && stream) {
      auto desc = *stream; desc.pShaderCode = signatureOnly ? nullptr : source.tokens.data();
      const D3D10DDIARG_STAGE_IO_SIGNATURES outputOnly{nullptr, 0, outputs.data(), UINT(outputs.size())};
      owner.api().pfnCreateGeometryShaderWithStreamOutput(owner.device(), &desc, handle, {}, signatureOnly ? &outputOnly : &signature);
    } else if (type == 2) owner.api().pfnCreateGeometryShader(owner.device(), source.tokens.data(), handle, {}, &signature);
    else throw Failure{"shader-stage", E_INVALIDARG};
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("shader-create"); require(storage.intact(), "shader-private-guard"); }
    catch (...) { close(); throw; }
  }
  ~Shader() { close(); }
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;
  void bind() {
    owner.call([&] {
      if (type == 1) owner.api().pfnVsSetShader(owner.device(), handle);
      else if (type == 0) owner.api().pfnPsSetShader(owner.device(), handle);
      else if (type == 2) owner.api().pfnGsSetShader(owner.device(), handle);
      else if (type == 5) owner.api().pfnCsSetShader(owner.device(), handle);
    }, "shader-bind");
  }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyShader(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  Session& owner;
  Storage storage;
  D3D10DDI_HSHADER handle;
private:
  UINT type;
  bool alive = false;
};

} // namespace dxvk::umd::probe::graphics
