// SPDX-License-Identifier: MIT
#include "umd-kmt-graphics-probe.h"
#include "umd-cube-probe-oracle.h"

namespace {
using namespace dxvk::umd::probe;
using namespace dxvk::umd::probe::graphics;
namespace oracle = dxvk::umd::probe::cube;
unsigned cases, readbacks, texels, sampled, sampleRuns;

struct Description {
  oracle::Shape shape;
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D11DDIARG_CREATERESOURCE args{};
  explicit Description(oracle::Shape value) : shape(value), mips(value.levels) {
    for (unsigned mip = 0; mip < value.levels; ++mip) {
      const unsigned edge = oracle::extent(value, mip);
      mips[mip] = {edge, edge, 1, (edge + 15u) & ~15u, (edge + 3u) & ~3u, 1};
    }
    args.pMipInfoList = mips.data(); args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE;
    args.Usage = D3D10_DDI_USAGE_DEFAULT; args.Format = DXGI_FORMAT_R32_FLOAT;
    args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
    args.SampleDesc.Count = 1; args.MipLevels = value.levels; args.ArraySize = value.faces;
  }
  Description(const Description&) = delete;
  Description& operator=(const Description&) = delete;
};
struct Pitched {
  std::vector<std::vector<unsigned char>> bytes;
  std::vector<D3D10_DDIARG_SUBRESOURCE_UP> rows;
  Pitched(const Description& description, const oracle::Words& words) : bytes(words.size()), rows(words.size()) {
    require(words.size() == description.shape.faces * description.shape.levels, "cube-initial-subresource-count");
    for (unsigned i = 0; i < words.size(); ++i) {
      const unsigned edge = oracle::extent(description.shape, i % description.shape.levels);
      rows[i].SysMemPitch = edge * 4 + 12; rows[i].SysMemSlicePitch = rows[i].SysMemPitch * edge + 20;
      bytes[i].resize(rows[i].SysMemSlicePitch, 0xcd); rows[i].pSysMem = bytes[i].data();
      for (unsigned y = 0; y < edge; ++y)
        std::memcpy(bytes[i].data() + size_t(y) * rows[i].SysMemPitch, words[i].data() + y * edge, edge * 4);
    }
  }
};
class View {
  static D3D11DDIARG_CREATESHADERRESOURCEVIEW desc(Resource& resource, unsigned firstFace,
      unsigned cubes, unsigned mip, unsigned count) {
    D3D11DDIARG_CREATESHADERRESOURCEVIEW args{};
    args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R32_FLOAT;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE; args.TexCube = {mip, count, firstFace, cubes};
    return args;
  }
public:
  View(Session& s, Resource& resource, unsigned firstFace, unsigned cubes, unsigned mip, unsigned count)
  : owner(s), description(desc(resource, firstFace, cubes, mip, count)),
    storage(s.size([&] { return s.api().pfnCalcPrivateShaderResourceViewSize(s.device(), &description); }, "cube-SRV-private-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateShaderResourceView(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("cube-SRV-create"); require(storage.intact(), "cube-SRV-private-guards"); }
    catch (...) { close(); throw; }
  }
  ~View() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroyShaderResourceView(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  View(const View&) = delete;
  View& operator=(const View&) = delete;
  Session& owner; D3D11DDIARG_CREATESHADERRESOURCEVIEW description;
  Storage storage; D3D10DDI_HSHADERRESOURCEVIEW handle;
private:
  bool alive = false;
};
class PointSampler {
public:
  explicit PointSampler(Session& s) : owner(s), description(makeDesc()),
    storage(s.size([&] { return s.api().pfnCalcPrivateSamplerSize(s.device(), &description); }, "cube-sampler-private-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateSampler(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("cube-sampler-create"); require(storage.intact(), "cube-sampler-private-guards"); }
    catch (...) { close(); throw; }
  }
  ~PointSampler() { close(); }
  void close() noexcept {
    if (alive) {
      owner.transport().beginDdi(); owner.api().pfnDestroySampler(owner.device(), handle);
      owner.cleanupResult(storage.intact()); alive = false;
    }
  }
  PointSampler(const PointSampler&) = delete;
  PointSampler& operator=(const PointSampler&) = delete;
  Session& owner; D3D10_DDI_SAMPLER_DESC description;
  Storage storage; D3D10DDI_HSAMPLER handle;
private:
  bool alive = false;
  static D3D10_DDI_SAMPLER_DESC makeDesc() {
    D3D10_DDI_SAMPLER_DESC result{};
    result.Filter = D3D10_DDI_FILTER_MIN_MAG_MIP_POINT;
    result.AddressU = result.AddressV = result.AddressW = D3D10_DDI_TEXTURE_ADDRESS_CLAMP;
    result.MaxAnisotropy = 1; result.ComparisonFunc = D3D10_DDI_COMPARISON_NEVER; result.MaxLOD = D3D11_FLOAT32_MAX;
    return result;
  }
};
class Uav {
  static D3D11DDIARG_CREATEUNORDEREDACCESSVIEW desc(Resource& resource) {
    D3D11DDIARG_CREATEUNORDEREDACCESSVIEW args{};
    args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_UNKNOWN;
    args.ResourceDimension = D3D10DDIRESOURCE_BUFFER; args.Buffer = {0, 64, 0};
    return args;
  }
public:
  Uav(Session& s, Resource& resource) : owner(s), description(desc(resource)),
    storage(s.size([&] { return s.api().pfnCalcPrivateUnorderedAccessViewSize(s.device(), &description); }, "cube-UAV-size")),
    handle{storage.data()} {
    owner.transport().beginDdi(); owner.api().pfnCreateUnorderedAccessView(owner.device(), &description, handle, {});
    alive = owner.transport().lastError() == S_OK;
    try { owner.check("cube-UAV-create"); require(storage.intact(), "cube-UAV-private-guards"); }
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

void readCube(Session& s, Resource& resource, const Description& description, const oracle::Words& expected,
    const std::wstring& directory) {
  auto args = description.args;
  args.Usage = D3D10_DDI_USAGE_STAGING; args.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  args.BindFlags = args.MiscFlags = 0; args.pInitialDataUP = nullptr;
  Resource staging(s, args);
  s.call([&] { s.api().pfnResourceCopy(s.device(), staging.handle, resource.handle); }, "cube-copy-staging");
  s.call([&] { s.api().pfnFlush(s.device()); }, "cube-readback-flush");
  const unsigned index = readbacks++;
  for (unsigned face = 0; face < description.shape.faces; ++face) for (unsigned mip = 0; mip < description.shape.levels; ++mip) {
    const unsigned edge = oracle::extent(description.shape, mip), subresource = face * description.shape.levels + mip;
    const auto map = staging.map(subresource); require(map.RowPitch >= edge * 4, "cube-map-row-pitch");
    std::vector<UINT> actual(edge * edge);
    for (unsigned y = 0; y < edge; ++y)
      std::memcpy(actual.data() + y * edge, static_cast<const char*>(map.pData) + size_t(y) * map.RowPitch, edge * 4);
    staging.unmap();
    const UINT fields[] = {index, description.shape.edge, description.shape.levels, description.shape.faces,
      face, mip, edge, map.RowPitch, map.DepthPitch};
    const auto name = L"cube-readback-" + std::to_wstring(index) + L"-face-" + std::to_wstring(face) + L"-mip-" + std::to_wstring(mip);
    writeOriginal(directory, name + L".actual.u32", actual.data(), actual.size() * 4);
    writeOriginal(directory, name + L".expected.u32", expected[subresource].data(), expected[subresource].size() * 4);
    writeOriginal(directory, name + L".shape-pitches.u32", fields, sizeof(fields));
    require(oracle::matches(actual, expected[subresource]), "entire-cube-face-mip-oracle"); texels += unsigned(actual.size());
  }
  std::printf("D3D11_KMT_CUBE_READBACK index=%u faces=%u levels=%u edge=%u mismatches=0\n",
    index, description.shape.faces, description.shape.levels, description.shape.edge);
}
constexpr char SampleSource[] = R"(
TextureCubeArray<float> source:register(t0);SamplerState pointSampler:register(s0);
RWStructuredBuffer<uint> result:register(u0);
cbuffer Shape:register(b0){uint cubes;uint mip;uint2 padding;}
[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){
if(id.x>=6*cubes)return;
const float3 directions[6]={float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
result[id.x]=asuint(source.SampleLevel(pointSampler,float4(directions[id.x%6],id.x/6),mip));})";
class Sampler {
public:
  Sampler(Session& s, const std::wstring& directory) : owner(s), source(compileShader(directory,
    L"cube-point-cs", SampleSource, "cs_5_0", 5)), shader(s, source, 5), point(s) {}
  void sample(View& view, oracle::Shape shape, const oracle::Words& values, unsigned firstFace,
      unsigned cubes, unsigned firstMip, unsigned relativeMip, const std::wstring& directory) {
    const unsigned count = cubes * 6; require(count <= 64 && count, "cube-sample-capacity");
    std::vector<UINT> poison(64, 0xcdcdcdcdu);
    D3D10_DDIARG_SUBRESOURCE_UP initial{poison.data(), 256, 256};
    D3D10DDI_MIPINFO bufferShape{}, constantShape{};
    auto args = bufferDescription(bufferShape, 256, D3D11_DDI_BIND_UNORDERED_ACCESS, &initial);
    args.MiscFlags = D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED; args.ByteStride = 4;
    Resource buffer(owner, args); Uav target(owner, buffer);
    UINT constants[] = {cubes, relativeMip, 0, 0};
    D3D10_DDIARG_SUBRESOURCE_UP constantData{constants, sizeof(constants), sizeof(constants)};
    Resource constant(owner, bufferDescription(constantShape, sizeof(constants), D3D10_DDI_BIND_CONSTANT_BUFFER, &constantData));
    shader.bind(); owner.call([&] {
      owner.api().pfnCsSetShaderResources(owner.device(), 0, 1, &view.handle);
      owner.api().pfnCsSetSamplers(owner.device(), 0, 1, &point.handle);
      owner.api().pfnCsSetConstantBuffers(owner.device(), 0, 1, &constant.handle);
      owner.api().pfnCsSetUnorderedAccessViews(owner.device(), 0, 1, &target.handle, nullptr);
      owner.api().pfnDispatch(owner.device(), count, 1, 1);
    }, "sample-native-cube-SRV");
    const D3D10DDI_HSHADERRESOURCEVIEW emptyView{}; const D3D10DDI_HSAMPLER emptySampler{};
    const D3D10DDI_HRESOURCE emptyResource{}; const D3D11DDI_HUNORDEREDACCESSVIEW emptyUav{};
    owner.call([&] {
      owner.api().pfnCsSetUnorderedAccessViews(owner.device(), 0, 1, &emptyUav, nullptr);
      owner.api().pfnCsSetShaderResources(owner.device(), 0, 1, &emptyView);
      owner.api().pfnCsSetSamplers(owner.device(), 0, 1, &emptySampler);
      owner.api().pfnCsSetConstantBuffers(owner.device(), 0, 1, &emptyResource);
    }, "unbind-cube-sampler");
    const auto actual = readBuffer(owner, buffer, 64);
    auto expected = oracle::sampled(shape, values, firstFace, cubes, firstMip, relativeMip);
    expected.resize(64, 0xcdcdcdcdu);
    const auto name = L"cube-sampled-" + std::to_wstring(sampleRuns);
    const UINT fields[] = {sampleRuns, shape.edge, shape.levels, shape.faces, firstFace, cubes, firstMip, relativeMip, 64, count};
    writeOriginal(directory, name + L".actual.u32", actual.data(), actual.size() * 4);
    writeOriginal(directory, name + L".expected.u32", expected.data(), expected.size() * 4);
    writeOriginal(directory, name + L".shape-relative-mip.u32", fields, sizeof(fields));
    require(oracle::matches(actual, expected), "cube-sampled-words-and-untouched-tail"); sampled += count;
    std::printf("D3D11_KMT_CUBE_SAMPLED index=%u count=%u first_face=%u first_mip=%u relative_mip=%u raw_words=64 mismatches=0\n",
      sampleRuns++, count, firstFace, firstMip, relativeMip);
  }
private:
  Session& owner; ShaderSource source; Shader shader; PointSampler point;
};

void cubeCases(Session& s, const std::wstring& directory) {
  require(s.api().pfnCalcPrivateShaderResourceViewSize && s.api().pfnCreateShaderResourceView && s.api().pfnDestroyShaderResourceView
    && s.api().pfnCalcPrivateSamplerSize && s.api().pfnCreateSampler && s.api().pfnDestroySampler
    && s.api().pfnCreateComputeShader && s.api().pfnCsSetShader && s.api().pfnCsSetShaderResources && s.api().pfnCsSetSamplers
    && s.api().pfnCsSetConstantBuffers && s.api().pfnCsSetUnorderedAccessViews && s.api().pfnDispatch && s.api().pfnGenMips
    && s.api().pfnCalcPrivateUnorderedAccessViewSize && s.api().pfnCreateUnorderedAccessView && s.api().pfnDestroyUnorderedAccessView
    && s.api().pfnResourceUpdateSubresourceUP && s.api().pfnResourceCopyRegion, "typed11-cube-slots");
  Sampler sampler(s, directory);
  {
    Description d({7, 3, 12}); const auto expected = oracle::initial(d.shape); Pitched data(d, expected);
    d.args.Usage = D3D10_DDI_USAGE_IMMUTABLE; d.args.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
    d.args.pInitialDataUP = data.rows.data(); Resource resource(s, d.args);
    readCube(s, resource, d, expected, directory); ++cases;
    for (const auto& range : {std::array<unsigned, 4>{0, 1, 0, 3}, {6, 1, 1, 2}, {0, 2, 0, 3}}) {
      View view(s, resource, range[0], range[1], range[2], UINT32_MAX);
      for (unsigned relative = 0; relative < range[3]; ++relative)
        sampler.sample(view, d.shape, expected, range[0], range[1], range[2], relative, directory);
    }
  }
  {
    Description d({7, 3, 12}); const auto original = oracle::initial(d.shape); Pitched data(d, original);
    d.args.pInitialDataUP = data.rows.data(); Resource source(s, d.args), destination(s, d.args);
    const D3D10_DDI_BOX patch{0, 0, 0, 2, 2, 1}, from{1, 1, 0, 3, 3, 1};
    std::array<UINT, 8> padded{}; padded.fill(0xcdcdcdcdu);
    padded[0] = padded[1] = padded[4] = padded[5] = oracle::bits(123456);
    s.call([&] { s.api().pfnResourceUpdateSubresourceUP(s.device(), destination.handle, 7 * 3 + 1, &patch,
      padded.data(), 16, 32); }, "pitched-selected-cube-face-upload");
    s.call([&] { s.api().pfnResourceCopyRegion(s.device(), destination.handle, 11 * 3, 3, 2, 0,
      source.handle, 0, &from); }, "boxed-copy-between-cube-faces");
    readCube(s, destination, d, oracle::transferred(), directory); ++cases;
  }
  for (const auto& range : {std::array<unsigned, 2>{6, UINT32_MAX}, {0, 2}, {6, 1}}) {
    Description d({8, 4, 12}); const auto initial = oracle::initial(d.shape, true); Pitched data(d, initial);
    d.args.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP; d.args.pInitialDataUP = data.rows.data();
    Resource resource(s, d.args); View view(s, resource, range[0], 1, 1, range[1]);
    s.call([&] { s.api().pfnGenMips(s.device(), view.handle); }, "scoped-selected-cube-generate-mips");
    const auto expected = oracle::generated(range[0], range[1]); readCube(s, resource, d, expected, directory); ++cases;
    const unsigned levels = range[1] == UINT32_MAX ? 3 : range[1];
    for (unsigned relative = 0; relative < levels; ++relative)
      sampler.sample(view, d.shape, expected, range[0], 1, 1, relative, directory);
  }
  {
    Description d({7, 3, 6}); const auto expected = oracle::initial(d.shape); Pitched data(d, expected);
    d.args.pInitialDataUP = data.rows.data(); Resource resource(s, d.args);
    readCube(s, resource, d, expected, directory); ++cases;
    View view(s, resource, 0, 1, 0, 3);
    for (unsigned relative = 0; relative < 3; ++relative)
      sampler.sample(view, d.shape, expected, 0, 1, 0, relative, directory);
  }
  require(cases == 6 && readbacks == 6 && texels == 4830 && sampled == 120 && sampleRuns == 17, "complete-cube-oracle-coverage");
  s.call([&] { s.api().pfnCsSetShader(s.device(), {}); }, "unbind-cube-compute-shader");
}
} // namespace

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0); LUID luid{};
  if (argc != 3 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d11-cube-probe <16 hex LUID bytes> <fresh output directory>\n"); return 2;
  }
  KmtComputeTransport transport; HRESULT hr = S_OK; bool completed = false;
  std::printf("D3D11_KMT_CUBE_SELECTED luid="); printLuid(luid); std::printf("\n");
  try {
    WCHAR path[32768]{}; const HMODULE core = GetModuleHandleW(VIOGPU_DXVK_UMD_FILE);
    const DWORD length = core ? GetModuleFileNameW(core, path, DWORD(std::size(path))) : 0;
    require(length && length < std::size(path), "original-core-module");
    std::printf("D3D11_KMT_CUBE_CORE path=%ls\n", path);
    const std::wstring directory = argv[2]; const BOOL created = CreateDirectoryW(directory.c_str(), nullptr);
    require(created, "fresh-original-directory", created ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
    hr = transport.open(luid); require(hr == S_OK, "real-kmt-open", hr);
    Session session(transport); session.create(); cubeCases(session, directory);
    hr = session.close(); require(hr == S_OK, "typed11-owned-teardown", hr);
    require(transport.balanced(), "real-kernel-ownership-balance"); completed = true;
  } catch (const Failure& failure) {
    hr = failure.result; std::printf("D3D11_KMT_CUBE_FAILURE stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(hr));
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
  catch (...) { hr = E_FAIL; }
  transport.printCounts(); const bool balanced = completed && transport.balanced(); const HRESULT closed = transport.close();
  std::printf("D3D11_KMT_CUBE_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(closed));
  if (hr == S_OK && closed != S_OK) hr = closed;
  const bool passed = completed && balanced && hr == S_OK;
  std::printf("D3D11_KMT_CUBE_%s cases=%u readbacks=%u texels=%u sampled=%u sample_runs=%u balanced=%u hr=%08lx ordinary_runtime_admission=0\n",
    passed ? "PASS" : "FAIL", cases, readbacks, texels, sampled, sampleRuns, unsigned(balanced), static_cast<unsigned long>(hr));
  return passed ? 0 : 1;
}
