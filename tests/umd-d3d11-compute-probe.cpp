#include "umd-kmt-compute-transport.h"
#include "umd-compute-oracle.h"
#include "umd_build_config.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace {

using namespace dxvk::umd::probe;

// DXGI table selection follows the runtime revision in the low Version word.
// The minimum D3D11 build alone (revision zero) still selects DXGI 1.0.
constexpr UINT RuntimeVersion = (D3D11_0_DDI_BUILD_VERSION << 16) | DXGI_RESOLVE_SHARED_RESOURCE;
static_assert(IS_DXGI1_1_BASE_FUNCTIONS(D3D11_0_DDI_INTERFACE_VERSION, RuntimeVersion));
static_assert(!IS_DXGI1_1_BASE_FUNCTIONS(D3D11_0_DDI_INTERFACE_VERSION, RuntimeVersion - 1));

struct ProbeFailure { const char* stage; HRESULT result; };
void require(bool value, const char* stage, HRESULT hr = E_FAIL) {
  if (!value) throw ProbeFailure{stage, hr};
}

// Bound both ends of every runtime-owned output and private storage area.
// The memory handed to the UMD remains alive until its matching Destroy DDI.
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
class PrivateStorage {
public:
  void allocate(SIZE_T size) {
    require(size && size <= 16 * 1024 * 1024, "private-storage-size");
    m_size = size;
    m_bytes.reset(new unsigned char[size + 32]);
    std::memset(m_bytes.get(), 0xa5, size + 32);
    std::memset(data(), 0, size);
  }
  void* data() { return m_bytes ? m_bytes.get() + 16 : nullptr; }
  bool intact() const {
    if (!m_bytes) return true;
    for (SIZE_T i = 0; i < 16; ++i)
      if (m_bytes[i] != 0xa5 || m_bytes[m_size + 16 + i] != 0xa5) return false;
    return true;
  }
  bool zero() const {
    if (!m_bytes) return true;
    for (SIZE_T i = 0; i < m_size; ++i) if (m_bytes[i + 16]) return false;
    return true;
  }
private:
  std::unique_ptr<unsigned char[]> m_bytes;
  SIZE_T m_size = 0;
};

