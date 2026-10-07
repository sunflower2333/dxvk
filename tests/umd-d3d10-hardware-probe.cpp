// SPDX-License-Identifier: MIT
// Standalone, development-entry hardware probe. Never linked into the UMD.
#include "umd-kmt-compute-transport.h"
#include "umd-d3d10-hardware-oracle.h"
#include <bcrypt.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <cwchar>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace dxvk::umd::probe10;
// The supported-version constants carry build bits and low revision zero.
// Both legacy profiles therefore use the exact DXGI 1.0 output structure.
static_assert(D3D10_0_DDI_INTERFACE_VERSION == 0x000a0001);
static_assert(D3D10_1_DDI_INTERFACE_VERSION == 0x000a0002);
static_assert(UINT(D3D10_0_DDI_SUPPORTED) == (D3D10_0_DDI_BUILD_VERSION << 16));
static_assert(UINT(D3D10_1_DDI_SUPPORTED) == (D3D10_1_DDI_BUILD_VERSION << 16));
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D10_0_DDI_INTERFACE_VERSION, UINT(D3D10_0_DDI_SUPPORTED)));
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D10_1_DDI_INTERFACE_VERSION, UINT(D3D10_1_DDI_SUPPORTED)));
constexpr UINT DxgiRevision11 = VISTA_GOLD_PRODUCT_VER | DXGI_RESOLVE_SHARED_RESOURCE;
// The original macro does not fully parenthesize its Version parameter.
// Pass named values so the build/revision expression cannot change its mask.
constexpr UINT Dxgi10Before11 = (D3D10_0_DDI_BUILD_VERSION << 16) | (DxgiRevision11 - 1);
constexpr UINT Dxgi10At11 = (D3D10_0_DDI_BUILD_VERSION << 16) | DxgiRevision11;
constexpr UINT Dxgi10_1Before11 = (D3D10_1_DDI_BUILD_VERSION << 16) | (DxgiRevision11 - 1);
constexpr UINT Dxgi10_1At11 = (D3D10_1_DDI_BUILD_VERSION << 16) | DxgiRevision11;
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D10_0_DDI_INTERFACE_VERSION, Dxgi10Before11));
static_assert(IS_DXGI1_1_BASE_FUNCTIONS(D3D10_0_DDI_INTERFACE_VERSION, Dxgi10At11));
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D10_1_DDI_INTERFACE_VERSION, Dxgi10_1Before11));
static_assert(IS_DXGI1_1_BASE_FUNCTIONS(D3D10_1_DDI_INTERFACE_VERSION, Dxgi10_1At11));
struct Failure { const char* stage; HRESULT hr; };
void require(bool value, const char* stage, HRESULT hr = E_FAIL) {
  if (!value) throw Failure{stage, hr};
}

