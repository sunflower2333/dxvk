// SPDX-License-Identifier: MIT
#include "umd-kmt-graphics-probe.h"
#include "umd-volume-probe-oracle.h"
#include <algorithm>

namespace {
using namespace dxvk::umd::probe;
using namespace dxvk::umd::probe::graphics;
namespace oracle = dxvk::umd::probe::volume;

unsigned readbacks, voxels, sampled, cases, negatives;
struct Description {
  oracle::Extent base;
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D11DDIARG_CREATERESOURCE args{};
  Description(oracle::Extent shape, unsigned levels) : base(shape), mips(levels) {
    require(levels && levels <= 16, "volume-description-levels");
    for (unsigned mip = 0; mip < levels; ++mip) {
      const auto m = oracle::extent(base, mip);
      mips[mip] = {m.width, m.height, m.depth, (m.width + 15u) & ~15u, (m.height + 3u) & ~3u, (m.depth + 3u) & ~3u};
    }
    args.pMipInfoList = mips.data(); args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE3D;
    args.Usage = D3D10_DDI_USAGE_DEFAULT; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
    args.SampleDesc.Count = 1; args.MipLevels = levels; args.ArraySize = 1;
  }
  Description(const Description&) = delete;
  Description& operator=(const Description&) = delete;
};
// Preserve the accepted original volume reference's independent row/slice
// padding, distinct from the logical and physical dimensions supplied to DDI.
struct Pitched {
  std::vector<std::vector<unsigned char>> bytes;
  std::vector<D3D10_DDIARG_SUBRESOURCE_UP> rows;
  Pitched(const Description& description, const oracle::Words& data) : bytes(data.size()), rows(data.size()) {
    require(data.size() == description.mips.size(), "pitched-volume-levels");
    for (unsigned mip = 0; mip < data.size(); ++mip) {
      const auto m = oracle::extent(description.base, mip); auto& row = rows[mip];
      row.SysMemPitch = m.width * 4 + 12; row.SysMemSlicePitch = row.SysMemPitch * m.height + 20;
      bytes[mip].resize(size_t(row.SysMemSlicePitch) * m.depth, 0xcd); row.pSysMem = bytes[mip].data();
      for (unsigned z = 0; z < m.depth; ++z) for (unsigned y = 0; y < m.height; ++y)
        std::memcpy(bytes[mip].data() + size_t(z) * row.SysMemSlicePitch + y * row.SysMemPitch,
          data[mip].data() + oracle::offset(m, 0, y, z), m.width * 4);
    }
  }
};

class ShaderView {
  static D3D11DDIARG_CREATESHADERRESOURCEVIEW makeDesc(Resource& resource, unsigned mip, unsigned count) {
    D3D11DDIARG_CREATESHADERRESOURCEVIEW args{};
    args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE3D; args.Tex3D = {mip, count};
    return args;
  }
public:
  ShaderView(Session& s, Resource& resource, unsigned mip, unsigned count) : owner(s),
    description(makeDesc(resource, mip, count)),
    storage(s.size([&] { return s.api().pfnCalcPrivateShaderResourceViewSize(s.device(), &description); }, "volume-SRV-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateShaderResourceView(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("volume-SRV-create"); require(storage.intact(), "volume-SRV-guards"); }
    catch (...) { close(); throw; }
  }
  ~ShaderView() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyShaderResourceView(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  ShaderView(const ShaderView&) = delete;
  ShaderView& operator=(const ShaderView&) = delete;
  Session& owner; D3D11DDIARG_CREATESHADERRESOURCEVIEW description;
  Storage storage; D3D10DDI_HSHADERRESOURCEVIEW handle;
private:
  bool alive = false;
};
class Target {
  static D3D10DDIARG_CREATERENDERTARGETVIEW makeDesc(Resource& resource, unsigned mip, unsigned first, unsigned count) {
    D3D10DDIARG_CREATERENDERTARGETVIEW args{};
    args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE3D; args.Tex3D = {mip, first, count};
    return args;
  }
public:
  Target(Session& s, Resource& resource, unsigned mip, unsigned first, unsigned count) : owner(s),
    description(makeDesc(resource, mip, first, count)),
    storage(s.size([&] { return s.api().pfnCalcPrivateRenderTargetViewSize(s.device(), &description); }, "volume-RTV-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateRenderTargetView(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("volume-RTV-create"); require(storage.intact(), "volume-RTV-guards"); }
    catch (...) { close(); throw; }
  }
  ~Target() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyRenderTargetView(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  Target(const Target&) = delete;
  Target& operator=(const Target&) = delete;
  Session& owner; D3D10DDIARG_CREATERENDERTARGETVIEW description;
  Storage storage; D3D10DDI_HRENDERTARGETVIEW handle;
private:
  bool alive = false;
};
class Uav {
  static D3D11DDIARG_CREATEUNORDEREDACCESSVIEW makeDesc(Resource& buffer, unsigned count) {
    D3D11DDIARG_CREATEUNORDEREDACCESSVIEW args{};
    args.hDrvResource = buffer.handle; args.Format = DXGI_FORMAT_UNKNOWN;
    args.ResourceDimension = D3D10DDIRESOURCE_BUFFER; args.Buffer = {0, count, 0};
    return args;
  }
public:
  Uav(Session& s, Resource& buffer, unsigned count) : owner(s),
    description(makeDesc(buffer, count)),
    storage(s.size([&] { return s.api().pfnCalcPrivateUnorderedAccessViewSize(s.device(), &description); }, "volume-readback-UAV-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateUnorderedAccessView(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("volume-readback-UAV-create"); require(storage.intact(), "volume-UAV-guards"); }
    catch (...) { close(); throw; }
  }
  ~Uav() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyUnorderedAccessView(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  Uav(const Uav&) = delete;
  Uav& operator=(const Uav&) = delete;
  Session& owner; D3D11DDIARG_CREATEUNORDEREDACCESSVIEW description;
  Storage storage; D3D11DDI_HUNORDEREDACCESSVIEW handle;
private:
  bool alive = false;
};

void readVolume(Session& s, Resource& resource, const Description& description, const oracle::Words& expected,
    const std::wstring& directory) {
  auto args = description.args;
  args.Usage = D3D10_DDI_USAGE_STAGING; args.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  args.BindFlags = args.MiscFlags = 0; args.pInitialDataUP = nullptr;
  Resource staging(s, args);
  s.call([&] { s.api().pfnResourceCopy(s.device(), staging.handle, resource.handle); }, "volume-copy-staging");
  s.call([&] { s.api().pfnFlush(s.device()); }, "volume-copy-flush");
  const unsigned index = readbacks++;
  for (unsigned mip = 0; mip < expected.size(); ++mip) {
    const auto m = oracle::extent(description.base, mip); const auto map = staging.map(mip);
    require(map.RowPitch >= m.width * 4 && (m.depth == 1
      || map.DepthPitch >= uint64_t(m.height - 1) * map.RowPitch + m.width * 4), "volume-map-pitches");
    std::vector<UINT> actual(expected[mip].size());
    for (unsigned z = 0; z < m.depth; ++z) for (unsigned y = 0; y < m.height; ++y) for (unsigned x = 0; x < m.width; ++x)
      std::memcpy(&actual[oracle::offset(m, x, y, z)], static_cast<const char*>(map.pData)
        + size_t(z) * map.DepthPitch + size_t(y) * map.RowPitch + x * 4, 4);
    staging.unmap();
    const UINT fields[] = {index, mip, m.width, m.height, m.depth, map.RowPitch, map.DepthPitch};
    const auto name = L"volume-readback-" + std::to_wstring(index) + L"-mip-" + std::to_wstring(mip);
    writeOriginal(directory, name + L".actual.u32", actual.data(), actual.size() * 4);
    writeOriginal(directory, name + L".expected.u32", expected[mip].data(), expected[mip].size() * 4);
    writeOriginal(directory, name + L".dimensions-pitches.u32", fields, sizeof(fields));
    require(oracle::matches(actual, expected[mip]), "entire-volume-voxel-oracle");
    voxels += unsigned(actual.size());
    std::printf("D3D11_KMT_VOLUME_READBACK index=%u mip=%u xyz=%u,%u,%u words=%zu row_pitch=%u depth_pitch=%u mismatches=0\n",
      index, mip, m.width, m.height, m.depth, actual.size(), map.RowPitch, map.DepthPitch);
  }
}

template<typename Fn> void reject(Session& s, const char* name, Fn&& fn) {
  const HRESULT error = s.negative(std::forward<Fn>(fn), E_INVALIDARG, name); ++negatives;
  std::printf("D3D11_KMT_VOLUME_NEGATIVE index=%u stage=%s hr=%08lx\n", negatives, name, static_cast<unsigned long>(error));
}

constexpr char SampleSource[] = R"(
Texture3D<float4> source:register(t0);RWStructuredBuffer<uint> result:register(u0);
cbuffer Shape:register(b0){uint width;uint height;uint depth;uint mip;}
[numthreads(4,4,4)]void main(uint3 id:SV_DispatchThreadID){
if(any(id>=uint3(width,height,depth)))return;
uint4 c=(uint4)round(saturate(source.Load(int4(id,mip)))*255.0);
result[(id.z*height+id.y)*width+id.x]=c.x|(c.y<<8)|(c.z<<16)|(c.w<<24);})";
class Sampler {
public:
  Sampler(Session& s, const std::wstring& directory) : owner(s), source(compileShader(directory,
      L"volume-load-cs", SampleSource, "cs_5_0", 5)), shader(s, source, 5) {}
  void sample(ShaderView& view, oracle::Extent size, unsigned relativeMip, const std::vector<UINT>& expected,
      const std::wstring& directory) {
    const UINT count = size.width * size.height * size.depth;
    require(count && count == expected.size(), "volume-sample-size");
    std::vector<UINT> poison(count, 0xcdcdcdcdu);
    D3D10_DDIARG_SUBRESOURCE_UP initial{poison.data(), count * 4, count * 4};
    D3D10DDI_MIPINFO bufferShape{}, constantShape{};
    auto args = bufferDescription(bufferShape, count * 4, D3D11_DDI_BIND_UNORDERED_ACCESS, &initial);
    args.MiscFlags = D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED; args.ByteStride = 4;
    Resource buffer(owner, args); Uav target(owner, buffer, count);
    const UINT shape[] = {size.width, size.height, size.depth, relativeMip};
    D3D10_DDIARG_SUBRESOURCE_UP constantData{const_cast<UINT*>(shape), sizeof(shape), sizeof(shape)};
    Resource constant(owner, bufferDescription(constantShape, sizeof(shape), D3D10_DDI_BIND_CONSTANT_BUFFER, &constantData));
    shader.bind(); owner.call([&] {
      owner.api().pfnCsSetShaderResources(owner.device(), 0, 1, &view.handle);
      owner.api().pfnCsSetConstantBuffers(owner.device(), 0, 1, &constant.handle);
      owner.api().pfnCsSetUnorderedAccessViews(owner.device(), 0, 1, &target.handle, nullptr);
      owner.api().pfnDispatch(owner.device(), (size.width + 3) / 4, (size.height + 3) / 4, (size.depth + 3) / 4);
    }, "sample-native-volume-SRV");
    const D3D10DDI_HRESOURCE emptyResource{}; const D3D10DDI_HSHADERRESOURCEVIEW emptyView{};
    const D3D11DDI_HUNORDEREDACCESSVIEW emptyUav{};
    owner.call([&] {
      owner.api().pfnCsSetUnorderedAccessViews(owner.device(), 0, 1, &emptyUav, nullptr);
      owner.api().pfnCsSetShaderResources(owner.device(), 0, 1, &emptyView);
      owner.api().pfnCsSetConstantBuffers(owner.device(), 0, 1, &emptyResource);
    }, "unbind-native-volume-sampler");
    const auto actual = readBuffer(owner, buffer, count);
    const auto name = L"volume-sampled-" + std::to_wstring(samples++);
    writeOriginal(directory, name + L".actual.u32", actual.data(), actual.size() * 4);
    writeOriginal(directory, name + L".expected.u32", expected.data(), expected.size() * 4);
    writeOriginal(directory, name + L".shape-relative-mip.u32", shape, sizeof(shape));
    require(oracle::matches(actual, expected), "native-compute-volume-load-oracle"); sampled += count;
    std::printf("D3D11_KMT_VOLUME_SAMPLED index=%u words=%u relative_mip=%u mismatches=0\n", samples - 1, count, relativeMip);
  }
private:
  Session& owner; ShaderSource source; Shader shader; unsigned samples = 0;
};

void volumeCases(Session& s, const std::wstring& directory) {
  require(s.api().pfnCalcPrivateShaderResourceViewSize && s.api().pfnCreateShaderResourceView && s.api().pfnDestroyShaderResourceView
    && s.api().pfnCalcPrivateRenderTargetViewSize && s.api().pfnCreateRenderTargetView && s.api().pfnDestroyRenderTargetView
    && s.api().pfnCalcPrivateUnorderedAccessViewSize && s.api().pfnCreateUnorderedAccessView && s.api().pfnDestroyUnorderedAccessView
    && s.api().pfnCreateComputeShader && s.api().pfnCsSetShader && s.api().pfnCsSetShaderResources && s.api().pfnCsSetConstantBuffers
    && s.api().pfnCsSetUnorderedAccessViews && s.api().pfnDispatch && s.api().pfnGenMips && s.api().pfnClearRenderTargetView
    && s.api().pfnDynamicResourceMapDiscard && s.api().pfnDynamicResourceUnmap && s.api().pfnResourceMap
    && s.api().pfnResourceUpdateSubresourceUP && s.api().pfnResourceCopyRegion, "typed11-volume-slots");
  Sampler sampler(s, directory);
  {
    Description d({9, 5, 7}, 4); const auto expected = oracle::initial(d.base, 4); Pitched data(d, expected);
    d.args.Usage = D3D10_DDI_USAGE_IMMUTABLE; d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
    d.args.pInitialDataUP = data.rows.data(); Resource resource(s, d.args);
    readVolume(s, resource, d, expected, directory); ++cases;
    ShaderView view(s, resource, 0, UINT32_MAX);
    for (unsigned mip = 0; mip < 4; ++mip) sampler.sample(view, oracle::extent(d.base, mip), mip, expected[mip], directory);
  }
  {
    Description d({9, 5, 7}, 4), patch({3, 2, 3}, 1);
    const auto original = oracle::initial(d.base, 4), values = oracle::initial(patch.base, 1);
    Pitched data(d, original), input(patch, values); d.args.pInitialDataUP = data.rows.data();
    Resource source(s, d.args), destination(s, d.args);
    const D3D10_DDI_BOX box{2, 1, 2, 5, 3, 5}, from{1, 1, 1, 4, 3, 3};
    s.call([&] { s.api().pfnResourceUpdateSubresourceUP(s.device(), destination.handle, 0, &box,
      input.rows[0].pSysMem, input.rows[0].SysMemPitch, input.rows[0].SysMemSlicePitch); }, "pitched-three-slice-upload");
    s.call([&] { s.api().pfnResourceCopyRegion(s.device(), destination.handle, 0, 5, 2, 4, source.handle, 0, &from); }, "boxed-volume-copy");
    reject(s, "volume-copy-destination-depth", [&] { s.api().pfnResourceCopyRegion(s.device(), destination.handle, 0,
      0, 0, UINT32_MAX, source.handle, 0, &from); });
    reject(s, "volume-upload-row-too-short", [&] { s.api().pfnResourceUpdateSubresourceUP(s.device(), destination.handle,
      0, &box, input.rows[0].pSysMem, 11, 64); });
    reject(s, "volume-upload-slice-overlap", [&] { s.api().pfnResourceUpdateSubresourceUP(s.device(), destination.handle,
      0, &box, input.rows[0].pSysMem, 24, 35); });
    readVolume(s, destination, d, oracle::transferred(), directory); ++cases;
  }
  {
    Description d({7, 3, 5}, 1); d.args.Usage = D3D10_DDI_USAGE_DYNAMIC;
    d.args.MapFlags = D3D10_DDI_CPU_ACCESS_WRITE; d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
    Resource resource(s, d.args);
    Guarded<D3D10DDI_MAPPED_SUBRESOURCE> invalid;
    invalid.value = {reinterpret_cast<void*>(uintptr_t(1)), 17, 19};
    reject(s, "volume-dynamic-invalid-mip", [&] { s.api().pfnResourceMap(s.device(), resource.handle, 1,
      D3D10_DDI_MAP_WRITE_DISCARD, 0, &invalid.value); });
    require(invalid.intact() && !invalid.value.pData && !invalid.value.RowPitch && !invalid.value.DepthPitch, "failed-map-bounded-zero-output");
    const UINT64 rejectedMap[] = {UINT64(reinterpret_cast<uintptr_t>(invalid.value.pData)),
      invalid.value.RowPitch, invalid.value.DepthPitch, UINT64(invalid.intact())};
    writeOriginal(directory, L"invalid-dynamic-map-output.u64", rejectedMap, sizeof(rejectedMap));
    for (unsigned round = 0; round < 3; ++round) {
      const auto expected = oracle::dynamic(round); const auto map = resource.discard();
      require(map.RowPitch >= 28 && map.DepthPitch >= uint64_t(map.RowPitch) * 2 + 28, "dynamic-volume-pitches");
      for (unsigned z = 0; z < 5; ++z) for (unsigned y = 0; y < 3; ++y)
        std::memcpy(static_cast<char*>(map.pData) + size_t(z) * map.DepthPitch + size_t(y) * map.RowPitch,
          expected[0].data() + oracle::offset(d.base, 0, y, z), 28);
      resource.unmap(); readVolume(s, resource, d, expected, directory); ++cases;
    }
  }
  {
    Description d({8, 4, 8}, 3); const auto initial = oracle::initial(d.base, 3); Pitched data(d, initial);
    d.args.pInitialDataUP = data.rows.data(); Resource resource(s, d.args);
    Target selected(s, resource, 1, 1, 1), remaining(s, resource, 1, 2, UINT32_MAX);
    FLOAT red[] = {1, 0, 0, 1}, green[] = {0, 1, 0, 1};
    s.call([&] { s.api().pfnClearRenderTargetView(s.device(), selected.handle, red);
      s.api().pfnClearRenderTargetView(s.device(), remaining.handle, green); }, "selected-and-remaining-W-slice-clears");
    readVolume(s, resource, d, oracle::cleared(), directory); ++cases;
  }
  for (UINT count : {UINT32_MAX, 2u, 1u}) {
    Description d({8, 8, 8}, 4); const auto initial = oracle::mipInput(); Pitched data(d, initial);
    d.args.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP; d.args.pInitialDataUP = data.rows.data();
    Resource resource(s, d.args); ShaderView view(s, resource, 1, count);
    s.call([&] { s.api().pfnGenMips(s.device(), view.handle); }, "scoped-volume-generate-mips");
    const auto expected = oracle::generated(count); readVolume(s, resource, d, expected, directory); ++cases;
    const unsigned levels = count == UINT32_MAX ? 3 : count;
    for (unsigned relative = 0; relative < levels; ++relative)
      sampler.sample(view, oracle::extent(d.base, 1 + relative), relative, expected[1 + relative], directory);
  }
  require(cases == 9 && readbacks == 9 && voxels == 3046 && sampled == 551 && negatives == 4, "complete-volume-oracle-coverage");
}
} // namespace

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  LUID luid{};
  if (argc != 3 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d11-volume-probe <16 hex LUID bytes> <fresh output directory>\n"); return 2;
  }
  KmtComputeTransport transport; HRESULT hr = S_OK; bool completed = false; unsigned acceptedErrors = 0;
  std::printf("D3D11_KMT_VOLUME_SELECTED luid="); printLuid(luid); std::printf("\n");
  try {
    WCHAR path[32768]{}; const HMODULE core = GetModuleHandleW(VIOGPU_DXVK_UMD_FILE);
    const DWORD length = core ? GetModuleFileNameW(core, path, DWORD(std::size(path))) : 0;
    require(length && length < std::size(path), "original-core-module");
    std::printf("D3D11_KMT_VOLUME_CORE path=%ls\n", path);
    const std::wstring directory = argv[2]; const BOOL created = CreateDirectoryW(directory.c_str(), nullptr);
    require(created, "fresh-original-directory", created ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
    hr = transport.open(luid); require(hr == S_OK, "real-kmt-open", hr);
    Session session(transport); session.create();
    try { volumeCases(session, directory); }
    catch (...) { acceptedErrors = session.acceptedErrors(); throw; }
    acceptedErrors = session.acceptedErrors();
    hr = session.close(); require(hr == S_OK, "typed11-owned-teardown", hr);
    require(transport.balanced(acceptedErrors), "actual-kernel-ownership-balance"); completed = true;
  } catch (const Failure& failure) {
    hr = failure.result; std::printf("D3D11_KMT_VOLUME_FAILURE stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(hr));
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
  catch (...) { hr = E_FAIL; }
  transport.printCounts(); const bool balanced = completed && transport.balanced(acceptedErrors);
  const HRESULT closed = transport.close();
  std::printf("D3D11_KMT_VOLUME_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(closed));
  if (hr == S_OK && closed != S_OK) hr = closed;
  const bool passed = completed && balanced && hr == S_OK;
  std::printf("D3D11_KMT_VOLUME_%s cases=%u readbacks=%u voxels=%u sampled=%u negatives=%u expected_core_errors=%u balanced=%u hr=%08lx ordinary_runtime_admission=0\n",
    passed ? "PASS" : "FAIL", cases, readbacks, voxels, sampled, negatives, acceptedErrors, unsigned(balanced), static_cast<unsigned long>(hr));
  return passed ? 0 : 1;
}