bool parseLuid(const WCHAR* text, LUID& luid) {
  if (std::wcslen(text) != sizeof(LUID) * 2) return false;
  unsigned char bytes[sizeof(LUID)]{};
  for (unsigned i = 0; i < sizeof(LUID) * 2; ++i) {
    const WCHAR c = text[i];
    unsigned digit = 0;
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

void printLuid(const LUID& luid) {
  const auto bytes = reinterpret_cast<const unsigned char*>(&luid);
  for (unsigned i = 0; i < sizeof(LUID); ++i) std::printf("%02x", unsigned(bytes[i]));
}

void writeOriginal(const std::wstring& directory, const WCHAR* name, const void* data, size_t size) {
  require(size <= MAXDWORD, "original-output-size");
  const std::wstring path = directory + L"\\" + name;
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "original-output-create", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0;
  const BOOL saved = WriteFile(file, data, DWORD(size), &written, nullptr);
  const DWORD error = saved ? ERROR_SUCCESS : GetLastError();
  const BOOL closed = CloseHandle(file);
  const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
  require(saved && written == size && closed, "original-output-write", error ? HRESULT_FROM_WIN32(error)
    : closeError ? HRESULT_FROM_WIN32(closeError) : E_FAIL);
}

const char ComputeSource[] = R"(
RWStructuredBuffer<uint4> dst:register(u0);
uint pack(uint3 value){return value.x+(value.y<<8)+(value.z<<16);}
[numthreads(2,3,2)]void main(uint3 dispatch:SV_DispatchThreadID,uint3 group:SV_GroupID,
    uint3 thread:SV_GroupThreadID,uint flat:SV_GroupIndex){
  dst[(dispatch.z*6+dispatch.y)*4+dispatch.x]=uint4(pack(dispatch),pack(group),pack(thread),flat);
})";

struct ReleaseBlob { void operator()(ID3DBlob* blob) const { if (blob) blob->Release(); } };
std::vector<UINT> compileCompute(const std::wstring& directory) {
  writeOriginal(directory, L"compute.hlsl", ComputeSource, std::strlen(ComputeSource));
  ID3DBlob* binary = nullptr;
  ID3DBlob* diagnostics = nullptr;
  const HRESULT hr = D3DCompile(ComputeSource, std::strlen(ComputeSource), "typed11-kmt-compute", nullptr,
    nullptr, "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &binary, &diagnostics);
  const std::unique_ptr<ID3DBlob, ReleaseBlob> binaryOwner(binary), diagnosticsOwner(diagnostics);
  if (diagnostics) {
    writeOriginal(directory, L"compiler-diagnostics.txt", diagnostics->GetBufferPointer(), diagnostics->GetBufferSize());
    std::fprintf(stderr, "%.*s\n", int(diagnostics->GetBufferSize()), static_cast<const char*>(diagnostics->GetBufferPointer()));
  }
  require(hr == S_OK && binary, "compile-cs-5-0", hr == S_OK ? E_FAIL : hr);
  const auto bytes = static_cast<const unsigned char*>(binary->GetBufferPointer());
  const size_t size = binary->GetBufferSize();
  writeOriginal(directory, L"compute-original.dxbc", bytes, size);
  require(size >= 32 && size <= MAXDWORD, "dxbc-size");
  auto word = [&](size_t offset) {
    require(offset <= size - 4, "dxbc-word-range");
    UINT value = 0; std::memcpy(&value, bytes + offset, 4); return value;
  };
  require(word(0) == 0x43425844 && word(20) == 1 && word(24) == size, "dxbc-header");
  const UINT chunks = word(28);
  require(chunks && chunks <= (size - 32) / 4, "dxbc-chunk-table");
  std::vector<UINT> code;
  for (UINT i = 0; i < chunks; ++i) {
    const UINT offset = word(32 + size_t(i) * 4);
    require(offset >= 32 + size_t(chunks) * 4 && offset <= size - 8 && !(offset % 4), "dxbc-chunk-range");
    const UINT length = word(offset + 4);
    require(length <= size - offset - 8, "dxbc-chunk-size");
    if (word(offset) != 0x58454853) continue;
    require(code.empty() && length >= 8 && !(length % 4), "compute-shex-shape");
    code.resize(length / 4); std::memcpy(code.data(), bytes + offset + 8, length);
  }
  require(code.size() >= 2 && code[0] == 0x00050050 && code[1] == code.size(), "compute-shex-profile");
  writeOriginal(directory, L"compute-original.shex", code.data(), code.size() * 4);
  std::printf("D3D11_KMT_COMPUTE_SHADER profile=cs_5_0 words=%zu signatures=runtime-compute-empty\n", code.size());
  return code;
}

class DeviceSession {
public:
  explicit DeviceSession(KmtComputeTransport& transport) : m_transport(transport) {
    transport.callbacks(m_adapterCallbacks, m_kernelCallbacks, m_coreCallbacks);
  }
  ~DeviceSession() { const HRESULT hr = close(); if (hr != S_OK) std::printf("D3D11_KMT_CLEANUP hr=%08lx\n", static_cast<unsigned long>(hr)); }
  DeviceSession(const DeviceSession&) = delete;
  DeviceSession& operator=(const DeviceSession&) = delete;

  void create() {
    require(VioGpuDxvkOpenAdapter10_2ForTest(nullptr) == E_INVALIDARG, "modern-open-null-rejected");
    D3D10DDIARG_OPENADAPTER opened{};
    opened.hRTAdapter = m_transport.runtimeAdapter(); opened.pAdapterCallbacks = &m_adapterCallbacks;
    opened.pAdapterFuncs_2 = &m_adapterFunctions.value;
    const HRESULT hr = VioGpuDxvkOpenAdapter10_2ForTest(&opened);
    std::printf("D3D11_KMT_OPEN_MODERN hr=%08lx\n", static_cast<unsigned long>(hr));
    require(hr == S_OK && opened.hAdapter.pDrvPrivate, "modern-open", hr == S_OK ? E_FAIL : hr);
    m_adapter = opened.hAdapter;
    require(m_adapterFunctions.intact() && m_adapterFunctions.value.pfnCalcPrivateDeviceSize
      && m_adapterFunctions.value.pfnCreateDevice && m_adapterFunctions.value.pfnCloseAdapter, "modern-adapter-table");
    D3D10DDIARG_CALCPRIVATEDEVICESIZE size{};
    size.Interface = D3D11_0_DDI_INTERFACE_VERSION; size.Version = RuntimeVersion;
    size.Flags = UINT(D3D11DDI_3DPIPELINELEVEL_11_0) << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
    require(IS_DXGI1_1_BASE_FUNCTIONS(size.Interface, size.Version), "dxgi-1-1-abi");
    m_deviceStorage.allocate(m_adapterFunctions.value.pfnCalcPrivateDeviceSize(m_adapter, &size));
    m_device.pDrvPrivate = m_deviceStorage.data();
    D3D10DDIARG_CREATEDEVICE args{};
    args.Interface = size.Interface; args.Version = size.Version; args.Flags = size.Flags;
    args.hDrvDevice = m_device; args.hRTDevice = m_transport.runtimeDevice(); args.hRTCoreLayer = m_transport.runtimeCore();
    args.pKTCallbacks = &m_kernelCallbacks; args.p11UMCallbacks = &m_coreCallbacks; args.p11DeviceFuncs = &m_deviceFunctions.value;
    args.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &m_dxgiFunctions.value;
    rejectCreationInputs(args);
    m_deviceFunctions.value = {}; m_dxgiFunctions.value = {};
    const HRESULT created = m_adapterFunctions.value.pfnCreateDevice(m_adapter, &args);
    std::printf("D3D11_KMT_CREATE_TYPED hr=%08lx interface=%08x version=%08x flags=%08x\n",
      static_cast<unsigned long>(created), args.Interface, args.Version, args.Flags);
    require(created == S_OK, "typed11-device-create", created);
    m_deviceAlive = true;
    require(guards() && m_deviceFunctions.value.pfnDestroyDevice && m_deviceFunctions.value.pfnCreateComputeShader
      && m_deviceFunctions.value.pfnCalcPrivateShaderSize && m_deviceFunctions.value.pfnCsSetShader
      && m_deviceFunctions.value.pfnCreateResource && m_deviceFunctions.value.pfnCalcPrivateResourceSize
      && m_deviceFunctions.value.pfnDestroyResource && m_deviceFunctions.value.pfnCalcPrivateUnorderedAccessViewSize
      && m_deviceFunctions.value.pfnCreateUnorderedAccessView && m_deviceFunctions.value.pfnDestroyUnorderedAccessView
      && m_deviceFunctions.value.pfnCsSetUnorderedAccessViews && m_deviceFunctions.value.pfnDispatch
      && m_deviceFunctions.value.pfnResourceCopy && m_deviceFunctions.value.pfnStagingResourceMap
      && m_deviceFunctions.value.pfnStagingResourceUnmap && m_deviceFunctions.value.pfnDestroyShader
      && m_deviceFunctions.value.pfnFlush, "typed11-required-slots");
  }

  void compute(const std::wstring& directory) {
    const auto code = compileCompute(directory);
    auto& api = m_deviceFunctions.value;
    m_transport.beginDdi();
    m_shaderStorage.allocate(api.pfnCalcPrivateShaderSize(m_device, code.data(), nullptr));
    check("private-shader-size"); m_shader.pDrvPrivate = m_shaderStorage.data();
    m_transport.beginDdi(); api.pfnCreateComputeShader(m_device, code.data(), m_shader, {});
    m_shaderAlive = m_transport.lastError() == S_OK; check("create-compute-shader");

    std::array<ComputeElement, ComputeElementCount> poison;
    for (auto& element : poison) element.fill(0xcdcdcdcd);
    createBuffer(m_bufferStorage, m_buffer, m_bufferAlive, false, poison.data());
    createBuffer(m_stagingStorage, m_staging, m_stagingAlive, true, nullptr);
    D3D11DDIARG_CREATEUNORDEREDACCESSVIEW view{};
    view.hDrvResource = m_buffer; view.Format = DXGI_FORMAT_UNKNOWN; view.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
    view.Buffer = {0, ComputeElementCount, 0};
    m_transport.beginDdi(); m_uavStorage.allocate(api.pfnCalcPrivateUnorderedAccessViewSize(m_device, &view));
    check("private-uav-size"); m_uav.pDrvPrivate = m_uavStorage.data();
    m_transport.beginDdi(); api.pfnCreateUnorderedAccessView(m_device, &view, m_uav, {});
    m_uavAlive = m_transport.lastError() == S_OK; check("create-structured-uav");
    m_transport.beginDdi(); api.pfnCsSetShader(m_device, m_shader); check("bind-compute-shader");
    m_transport.beginDdi(); api.pfnCsSetUnorderedAccessViews(m_device, 0, 1, &m_uav, nullptr); check("bind-compute-uav");
    m_transport.beginDdi(); api.pfnDispatch(m_device, 2, 2, 2); check("dispatch-2-2-2");
    m_transport.beginDdi(); api.pfnResourceCopy(m_device, m_staging, m_buffer); check("copy-readback-buffer");
    m_transport.beginDdi(); api.pfnFlush(m_device); check("flush-compute-copy");
    Guarded<D3D10DDI_MAPPED_SUBRESOURCE> mapped;
    m_transport.beginDdi(); api.pfnStagingResourceMap(m_device, m_staging, 0, D3D10_DDI_MAP_READ, 0, &mapped.value);
    m_mapped = m_transport.lastError() == S_OK; check("map-compute-readback");
    require(mapped.intact() && mapped.value.pData, "mapped-output-guard");
    std::array<ComputeElement, ComputeElementCount> readback{};
    static_assert(sizeof(readback) == 1536);
    std::memcpy(readback.data(), mapped.value.pData, sizeof(readback));
    m_transport.beginDdi(); api.pfnStagingResourceUnmap(m_device, m_staging, 0);
    m_mapped = m_transport.lastError() != S_OK;
    check("unmap-compute-readback");
    writeOriginal(directory, L"compute-readback.raw", readback.data(), sizeof(readback));
    unsigned mismatches = 0;
    for (UINT element = 0; element < ComputeElementCount; ++element) {
      const auto expected = expectedComputeElement(element);
      for (UINT component = 0; component < 4; ++component) {
        const UINT actual = readback[element][component];
        mismatches += actual != expected[component];
        std::printf("D3D11_KMT_COMPUTE_WORD element=%u component=%u actual=%08x expected=%08x\n",
          element, component, actual, expected[component]);
      }
    }
    std::printf("D3D11_KMT_COMPUTE_READBACK elements=96 words=384 bytes=1536 mismatches=%u\n", mismatches);
    require(!mismatches && computeReadbackMatches(readback), "compute-readback-oracle");
  }

  HRESULT close() noexcept {
    HRESULT hr = S_OK;
    auto keep = [&](HRESULT error) { if (hr == S_OK && error != S_OK) hr = error; };
    auto complete = [&] { keep(m_transport.lastError()); if (!guards()) keep(E_FAIL); };
    auto& api = m_deviceFunctions.value;
    if (m_deviceAlive) {
      if (m_mapped && api.pfnStagingResourceUnmap) {
        m_transport.beginDdi(); api.pfnStagingResourceUnmap(m_device, m_staging, 0); m_mapped = false; complete();
      }
      if (api.pfnCsSetUnorderedAccessViews) {
        const D3D11DDI_HUNORDEREDACCESSVIEW empty{};
        m_transport.beginDdi(); api.pfnCsSetUnorderedAccessViews(m_device, 0, 1, &empty, nullptr); complete();
      }
      if (api.pfnCsSetShader) { m_transport.beginDdi(); api.pfnCsSetShader(m_device, {}); complete(); }
      if (m_uavAlive && api.pfnDestroyUnorderedAccessView) {
        m_transport.beginDdi(); api.pfnDestroyUnorderedAccessView(m_device, m_uav); m_uavAlive = false; complete();
      }
      if (m_shaderAlive && api.pfnDestroyShader) {
        m_transport.beginDdi(); api.pfnDestroyShader(m_device, m_shader); m_shaderAlive = false; complete();
      }
      if (m_stagingAlive && api.pfnDestroyResource) {
        m_transport.beginDdi(); api.pfnDestroyResource(m_device, m_staging); m_stagingAlive = false; complete();
      }
      if (m_bufferAlive && api.pfnDestroyResource) {
        m_transport.beginDdi(); api.pfnDestroyResource(m_device, m_buffer); m_bufferAlive = false; complete();
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
  D3DDDI_ADAPTERCALLBACKS m_adapterCallbacks{};
  D3DDDI_DEVICECALLBACKS m_kernelCallbacks{};
  // Microsoft permits callback addresses to change in this live table. Its
  // storage outlives the UMD device, including failure and teardown paths.
  D3D11DDI_CORELAYER_DEVICECALLBACKS m_coreCallbacks{};
  Guarded<D3D10_2DDI_ADAPTERFUNCS> m_adapterFunctions;
  Guarded<D3D11DDI_DEVICEFUNCS> m_deviceFunctions;
  Guarded<DXGI1_1_DDI_BASE_FUNCTIONS> m_dxgiFunctions;
  D3D10DDI_HADAPTER m_adapter{};
  D3D10DDI_HDEVICE m_device{};
  D3D10DDI_HSHADER m_shader{};
  D3D10DDI_HRESOURCE m_buffer{}, m_staging{};
  D3D11DDI_HUNORDEREDACCESSVIEW m_uav{};
  PrivateStorage m_deviceStorage, m_shaderStorage, m_bufferStorage, m_stagingStorage, m_uavStorage;
  bool m_deviceAlive = false, m_shaderAlive = false, m_bufferAlive = false, m_stagingAlive = false;
  bool m_uavAlive = false, m_mapped = false;

  bool guards() const {
    return m_adapterFunctions.intact() && m_deviceFunctions.intact() && m_dxgiFunctions.intact()
      && m_deviceStorage.intact() && m_shaderStorage.intact() && m_bufferStorage.intact()
      && m_stagingStorage.intact() && m_uavStorage.intact();
  }
  void check(const char* stage) {
    const HRESULT hr = m_transport.lastError();
    require(hr == S_OK && guards(), stage, hr == S_OK ? E_FAIL : hr);
  }
  void rejectCreationInputs(const D3D10DDIARG_CREATEDEVICE& valid) {
    std::memset(&m_deviceFunctions.value, 0xcd, sizeof(m_deviceFunctions.value));
    std::memset(&m_dxgiFunctions.value, 0xcd, sizeof(m_dxgiFunctions.value));
    std::array<unsigned char, sizeof(D3D11DDI_DEVICEFUNCS)> beforeDevice;
    std::array<unsigned char, sizeof(DXGI1_1_DDI_BASE_FUNCTIONS)> beforeDxgi;
    std::memcpy(beforeDevice.data(), &m_deviceFunctions.value, beforeDevice.size());
    std::memcpy(beforeDxgi.data(), &m_dxgiFunctions.value, beforeDxgi.size());
    const unsigned beforeQueries = m_transport.counts.queries;
    for (unsigned test = 0; test < 8; ++test) {
      auto args = valid;
      D3D11DDI_CORELAYER_DEVICECALLBACKS missingError{};
      HRESULT expected = E_INVALIDARG;
      switch (test) {
        case 0: args.Interface = D3D11_1_DDI_INTERFACE_VERSION; expected = DXGI_ERROR_UNSUPPORTED; break;
        case 1: args.Version -= 1u << 16; expected = DXGI_ERROR_UNSUPPORTED; break;
        case 2: args.Flags |= 0x80000000u; expected = DXGI_ERROR_UNSUPPORTED; break;
        case 3: args.pKTCallbacks = nullptr; break;
        case 4: args.p11UMCallbacks = nullptr; break;
        case 5: args.p11UMCallbacks = &missingError; break;
        case 6: args.hRTDevice = {}; break;
        case 7: args.p11DeviceFuncs = nullptr; break;
      }
      m_transport.beginDdi();
      const HRESULT hr = m_adapterFunctions.value.pfnCreateDevice(m_adapter, &args);
      std::printf("D3D11_KMT_NEGATIVE_CREATE case=%u hr=%08lx expected=%08lx\n",
        test, static_cast<unsigned long>(hr), static_cast<unsigned long>(expected));
      require(hr == expected && m_transport.lastError() == S_OK && guards()
        && !std::memcmp(&m_deviceFunctions.value, beforeDevice.data(), beforeDevice.size())
        && !std::memcmp(&m_dxgiFunctions.value, beforeDxgi.data(), beforeDxgi.size())
        && m_deviceStorage.zero() && m_transport.counts.queries == beforeQueries
        && !m_transport.counts.contexts && !m_transport.counts.allocations, "typed11-reject-invalid-input");
    }
  }
  void createBuffer(PrivateStorage& storage, D3D10DDI_HRESOURCE& handle, bool& alive, bool staging, const void* initial) {
    constexpr UINT bytes = ComputeElementCount * sizeof(ComputeElement);
    D3D10DDI_MIPINFO shape{bytes, 1, 1, bytes, 1, 1};
    D3D10_DDIARG_SUBRESOURCE_UP data{const_cast<void*>(initial), bytes, bytes};
    D3D11DDIARG_CREATERESOURCE desc{};
    desc.pMipInfoList = &shape; desc.pInitialDataUP = initial ? &data : nullptr;
    desc.ResourceDimension = D3D10DDIRESOURCE_BUFFER; desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    desc.BindFlags = staging ? 0 : D3D11_DDI_BIND_UNORDERED_ACCESS;
    desc.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    desc.MiscFlags = staging ? 0 : D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.ByteStride = staging ? 0 : UINT(sizeof(ComputeElement));
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    auto& api = m_deviceFunctions.value;
    m_transport.beginDdi(); storage.allocate(api.pfnCalcPrivateResourceSize(m_device, &desc)); check("private-buffer-size");
    handle.pDrvPrivate = storage.data();
    m_transport.beginDdi(); api.pfnCreateResource(m_device, &desc, handle, {});
    alive = m_transport.lastError() == S_OK; check(staging ? "create-staging-buffer" : "create-structured-buffer");
  }
};

} // namespace

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  LUID luid{};
  if (argc != 3 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d11-compute-probe <16 hex LUID bytes> <fresh output directory>\n");
    return 2;
  }
  std::printf("D3D11_KMT_COMPUTE_SELECTED luid="); printLuid(luid); std::printf("\n");
  KmtComputeTransport transport;
  HRESULT hr = S_OK;
  bool completed = false;
  try {
    WCHAR modulePath[32768]{};
    const HMODULE module = GetModuleHandleW(VIOGPU_DXVK_UMD_FILE);
    const DWORD length = module ? GetModuleFileNameW(module, modulePath, DWORD(std::size(modulePath))) : 0;
    require(length && length < std::size(modulePath), "original-core-module");
    std::printf("D3D11_KMT_COMPUTE_CORE path=%ls\n", modulePath);
    const std::wstring directory = argv[2];
    const BOOL created = CreateDirectoryW(directory.c_str(), nullptr);
    const DWORD directoryError = created ? ERROR_SUCCESS : GetLastError();
    require(created, "fresh-output-directory", directoryError ? HRESULT_FROM_WIN32(directoryError) : E_FAIL);
    hr = transport.open(luid); require(hr == S_OK, "real-kmt-open", hr);
    DeviceSession device(transport); device.create(); device.compute(directory);
    hr = device.close(); require(hr == S_OK, "typed11-owned-teardown", hr);
    require(transport.balanced(), "actual-kernel-ownership-balance");
    completed = true;
  } catch (const ProbeFailure& failure) {
    hr = failure.result; std::printf("D3D11_KMT_COMPUTE_FAILURE stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(hr));
  } catch (const std::bad_alloc&) {
    hr = E_OUTOFMEMORY; std::printf("D3D11_KMT_COMPUTE_FAILURE stage=allocation-exception hr=%08lx\n", static_cast<unsigned long>(hr));
  } catch (...) {
    hr = E_FAIL; std::printf("D3D11_KMT_COMPUTE_FAILURE stage=exception hr=%08lx\n", static_cast<unsigned long>(hr));
  }
  transport.printCounts();
  const bool balanced = completed && transport.balanced();
  const HRESULT closed = transport.close();
  std::printf("D3D11_KMT_COMPUTE_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(closed));
  if (hr == S_OK && closed != S_OK) hr = closed;
  const bool passed = completed && balanced && hr == S_OK;
  std::printf("D3D11_KMT_COMPUTE_%s elements=96 words=384 mismatches=%s guards=%u balanced=%u hr=%08lx; development adapter, ordinary_runtime_admission=0\n",
    passed ? "PASS" : "FAIL", completed ? "0" : "unverified", UINT(completed), UINT(balanced), static_cast<unsigned long>(hr));
  return passed ? 0 : 1;
}