template<typename T> struct Guarded {
  std::array<unsigned char, 16> before;
  T value{};
  std::array<unsigned char, 16> after;
  Guarded() { before.fill(0xa5); after.fill(0xa5); }
  bool intact() const {
    static_assert(alignof(T) <= 16 && 16 % alignof(T) == 0);
    static_assert(offsetof(Guarded, value) == 16);
    static_assert(offsetof(Guarded, after) == 16 + sizeof(T));
    for (unsigned i = 0; i < 16; ++i) if (before[i] != 0xa5 || after[i] != 0xa5) return false;
    return true;
  }
};
class PrivateStorage {
public:
  void allocate(SIZE_T size) {
    require(!m_bytes && size && size <= 16 * 1024 * 1024, "private-storage-size");
    m_size = size; m_bytes.reset(new unsigned char[size + 32]);
    std::memset(m_bytes.get(), 0xa5, size + 32); std::memset(data(), 0, size);
  }
  void* data() { return m_bytes ? m_bytes.get() + 16 : nullptr; }
  bool intact() const {
    if (!m_bytes) return true;
    for (SIZE_T i = 0; i < 16; ++i)
      if (m_bytes[i] != 0xa5 || m_bytes[m_size + 16 + i] != 0xa5) return false;
    return true;
  }
private:
  std::unique_ptr<unsigned char[]> m_bytes;
  SIZE_T m_size = 0;
};
struct FileOwner {
  HANDLE handle = INVALID_HANDLE_VALUE;
  ~FileOwner() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
struct ModuleOwner {
  HMODULE handle = nullptr;
  ~ModuleOwner() { if (handle) FreeLibrary(handle); }
};
struct HashOwner {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<unsigned char> state;
  ~HashOwner() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
};

std::wstring absolutePath(const WCHAR* input) {
  require(input && std::wcslen(input) >= 3
    && ((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z'))
    && input[1] == L':' && input[2] == L'\\', "absolute-drive-path");
  WCHAR path[32768]{};
  const DWORD length = GetFullPathNameW(input, DWORD(std::size(path)), path, nullptr);
  require(length && length < std::size(path), "canonical-path", HRESULT_FROM_WIN32(GetLastError()));
  return {path, length};
}
std::array<std::uint8_t, 32> fileHash(const std::wstring& path) {
  HashOwner owner;
  require(BCryptOpenAlgorithmProvider(&owner.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, "sha256-provider");
  DWORD size = 0, returned = 0;
  require(BCryptGetProperty(owner.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size),
    sizeof(size), &returned, 0) == 0 && returned == sizeof(size) && size && size <= 1024 * 1024, "sha256-object");
  owner.state.resize(size);
  require(BCryptCreateHash(owner.algorithm, &owner.hash, owner.state.data(), size, nullptr, 0, 0) == 0, "sha256-create");
  FileOwner file;
  file.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file.handle != INVALID_HANDLE_VALUE, "hash-file-open", HRESULT_FROM_WIN32(GetLastError()));
  std::array<unsigned char, 65536> block{};
  for (;;) {
    DWORD bytes = 0;
    const BOOL read = ReadFile(file.handle, block.data(), DWORD(block.size()), &bytes, nullptr);
    const DWORD error = read ? ERROR_SUCCESS : GetLastError();
    require(read != FALSE, "hash-file-read", HRESULT_FROM_WIN32(error));
    if (!bytes) break;
    require(BCryptHashData(owner.hash, block.data(), bytes, 0) == 0, "sha256-update");
  }
  std::array<std::uint8_t, 32> digest{};
  require(BCryptFinishHash(owner.hash, digest.data(), ULONG(digest.size()), 0) == 0, "sha256-finish");
  return digest;
}
void printHex(const void* input, size_t bytes) {
  const auto data = static_cast<const unsigned char*>(input);
  for (size_t i = 0; i < bytes; ++i) std::printf("%02x", unsigned(data[i]));
}
std::wstring loadedPath(HMODULE module) {
  WCHAR path[32768]{};
  const DWORD length = GetModuleFileNameW(module, path, DWORD(std::size(path)));
  require(length && length < std::size(path), "loaded-module-path", HRESULT_FROM_WIN32(GetLastError()));
  return {path, length};
}
void retain(const std::wstring& directory, const std::wstring& name, const void* data, size_t bytes) {
  require(data && bytes && bytes <= MAXDWORD, "original-output-size");
  const std::wstring path = directory + L"\\" + name;
  FileOwner file;
  file.handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file.handle != INVALID_HANDLE_VALUE, "original-output-create", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0;
  const BOOL saved = WriteFile(file.handle, data, DWORD(bytes), &written, nullptr);
  const DWORD error = saved ? ERROR_SUCCESS : GetLastError();
  require(saved && written == bytes, "original-output-write", error ? HRESULT_FROM_WIN32(error) : E_FAIL);
  const BOOL closed = CloseHandle(file.handle); const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
  file.handle = INVALID_HANDLE_VALUE;
  require(closed != FALSE, "original-output-close", HRESULT_FROM_WIN32(closeError));
}

using OpenAdapter = HRESULT (APIENTRY*)(D3D10DDIARG_OPENADAPTER*);
class NamedCore {
public:
  NamedCore(const std::wstring& path, const std::array<std::uint8_t, 32>& expected) : m_path(path), m_hash(expected) {
    require(fileHash(path) == expected, "original-core-hash-before");
    m_module.handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(m_module.handle, "load-exact-core", HRESULT_FROM_WIN32(GetLastError()));
    require(!_wcsicmp(loadedPath(m_module.handle).c_str(), path.c_str()) && fileHash(path) == expected, "loaded-exact-core");
    // GetProcAddress is converted only to the original function signature.
    // Device/adapter tables are assigned using their exact WDK types.
    open = reinterpret_cast<OpenAdapter>(GetProcAddress(m_module.handle, "VioGpuDxvkOpenAdapter10_2ForTest"));
    require(open, "modern-development-export");
    std::printf("D3D10_KMT_CORE path=%ls sha256=", path.c_str()); printHex(expected.data(), expected.size()); std::printf("\n");
  }
  void verifyRetained() const { require(fileHash(m_path) == m_hash, "original-core-hash-after"); }
  OpenAdapter open = nullptr;
private:
  ModuleOwner m_module;
  std::wstring m_path;
  std::array<std::uint8_t, 32> m_hash;
};

constexpr char Hlsl[] = R"(
float4 vs_main(uint vertex : SV_VertexID) : SV_Position {
  float2 xy = float2((vertex << 1) & 2, vertex & 2);
  return float4(xy * float2(2,-2) + float2(-1,1), 0, 1);
}
float4 ps_main() : SV_Target { return float4(1,0,0,1); }
Texture2D<float> source : register(t0);
SamplerState sampler0 : register(s0);
float4 ps_gather() : SV_Target {
  float4 samples = source.Gather(sampler0, float2(0.5,0.5));
  return float4(0,0,(samples.x+samples.y+samples.z+samples.w)*0.5,1);
}
float4 ps_index(uint sample : SV_SampleIndex) : SV_Target {
  return float4(float(sample & 1),1,0,1);
}
)";
struct BlobDeleter { void operator()(ID3DBlob* blob) const { if (blob) blob->Release(); } };
using BlobOwner = std::unique_ptr<ID3DBlob, BlobDeleter>;
struct ReflectionDeleter { void operator()(ID3D11ShaderReflection* value) const { if (value) value->Release(); } };
struct Compiled {
  std::vector<UINT> code;
  std::vector<D3D10DDIARG_SIGNATURE_ENTRY> inputs, outputs;
  D3D10DDIARG_STAGE_IO_SIGNATURES signature() {
    D3D10DDIARG_STAGE_IO_SIGNATURES result{};
    result.pInputSignature = inputs.data(); result.NumInputSignatureEntries = UINT(inputs.size());
    result.pOutputSignature = outputs.data(); result.NumOutputSignatureEntries = UINT(outputs.size());
    return result;
  }
};
class SystemCompiler {
public:
  SystemCompiler() {
    WCHAR path[32768]{};
    const UINT length = GetSystemDirectoryW(path, UINT(std::size(path)));
    require(length && length < std::size(path), "system-directory");
    m_path = std::wstring(path, length) + L"\\d3dcompiler_47.dll";
    require(decodeHex("4b68cd1fc3d482d0965910f1995513549989ae17a53d9be122de08437102a1d1", m_expected), "compiler-receipt-digest");
    require(fileHash(m_path) == m_expected, "original-system-compiler-hash");
    m_module.handle = LoadLibraryExW(m_path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(m_module.handle && !_wcsicmp(loadedPath(m_module.handle).c_str(), m_path.c_str()), "system-compiler-load");
    m_compile = reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(m_module.handle, "D3DCompile"));
    m_reflect = reinterpret_cast<decltype(&D3DReflect)>(GetProcAddress(m_module.handle, "D3DReflect"));
    require(m_compile && m_reflect, "system-compiler-exports");
  }
  Compiled compile(const std::wstring& directory, const char* entry, bool vertex, bool model41) const {
    const char* profile = vertex ? model41 ? "vs_4_1" : "vs_4_0" : model41 ? "ps_4_1" : "ps_4_0";
    const std::wstring stem = std::wstring(vertex ? L"vs-" : L"ps-") + std::wstring(entry, entry + std::strlen(entry));
    ID3DBlob *binary = nullptr, *diagnostics = nullptr;
    const HRESULT hr = m_compile(Hlsl, sizeof(Hlsl) - 1, "typed10-kmt-hardware", nullptr, nullptr, entry, profile,
      D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &binary, &diagnostics);
    const BlobOwner binaryOwner(binary), diagnosticOwner(diagnostics);
    if (diagnostics && diagnostics->GetBufferSize()) retain(directory, stem + L".diagnostics", diagnostics->GetBufferPointer(), diagnostics->GetBufferSize());
    require(hr == S_OK && binary, "compile-original-shader", hr == S_OK ? E_FAIL : hr);
    const auto data = static_cast<const unsigned char*>(binary->GetBufferPointer());
    const size_t size = binary->GetBufferSize();
    retain(directory, stem + L".dxbc", data, size);
    require(size >= 32 && size <= 1024 * 1024, "original-dxbc-size");
    auto word = [&](size_t offset) { require(offset <= size - 4, "original-dxbc-word"); UINT value; std::memcpy(&value, data + offset, 4); return value; };
    require(word(0) == 0x43425844 && word(20) == 1 && word(24) == size, "original-dxbc-header");
    const UINT chunks = word(28); require(chunks && chunks <= (size - 32) / 4, "original-dxbc-chunks");
    Compiled result;
    for (UINT i = 0; i < chunks; ++i) {
      const UINT offset = word(32 + size_t(i) * 4);
      require(offset >= 32 + size_t(chunks) * 4 && offset <= size - 8 && !(offset & 3), "original-chunk-offset");
      const UINT bytes = word(offset + 4); require(bytes <= size - offset - 8, "original-chunk-size");
      if (word(offset) != 0x52444853) continue;
      require(result.code.empty() && bytes >= 8 && !(bytes & 3), "original-shdr");
      result.code.resize(bytes / 4); std::memcpy(result.code.data(), data + offset + 8, bytes);
    }
    require(result.code.size() >= 2 && result.code[1] == result.code.size()
      && result.code[0] == ((vertex ? 1u : 0u) << 16 | (model41 ? 0x41u : 0x40u)), "original-shader-profile");
    retain(directory, stem + L".tokens", result.code.data(), result.code.size() * 4);
    ID3D11ShaderReflection* reflection = nullptr;
    const HRESULT reflected = m_reflect(data, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&reflection));
    const std::unique_ptr<ID3D11ShaderReflection, ReflectionDeleter> reflectionOwner(reflection);
    require(reflected == S_OK && reflection, "original-shader-reflection", reflected == S_OK ? E_FAIL : reflected);
    D3D11_SHADER_DESC desc{}; require(reflection->GetDesc(&desc) == S_OK
      && desc.InputParameters <= 32 && desc.OutputParameters && desc.OutputParameters <= 32, "original-signature-count");
    for (bool input : {true, false}) {
      const UINT count = input ? desc.InputParameters : desc.OutputParameters;
      auto& entries = input ? result.inputs : result.outputs;
      for (UINT i = 0; i < count; ++i) {
        D3D11_SIGNATURE_PARAMETER_DESC parameter{};
        const HRESULT value = input ? reflection->GetInputParameterDesc(i, &parameter) : reflection->GetOutputParameterDesc(i, &parameter);
        require(value == S_OK && parameter.Mask && !(parameter.Mask & ~15u), "original-signature-entry", value == S_OK ? E_FAIL : value);
        D3D10DDIARG_SIGNATURE_ENTRY native{};
        native.SystemValue = static_cast<D3D10_SB_NAME>(parameter.SystemValueType == D3D_NAME_TARGET ? D3D_NAME_UNDEFINED : parameter.SystemValueType);
        native.Register = parameter.Register; native.Mask = parameter.Mask; entries.push_back(native);
      }
    }
    std::printf("D3D10_KMT_SHADER profile=%s entry=%s words=%zu inputs=%zu outputs=%zu\n", profile, entry, result.code.size(), result.inputs.size(), result.outputs.size());
    return result;
  }
  void verifyRetained() const { require(fileHash(m_path) == m_expected, "original-system-compiler-retained"); }
private:
  ModuleOwner m_module;
  std::wstring m_path;
  std::array<std::uint8_t, 32> m_expected{};
  decltype(&D3DCompile) m_compile = nullptr;
  decltype(&D3DReflect) m_reflect = nullptr;
};

enum class Kind { Resource, RenderTarget, ShaderView, Sampler, Rasterizer, Shader };
struct Object { PrivateStorage storage; Kind kind; bool alive = false; };
template<typename Table> class DeviceSession {
public:
  DeviceSession(KmtComputeTransport& transport, OpenAdapter open) : m_transport(transport), m_open(open) {
    transport.callbacks(m_adapterCallbacks, m_kernelCallbacks, m_coreCallbacks); m_objects.reserve(24);
  }
  ~DeviceSession() { const HRESULT hr = close(); if (hr != S_OK) std::printf("D3D10_KMT_CLEANUP hr=%08lx\n", static_cast<unsigned long>(hr)); }
  void create() {
    D3D10DDIARG_OPENADAPTER opened{};
    opened.hRTAdapter = m_transport.runtimeAdapter(); opened.pAdapterCallbacks = &m_adapterCallbacks;
    opened.pAdapterFuncs_2 = &m_adapterFunctions.value;
    const HRESULT hr = m_open(&opened); m_adapter = opened.hAdapter;
    require(hr == S_OK && m_adapter.pDrvPrivate && m_adapterFunctions.intact(), "modern-open-typed10", hr == S_OK ? E_FAIL : hr);
    auto& adapter = m_adapterFunctions.value;
    require(adapter.pfnCalcPrivateDeviceSize && adapter.pfnCreateDevice && adapter.pfnCloseAdapter && adapter.pfnGetSupportedVersions && adapter.pfnGetCaps, "modern-adapter-functions");
    Guarded<std::array<UINT64, 8>> versions;
    UINT count = UINT(versions.value.size());
    require(adapter.pfnGetSupportedVersions(m_adapter, &count, versions.value.data()) == S_OK
      && count && count <= versions.value.size() && versions.intact(), "modern-exact-versions");
    constexpr bool model41 = std::is_same_v<Table, D3D10_1DDI_DEVICEFUNCS>;
    const UINT64 required = model41 ? D3D10_1_DDI_SUPPORTED : D3D10_0_DDI_SUPPORTED;
    require(std::find(versions.value.begin(), versions.value.begin() + count, required) != versions.value.begin() + count, "selected-typed10-version");
    Guarded<D3D11DDI_3DPIPELINESUPPORT_CAPS> caps;
    D3D10_2DDIARG_GETCAPS capsArgs{};
    capsArgs.Type = D3D11DDICAPS_3DPIPELINESUPPORT; capsArgs.pData = &caps.value; capsArgs.DataSize = sizeof(caps.value);
    require(adapter.pfnGetCaps(m_adapter, &capsArgs) == S_OK && caps.intact() && !caps.value.Caps, "closed-runtime-pipeline-caps");
    D3D10DDIARG_CALCPRIVATEDEVICESIZE size{};
    size.Interface = UINT(required >> 32); size.Version = UINT(required);
    require(!IS_DXGI1_1_BASE_FUNCTIONS(size.Interface, size.Version), "exact-dxgi-1-0-table");
    m_deviceStorage.allocate(adapter.pfnCalcPrivateDeviceSize(m_adapter, &size)); m_device.pDrvPrivate = m_deviceStorage.data();
    D3D10DDIARG_CREATEDEVICE args{};
    args.Interface = size.Interface; args.Version = size.Version;
    args.hDrvDevice = m_device; args.hRTDevice = m_transport.runtimeDevice(); args.hRTCoreLayer = m_transport.runtimeCore();
    args.pKTCallbacks = &m_kernelCallbacks; args.pUMCallbacks = &m_coreCallbacks;
    if constexpr (model41) args.p10_1DeviceFuncs = &m_functions.value;
    else args.pDeviceFuncs = &m_functions.value;
    args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &m_dxgiFunctions.value;
    m_transport.beginDdi(); const HRESULT created = adapter.pfnCreateDevice(m_adapter, &args);
    m_deviceAlive = created == S_OK;
    require(created == S_OK, "create-real-typed10-device", created); check("typed10-device-output");
    auto& api = m_functions.value;
    require(api.pfnDestroyDevice && api.pfnCalcPrivateResourceSize && api.pfnCreateResource && api.pfnDestroyResource
      && api.pfnCalcPrivateRenderTargetViewSize && api.pfnCreateRenderTargetView && api.pfnDestroyRenderTargetView
      && api.pfnSetRenderTargets && api.pfnClearRenderTargetView && api.pfnResourceCopy && api.pfnResourceResolveSubresource
      && api.pfnCheckMultisampleQualityLevels
      && api.pfnStagingResourceMap && api.pfnStagingResourceUnmap && api.pfnFlush
      && api.pfnCalcPrivateShaderSize && api.pfnCreateVertexShader && api.pfnCreatePixelShader && api.pfnDestroyShader
      && api.pfnVsSetShader && api.pfnPsSetShader && api.pfnGsSetShader && api.pfnDraw
      && api.pfnSetViewports && api.pfnIaSetTopology && api.pfnIaSetInputLayout
      && api.pfnCalcPrivateRasterizerStateSize && api.pfnCreateRasterizerState && api.pfnSetRasterizerState && api.pfnDestroyRasterizerState
      && api.pfnCalcPrivateShaderResourceViewSize && api.pfnCreateShaderResourceView && api.pfnDestroyShaderResourceView
      && api.pfnPsSetShaderResources && api.pfnCalcPrivateSamplerSize && api.pfnCreateSampler && api.pfnPsSetSamplers && api.pfnDestroySampler,
      "typed10-required-table-entries");
    std::printf("D3D10_KMT_DEVICE interface=%08x version=%08x flags=0 profile=%s\n", args.Interface, args.Version, model41 ? "10_1" : "10_0");
  }
  void render(const std::wstring& directory, const SystemCompiler& compiler) {
    constexpr bool model41 = std::is_same_v<Table, D3D10_1DDI_DEVICEFUNCS>;
    auto& api = m_functions.value;
    const auto vertex = createShader(compiler.compile(directory, "vs_main", true, model41), true);
    const auto pixel = createShader(compiler.compile(directory, "ps_main", false, model41), false);
    const auto target = createTexture(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D10_DDI_BIND_RENDER_TARGET, false);
    m_staging = createTexture(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, true);
    const auto view = createTarget(target);
    D3D10_DDI_RASTERIZER_DESC raster{}; raster.FillMode = D3D10_DDI_FILL_SOLID; raster.CullMode = D3D10_DDI_CULL_NONE;
    raster.DepthClipEnable = raster.MultisampleEnable = TRUE;
    m_transport.beginDdi(); auto& rasterObject = object(Kind::Rasterizer, api.pfnCalcPrivateRasterizerStateSize(m_device, &raster)); check("private-rasterizer-size");
    const D3D10DDI_HRASTERIZERSTATE rasterHandle{rasterObject.storage.data()}; rasterObject.alive = true;
    m_transport.beginDdi(); api.pfnCreateRasterizerState(m_device, &raster, rasterHandle, {}); check("create-rasterizer");
    m_transport.beginDdi(); api.pfnSetRasterizerState(m_device, rasterHandle); check("bind-rasterizer");
    const D3D10_DDI_VIEWPORT viewport{0, 0, FLOAT(Width), FLOAT(Height), 0, 1};
    m_transport.beginDdi(); api.pfnSetViewports(m_device, 1, 0, &viewport); check("viewport");
    m_transport.beginDdi(); api.pfnIaSetTopology(m_device, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST); check("topology");
    m_transport.beginDdi(); api.pfnIaSetInputLayout(m_device, {}); check("vertex-id-no-layout");
    m_transport.beginDdi(); api.pfnGsSetShader(m_device, {}); check("no-geometry-shader");
    m_transport.beginDdi(); api.pfnVsSetShader(m_device, vertex); check("bind-vertex-shader");
    m_transport.beginDdi(); api.pfnPsSetShader(m_device, pixel); check("bind-pixel-shader");
    bindTarget(view); clear(view); readback(directory, L"01-clear.raw", target, Scene::Clear);
    draw(); readback(directory, L"02-vs-ps.raw", target, Scene::VertexPixel);
    if constexpr (model41) {
      gather(directory, compiler, target);
      UINT quality = 0;
      m_transport.beginDdi(); api.pfnCheckMultisampleQualityLevels(m_device, DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality); check("four-sample-quality");
      require(quality, "four-sample-quality-supported");
      const auto multi = createTexture(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 4, D3D10_DDI_BIND_RENDER_TARGET, false);
      const auto multiView = createTarget(multi);
      const auto resolved = createTexture(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, false);
      bindTarget(multiView); clear(multiView); resolve(resolved, multi);
      readback(directory, L"04-msaa-clear.raw", resolved, Scene::Clear);
      const auto index = createShader(compiler.compile(directory, "ps_index", false, true), false);
      m_transport.beginDdi(); api.pfnPsSetShader(m_device, index); check("bind-sample-index");
      draw(); resolve(resolved, multi); readback(directory, L"05-sample-index.raw", resolved, Scene::SampleIndex);
    }
    require(m_draws == (model41 ? 3u : 1u) && m_images == (model41 ? 5u : 2u), "bounded-scene-count");
  }
  unsigned draws() const { return m_draws; }
  unsigned pixels() const { return m_images * PixelCount; }
  HRESULT close() noexcept {
    HRESULT hr = S_OK;
    auto keep = [&](HRESULT value) { if (hr == S_OK && value != S_OK) hr = value; };
    auto complete = [&] { keep(m_transport.lastError()); if (!guards()) keep(E_FAIL); };
    auto& api = m_functions.value;
    if (m_deviceAlive) {
      if (m_mapped && api.pfnStagingResourceUnmap) { m_transport.beginDdi(); api.pfnStagingResourceUnmap(m_device, m_staging, 0); m_mapped = false; complete(); }
      if (api.pfnSetRenderTargets) { m_transport.beginDdi(); api.pfnSetRenderTargets(m_device, nullptr, 0, 0, {}); complete(); }
      if (api.pfnVsSetShader) { m_transport.beginDdi(); api.pfnVsSetShader(m_device, {}); complete(); }
      if (api.pfnPsSetShader) { m_transport.beginDdi(); api.pfnPsSetShader(m_device, {}); complete(); }
      if (api.pfnGsSetShader) { m_transport.beginDdi(); api.pfnGsSetShader(m_device, {}); complete(); }
      if (api.pfnPsSetShaderResources) { const D3D10DDI_HSHADERRESOURCEVIEW empty{}; m_transport.beginDdi(); api.pfnPsSetShaderResources(m_device, 0, 1, &empty); complete(); }
      if (api.pfnPsSetSamplers) { const D3D10DDI_HSAMPLER empty{}; m_transport.beginDdi(); api.pfnPsSetSamplers(m_device, 0, 1, &empty); complete(); }
      if (api.pfnSetRasterizerState) { m_transport.beginDdi(); api.pfnSetRasterizerState(m_device, {}); complete(); }
      for (auto i = m_objects.rbegin(); i != m_objects.rend(); ++i) {
        if (!i->alive) continue;
        m_transport.beginDdi(); void* data = i->storage.data();
        switch (i->kind) {
          case Kind::Resource: api.pfnDestroyResource(m_device, {data}); break;
          case Kind::RenderTarget: api.pfnDestroyRenderTargetView(m_device, {data}); break;
          case Kind::ShaderView: api.pfnDestroyShaderResourceView(m_device, {data}); break;
          case Kind::Sampler: api.pfnDestroySampler(m_device, {data}); break;
          case Kind::Rasterizer: api.pfnDestroyRasterizerState(m_device, {data}); break;
          case Kind::Shader: api.pfnDestroyShader(m_device, {data}); break;
        }
        i->alive = false; complete();
      }
      if (api.pfnDestroyDevice) { m_transport.beginDdi(); api.pfnDestroyDevice(m_device); complete(); }
      else keep(E_FAIL);
      m_deviceAlive = false;
    }
    if (m_adapter.pDrvPrivate) {
      if (m_adapterFunctions.value.pfnCloseAdapter) keep(m_adapterFunctions.value.pfnCloseAdapter(m_adapter));
      else keep(E_FAIL);
      m_adapter = {};
    }
    if (!guards()) keep(E_FAIL);
    return hr;
  }
private:
  KmtComputeTransport& m_transport;
  OpenAdapter m_open;
  D3DDDI_ADAPTERCALLBACKS m_adapterCallbacks{};
  D3DDDI_DEVICECALLBACKS m_kernelCallbacks{};
  // Live callback storage remains valid through every Destroy and Close DDI.
  D3D10DDI_CORELAYER_DEVICECALLBACKS m_coreCallbacks{};
  Guarded<D3D10_2DDI_ADAPTERFUNCS> m_adapterFunctions;
  Guarded<Table> m_functions;
  Guarded<DXGI_DDI_BASE_FUNCTIONS> m_dxgiFunctions;
  PrivateStorage m_deviceStorage;
  D3D10DDI_HADAPTER m_adapter{};
  D3D10DDI_HDEVICE m_device{};
  D3D10DDI_HRESOURCE m_staging{};
  std::vector<Object> m_objects;
  bool m_deviceAlive = false, m_mapped = false;
  unsigned m_draws = 0, m_images = 0;
  bool guards() const {
    if (!m_adapterFunctions.intact() || !m_functions.intact() || !m_dxgiFunctions.intact() || !m_deviceStorage.intact()) return false;
    for (const auto& entry : m_objects) if (!entry.storage.intact()) return false;
    return true;
  }
  void check(const char* stage) const { const HRESULT hr = m_transport.lastError(); require(hr == S_OK && guards(), stage, hr == S_OK ? E_FAIL : hr); }
  Object& object(Kind kind, SIZE_T size) {
    require(m_objects.size() < 24, "bounded-object-count");
    Object value; value.kind = kind; value.storage.allocate(size); m_objects.push_back(std::move(value)); return m_objects.back();
  }
  D3D10DDI_HRESOURCE createTexture(UINT width, UINT height, DXGI_FORMAT format, UINT samples, UINT bind, bool staging, const void* initial = nullptr) {
    D3D10DDI_MIPINFO mip{width, height, 1, width, height, 1};
    D3D10_DDIARG_SUBRESOURCE_UP data{const_cast<void*>(initial), width * 4, width * height * 4};
    D3D10DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList = &mip; desc.pInitialDataUP = initial ? &data : nullptr; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT; desc.BindFlags = bind;
    desc.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0; desc.Format = format; desc.SampleDesc.Count = samples;
    desc.MipLevels = desc.ArraySize = 1;
    auto& api = m_functions.value;
    m_transport.beginDdi(); auto& item = object(Kind::Resource, api.pfnCalcPrivateResourceSize(m_device, &desc)); check("private-texture-size");
    const D3D10DDI_HRESOURCE handle{item.storage.data()}; item.alive = true;
    m_transport.beginDdi(); api.pfnCreateResource(m_device, &desc, handle, {}); check(staging ? "create-staging-texture" : "create-default-texture"); return handle;
  }
  D3D10DDI_HRENDERTARGETVIEW createTarget(D3D10DDI_HRESOURCE texture) {
    D3D10DDIARG_CREATERENDERTARGETVIEW desc{}; desc.hDrvResource = texture;
    desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.Tex2D.ArraySize = 1;
    auto& api = m_functions.value;
    m_transport.beginDdi(); auto& item = object(Kind::RenderTarget, api.pfnCalcPrivateRenderTargetViewSize(m_device, &desc)); check("private-target-size");
    const D3D10DDI_HRENDERTARGETVIEW handle{item.storage.data()}; item.alive = true;
    m_transport.beginDdi(); api.pfnCreateRenderTargetView(m_device, &desc, handle, {}); check("create-target-view"); return handle;
  }
  D3D10DDI_HSHADER createShader(Compiled compiled, bool vertex) {
    auto signature = compiled.signature(); auto& api = m_functions.value;
    m_transport.beginDdi(); auto& item = object(Kind::Shader, api.pfnCalcPrivateShaderSize(m_device, compiled.code.data(), &signature)); check("private-shader-size");
    const D3D10DDI_HSHADER handle{item.storage.data()}; item.alive = true;
    m_transport.beginDdi();
    if (vertex) api.pfnCreateVertexShader(m_device, compiled.code.data(), handle, {}, &signature);
    else api.pfnCreatePixelShader(m_device, compiled.code.data(), handle, {}, &signature);
    check(vertex ? "create-native-vertex-shader" : "create-native-pixel-shader"); return handle;
  }
  void bindTarget(D3D10DDI_HRENDERTARGETVIEW view) { m_transport.beginDdi(); m_functions.value.pfnSetRenderTargets(m_device, &view, 1, 0, {}); check("bind-target"); }
  void clear(D3D10DDI_HRENDERTARGETVIEW view) { FLOAT color[]{0,1,0,1}; m_transport.beginDdi(); m_functions.value.pfnClearRenderTargetView(m_device, view, color); check("clear-green"); }
  void draw() { m_transport.beginDdi(); m_functions.value.pfnDraw(m_device, 3, 0); check("draw-fullscreen"); ++m_draws; }
  void resolve(D3D10DDI_HRESOURCE destination, D3D10DDI_HRESOURCE source) {
    m_transport.beginDdi(); m_functions.value.pfnResourceResolveSubresource(m_device, destination, 0, source, 0, DXGI_FORMAT_R8G8B8A8_UNORM); check("resolve-four-sample");
  }
  void gather(const std::wstring& directory, const SystemCompiler& compiler, D3D10DDI_HRESOURCE target) {
    auto& api = m_functions.value;
    const FLOAT values[]{0,1,1,0};
    const auto resource = createTexture(2, 2, DXGI_FORMAT_R32_FLOAT, 1, D3D10_DDI_BIND_SHADER_RESOURCE, false, values);
    D3D10_1DDIARG_CREATESHADERRESOURCEVIEW desc{};
    desc.hDrvResource = resource; desc.Format = DXGI_FORMAT_R32_FLOAT; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Tex2D.ArraySize = desc.Tex2D.MipLevels = 1;
    m_transport.beginDdi(); auto& viewObject = object(Kind::ShaderView, api.pfnCalcPrivateShaderResourceViewSize(m_device, &desc)); check("private-gather-view-size");
    const D3D10DDI_HSHADERRESOURCEVIEW view{viewObject.storage.data()}; viewObject.alive = true;
    m_transport.beginDdi(); api.pfnCreateShaderResourceView(m_device, &desc, view, {}); check("create-gather-view");
    D3D10_DDI_SAMPLER_DESC sampler{}; sampler.Filter = D3D10_DDI_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D10_DDI_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = 1000; sampler.MaxAnisotropy = 1; sampler.ComparisonFunc = D3D10_DDI_COMPARISON_NEVER;
    m_transport.beginDdi(); auto& samplerObject = object(Kind::Sampler, api.pfnCalcPrivateSamplerSize(m_device, &sampler)); check("private-gather-sampler-size");
    const D3D10DDI_HSAMPLER samplerHandle{samplerObject.storage.data()}; samplerObject.alive = true;
    m_transport.beginDdi(); api.pfnCreateSampler(m_device, &sampler, samplerHandle, {}); check("create-gather-sampler");
    const auto shader = createShader(compiler.compile(directory, "ps_gather", false, true), false);
    m_transport.beginDdi(); api.pfnPsSetShaderResources(m_device, 0, 1, &view); check("bind-gather-resource");
    m_transport.beginDdi(); api.pfnPsSetSamplers(m_device, 0, 1, &samplerHandle); check("bind-gather-sampler");
    m_transport.beginDdi(); api.pfnPsSetShader(m_device, shader); check("bind-gather-shader");
    draw(); readback(directory, L"03-gather.raw", target, Scene::Gather);
  }
  void readback(const std::wstring& directory, const WCHAR* name, D3D10DDI_HRESOURCE source, Scene scene) {
    auto& api = m_functions.value;
    m_transport.beginDdi(); api.pfnResourceCopy(m_device, m_staging, source); check("copy-staging-readback");
    m_transport.beginDdi(); api.pfnFlush(m_device); check("flush-staging-copy");
    Guarded<D3D10DDI_MAPPED_SUBRESOURCE> mapped;
    m_transport.beginDdi(); api.pfnStagingResourceMap(m_device, m_staging, 0, D3D10_DDI_MAP_READ, 0, &mapped.value);
    m_mapped = m_transport.lastError() == S_OK; check("map-staging-readback");
    require(mapped.intact() && mapped.value.pData && mapped.value.RowPitch >= Width * 4 && mapped.value.RowPitch <= 65536, "mapped-readback-output");
    Pixels image{};
    for (UINT y = 0; y < Height; ++y) std::memcpy(image.data() + y * Width * 4,
      static_cast<const unsigned char*>(mapped.value.pData) + size_t(y) * mapped.value.RowPitch, Width * 4);
    m_transport.beginDdi(); api.pfnStagingResourceUnmap(m_device, m_staging, 0);
    m_mapped = m_transport.lastError() != S_OK; check("unmap-staging-readback");
    retain(directory, name, image.data(), image.size());
    unsigned mismatches = 0;
    const bool matched = readbackMatches(image.data(), image.size(), scene, &mismatches);
    std::printf("D3D10_KMT_IMAGE scene=%s pixels=256 bytes=1024 mismatches=%u\n", sceneName(scene), mismatches);
    require(matched, "strict-readback-pixel-oracle"); ++m_images;
  }
};

template<typename Table> void execute(KmtComputeTransport& transport, NamedCore& core,
    const SystemCompiler& compiler, const std::wstring& directory, unsigned& draws, unsigned& pixels) {
  DeviceSession<Table> session(transport, core.open);
  try {
    session.create(); session.render(directory, compiler);
    const HRESULT hr = session.close(); require(hr == S_OK, "typed10-owned-teardown", hr);
    require(transport.balanced(), "real-kmt-ownership-balance");
  } catch (...) {
    draws = session.draws(); pixels = session.pixels(); throw;
  }
  draws = session.draws(); pixels = session.pixels();
}
} // namespace

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Profile profile = Profile::D3D10_0;
  std::array<std::uint8_t, 32> coreHash{};
  std::array<std::uint8_t, sizeof(LUID)> luidBytes{};
  if (argc != 6 || !decodeHex(argv[2], coreHash) || !decodeHex(argv[3], luidBytes) || !parseProfile(argv[4], profile)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d10-hardware-probe <absolute core DLL> <SHA256> <16 hex LUID bytes> <10_0|10_1> <fresh absolute output directory>\n"); return 2;
  }
  LUID luid{}; std::memcpy(&luid, luidBytes.data(), sizeof(luid));
  if (!luid.LowPart && !luid.HighPart) { std::fprintf(stderr, "selected hardware LUID must be nonzero\n"); return 2; }
  std::printf("D3D10_KMT_SELECTED profile=%s luid=", profile == Profile::D3D10_0 ? "10_0" : "10_1"); printHex(&luid, sizeof(luid)); std::printf("\n");
  HRESULT hr = S_OK; bool completed = false; unsigned draws = 0, pixels = 0;
  try {
    const auto corePath = absolutePath(argv[1]), directory = absolutePath(argv[5]);
    const BOOL created = CreateDirectoryW(directory.c_str(), nullptr);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    require(created != FALSE, "fresh-output-directory", HRESULT_FROM_WIN32(createError));
    retain(directory, L"hardware-original.hlsl", Hlsl, sizeof(Hlsl) - 1);
    NamedCore core(corePath, coreHash); SystemCompiler compiler; KmtComputeTransport transport;
    try {
      hr = transport.open(luid); require(hr == S_OK, "real-selected-kmt-open", hr);
      if (profile == Profile::D3D10_0) execute<D3D10DDI_DEVICEFUNCS>(transport, core, compiler, directory, draws, pixels);
      else execute<D3D10_1DDI_DEVICEFUNCS>(transport, core, compiler, directory, draws, pixels);
      completed = true;
    } catch (...) {
      transport.printCounts(); const HRESULT close = transport.close();
      std::printf("D3D10_KMT_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(close)); throw;
    }
    transport.printCounts(); const bool balanced = transport.balanced();
    const HRESULT closed = transport.close();
    std::printf("D3D10_KMT_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(closed));
    require(balanced && closed == S_OK, "raw-kmt-close", closed == S_OK ? E_FAIL : closed);
    core.verifyRetained(); compiler.verifyRetained();
  } catch (const Failure& failure) { completed = false; hr = failure.hr; std::printf("D3D10_KMT_FAILURE stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(hr)); }
    catch (const std::bad_alloc&) { completed = false; hr = E_OUTOFMEMORY; std::printf("D3D10_KMT_FAILURE stage=allocation-exception\n"); }
    catch (...) { completed = false; hr = E_FAIL; std::printf("D3D10_KMT_FAILURE stage=exception\n"); }
  std::printf("D3D10_KMT_%s profile=%s draws=%u pixels=%u hr=%08lx ordinary_runtime_admission=0\n",
    completed ? "PASS" : "FAIL", profile == Profile::D3D10_0 ? "10_0" : "10_1", draws, pixels, static_cast<unsigned long>(hr));
  return completed ? 0 : 1;
}
