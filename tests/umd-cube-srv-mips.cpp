// SPDX-License-Identifier: MIT
// Exact newer WDK tables backed by an explicit source-linked WARP reference.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_d3d11_desc.h"
#include "../src/umd/umd_result.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, alternateCallbacks, backendCalls, views, sampledWords;
static HRESULT lastError = S_OK;
static DWORD callerThread;
static const LUID expectedLuid = {0x53726491, -37};
static ComPtr<ID3D11DeviceContext> createdContext;

static void check(bool value, unsigned line, const char* expression) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "FAIL cube SRV mip ranges line=%u expression=%s checks=%u callbacks=%u HRESULT=%08lx\n",
      line, expression, checks, callbacks, static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x), __LINE__, #x)
static void ok() { CHECK(lastError == S_OK); }

static_assert(sizeof(D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW) == 4 * sizeof(UINT), "original WDK cube range ABI");
// Check the real SDK descriptor, including UINT overflow and unchanged caller
// output on failure. C++17 cannot switch the SDK's active anonymous-union
// member during constant evaluation, so the native fixture runs this control.
static bool descriptorContract() {
  D3D11_TEXTURE2D_DESC resource{};
  resource.MipLevels = 5; resource.ArraySize = 18;
  resource.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  resource.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
  D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW native{1, UINT(-1), 6, 2};
  D3D11_SHADER_RESOURCE_VIEW_DESC output{};
  if (!dxvk::umd::cubeArrayShaderView11Desc(native, DXGI_FORMAT_R32_FLOAT, resource, output)
      || output.Format != DXGI_FORMAT_R32_FLOAT
      || output.ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBEARRAY
      || output.TextureCubeArray.MostDetailedMip != 1 || output.TextureCubeArray.MipLevels != 4
      || output.TextureCubeArray.First2DArrayFace != 6 || output.TextureCubeArray.NumCubes != 2) return false;
  const auto retained = output;
  for (const auto& invalid : std::array<D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW, 8>{
      {{5, UINT(-1), 0, 1}, {UINT(-1), UINT(-1), 0, 1}, {1, 5, 0, 1},
       {1, UINT(-2), 0, 1}, {1, UINT(-1), 1, 1}, {1, UINT(-1), 18, 1},
       {1, UINT(-1), 0, 0}, {1, UINT(-1), 0, UINT(-1)}}}) {
    if (dxvk::umd::cubeArrayShaderView11Desc(invalid, DXGI_FORMAT_R32_FLOAT, resource, output)
        || std::memcmp(&output, &retained, sizeof(output))) return false;
  }
  native = {4, 1, 12, 1};
  return dxvk::umd::cubeArrayShaderView11Desc(native, DXGI_FORMAT_R32_FLOAT, resource, output)
    && output.TextureCubeArray.MostDetailedMip == 4 && output.TextureCubeArray.MipLevels == 1
    && output.TextureCubeArray.First2DArrayFace == 12 && output.TextureCubeArray.NumCubes == 1;
}

struct Storage {
  static constexpr UINT64 canary = 0xe7d2a81bc365940full;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T size) : words((size + 7) / 8 + 2, 0) {
    CHECK(size); words.front() = words.back() = canary;
  }
  void* data() { return words.data() + 1; }
  void poison() { std::fill(words.begin() + 1, words.end() - 1, 0xcdcdcdcdcdcdcdcdull); }
  void guards() const { CHECK(words.front() == canary && words.back() == canary); }
};
static const Storage* failedStorage;
static const std::vector<UINT64>* failedOriginal;
static ID3D11DeviceContext* failedContext;
static ID3D11ShaderResourceView* retainedBinding;

static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(result));
  ++callbacks; lastError = result;
  if (failedStorage) {
    CHECK(failedOriginal && failedStorage->words == *failedOriginal);
    failedStorage->guards();
    ComPtr<ID3D11ShaderResourceView> bound;
    failedContext->PSGetShaderResources(0, 1, &bound);
    CHECK(bound.Get() == retainedBinding);
  }
}
static void APIENTRY alternateError(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  ++alternateCallbacks; error(runtime, result);
}

HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && !std::memcmp(&luid, &expectedLuid, sizeof(luid)));
  ++backendCalls; level = implementationFeatureLevel(level);
  const HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (result == S_OK) createdContext = *context;
  return result;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

template<bool Modern> struct Fixture {
  using Table = std::conditional_t<Modern, D3D11DDI_DEVICEFUNCS, D3D10_1DDI_DEVICEFUNCS>;
  using Core = std::conditional_t<Modern, D3D11DDI_CORELAYER_DEVICECALLBACKS, D3D10DDI_CORELAYER_DEVICECALLBACKS>;
  using ResourceArgs = std::conditional_t<Modern, D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  using ViewArgs = std::conditional_t<Modern, D3D11DDIARG_CREATESHADERRESOURCEVIEW, D3D10_1DDIARG_CREATESHADERRESOURCEVIEW>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  Core core{};
  struct { Table table{}; UINT64 canary = Storage::canary; } output;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  Fixture() {
    core.pfnSetErrorCb = error;
    if constexpr (Modern) {
      CHECK(VioGpuDxvkCreateDdiTestDevice11(&expectedLuid, device, {&core}, &core,
        &output.table, D3D_FEATURE_LEVEL_11_0) == S_OK);
    } else {
      CHECK(VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&core}, &core, &output.table) == S_OK);
    }
    CHECK(output.canary == Storage::canary);
    context = createdContext; createdContext.Reset(); CHECK(context);
    context->GetDevice(&backend); CHECK(backend);
  }
  ~Fixture() {
    context->ClearState(); context.Reset(); backend.Reset();
    output.table.pfnDestroyDevice(device); ok(); storage.guards();
    CHECK(output.canary == Storage::canary);
  }
};

// Every face/mip has an exactly representable float distinct from all others.
// The oracle uses absolute resource coordinates, independent of view translation.
static UINT texel(UINT absoluteFace, UINT absoluteMip) {
  const float value = float(1000u + 32u * absoluteFace + absoluteMip);
  UINT bits; std::memcpy(&bits, &value, sizeof(bits)); return bits;
}
template<bool Modern> struct Cube {
  Fixture<Modern>& f;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  UINT levels, faces;
  Cube(Fixture<Modern>& owner, UINT mipLevels, UINT arrayFaces, bool cube = true,
      UINT bind = D3D10_DDI_BIND_SHADER_RESOURCE)
      : f(owner), storage(f.output.table.pfnCalcPrivateResourceSize(f.device, nullptr)),
        handle{storage.data()}, levels(mipLevels), faces(arrayFaces) {
    std::vector<D3D10DDI_MIPINFO> shapes(levels);
    std::vector<std::vector<UINT>> pixels(size_t(levels) * faces);
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial(pixels.size());
    const UINT width = 1u << (levels - 1);
    for (UINT mip = 0; mip < levels; ++mip) {
      const UINT edge = width >> mip;
      shapes[mip] = {edge, edge, 1, (edge + 15u) & ~15u, (edge + 15u) & ~15u, 1};
      for (UINT face = 0; face < faces; ++face) {
        const UINT subresource = mip + face * levels;
        pixels[subresource].assign(size_t(edge) * edge, texel(face, mip));
        initial[subresource] = {pixels[subresource].data(), edge * UINT(sizeof(UINT)), edge * edge * UINT(sizeof(UINT))};
      }
    }
    typename Fixture<Modern>::ResourceArgs args{};
    args.pMipInfoList = shapes.data(); args.pInitialDataUP = initial.data();
    args.ResourceDimension = cube ? D3D10DDIRESOURCE_TEXTURECUBE : D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage = D3D10_DDI_USAGE_DEFAULT; args.BindFlags = bind;
    args.Format = DXGI_FORMAT_R32_FLOAT; args.SampleDesc.Count = 1;
    args.MipLevels = levels; args.ArraySize = faces;
    f.output.table.pfnCreateResource(f.device, &args, handle, {}); ok(); storage.guards();
    // Drop initial storage immediately, as the real runtime may do.
  }
  ~Cube() { f.output.table.pfnDestroyResource(f.device, handle); ok(); storage.guards(); }
};
template<bool Modern> static typename Fixture<Modern>::ViewArgs viewArgs(
    const Cube<Modern>& resource, const D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW& range) {
  typename Fixture<Modern>::ViewArgs args{};
  args.hDrvResource = resource.handle; args.Format = DXGI_FORMAT_R32_FLOAT;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURECUBE; args.TexCube = range;
  return args;
}
template<bool Modern> struct View {
  Fixture<Modern>& f;
  Storage storage;
  D3D10DDI_HSHADERRESOURCEVIEW handle;
  View(Fixture<Modern>& owner, const typename Fixture<Modern>::ViewArgs& args)
      : f(owner), storage(f.output.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args)),
        handle{storage.data()} {
    f.output.table.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok(); storage.guards();
  }
  ComPtr<ID3D11ShaderResourceView> bind() {
    f.output.table.pfnPsSetShaderResources(f.device, 0, 1, &handle); ok();
    ComPtr<ID3D11ShaderResourceView> bound;
    f.context->PSGetShaderResources(0, 1, &bound); CHECK(bound); return bound;
  }
  ~View() { f.output.table.pfnDestroyShaderResourceView(f.device, handle); ok(); storage.guards(); }
};
static void save(const char* profile, UINT index, const char* suffix, const void* data, size_t bytes) {
  char name[128];
  CHECK(std::snprintf(name, sizeof(name), "cube-srv-%s-%03u.%s", profile, index, suffix) > 0);
  std::ofstream output(name, std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
  output.close(); CHECK(!output.fail());
}

struct Sampler {
  ID3D11DeviceContext* context;
  ComPtr<ID3D11ComputeShader> shader;
  ComPtr<ID3D11SamplerState> sampler;
  ComPtr<ID3D11Buffer> constants, destination, staging;
  ComPtr<ID3D11UnorderedAccessView> output;
  static constexpr UINT capacity = 128;
  template<bool Modern> explicit Sampler(Fixture<Modern>& f) : context(f.context.Get()) {
    const char* source = R"(
TextureCubeArray<float> source : register(t0);
SamplerState pointSampler : register(s0);
RWStructuredBuffer<uint> result : register(u0);
cbuffer Shape : register(b0) { uint levels; uint cubes; uint2 padding; }
[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {
  if (id.x >= 6 * levels * cubes) return;
  const float3 directions[6] = {float3(1,0,0),float3(-1,0,0),float3(0,1,0),
    float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
  uint face = id.x % 6, mip = (id.x / 6) % levels, cube = id.x / (6 * levels);
  result[id.x] = asuint(source.SampleLevel(pointSampler, float4(directions[face], cube), mip));
})";
    ComPtr<ID3DBlob> binary, diagnostics;
    const HRESULT result = D3DCompile(source, std::strlen(source), "cube-srv-mips-reference", nullptr,
      nullptr, "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &binary, &diagnostics);
    if (FAILED(result) && diagnostics) std::fprintf(stderr, "%s\n", static_cast<const char*>(diagnostics->GetBufferPointer()));
    CHECK(result == S_OK);
    const char* profile = Modern ? "11" : "10_1";
    save(profile, 0, "hlsl", source, std::strlen(source));
    save(profile, 0, "dxbc", binary->GetBufferPointer(), binary->GetBufferSize());
    CHECK(f.backend->CreateComputeShader(binary->GetBufferPointer(), binary->GetBufferSize(), nullptr, &shader) == S_OK);
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX; sampling.MaxAnisotropy = 1; sampling.ComparisonFunc = D3D11_COMPARISON_NEVER;
    CHECK(f.backend->CreateSamplerState(&sampling, &sampler) == S_OK);
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = 16; buffer.Usage = D3D11_USAGE_DEFAULT; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &constants) == S_OK);
    buffer.ByteWidth = capacity * sizeof(UINT); buffer.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    buffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; buffer.StructureByteStride = sizeof(UINT);
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &destination) == S_OK);
    D3D11_UNORDERED_ACCESS_VIEW_DESC view{}; view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; view.Buffer.NumElements = capacity;
    CHECK(f.backend->CreateUnorderedAccessView(destination.Get(), &view, &output) == S_OK);
    buffer.Usage = D3D11_USAGE_STAGING; buffer.BindFlags = buffer.MiscFlags = buffer.StructureByteStride = 0;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CHECK(f.backend->CreateBuffer(&buffer, nullptr, &staging) == S_OK);
  }
  std::vector<UINT> sample(ID3D11ShaderResourceView* view, UINT levels, UINT cubes) {
    const UINT count = 6 * levels * cubes; CHECK(count <= capacity);
    const UINT shape[4] = {levels, cubes, 0, 0};
    const UINT poison[4] = {0xcdcdcdcdu, 0xcdcdcdcdu, 0xcdcdcdcdu, 0xcdcdcdcdu};
    context->ClearUnorderedAccessViewUint(output.Get(), poison);
    context->UpdateSubresource(constants.Get(), 0, nullptr, shape, 0, 0);
    context->CSSetShader(shader.Get(), nullptr, 0); context->CSSetShaderResources(0, 1, &view);
    auto boundSampler = sampler.Get(); auto constant = constants.Get(); auto target = output.Get();
    context->CSSetSamplers(0, 1, &boundSampler); context->CSSetConstantBuffers(0, 1, &constant);
    context->CSSetUnorderedAccessViews(0, 1, &target, nullptr); context->Dispatch(count, 1, 1);
    ID3D11UnorderedAccessView* emptyUav = nullptr; ID3D11ShaderResourceView* emptyView = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &emptyUav, nullptr); context->CSSetShaderResources(0, 1, &emptyView);
    context->CopyResource(staging.Get(), destination.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped) == S_OK); CHECK(mapped.pData);
    std::vector<UINT> data(capacity); std::memcpy(data.data(), mapped.pData, capacity * sizeof(UINT));
    context->Unmap(staging.Get(), 0);
    for (UINT i = count; i < capacity; ++i) CHECK(data[i] == poison[0]);
    data.resize(count); return data;
  }
};

template<bool Modern> static void validRanges(Fixture<Modern>& f, Cube<Modern>& cube) {
  Sampler sampler(f);
  const char* profile = Modern ? "11" : "10_1";
  UINT index = 0;
  for (UINT first = 0; first < cube.faces / 6; ++first)
    for (UINT cubes = 1; cubes <= cube.faces / 6 - first; ++cubes)
      for (UINT mip = 0; mip < cube.levels; ++mip)
        for (UINT requested : {1u, cube.levels - mip, UINT(-1)}) {
          const D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW range{mip, requested, first * 6, cubes};
          View<Modern> native(f, viewArgs(cube, range)); auto bound = native.bind();
          D3D11_SHADER_RESOURCE_VIEW_DESC observed{}; bound->GetDesc(&observed);
          const UINT levels = requested == UINT(-1) ? cube.levels - mip : requested;
          CHECK(observed.Format == DXGI_FORMAT_R32_FLOAT && observed.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY);
          CHECK(observed.TextureCubeArray.MostDetailedMip == mip && observed.TextureCubeArray.MipLevels == levels
            && observed.TextureCubeArray.First2DArrayFace == first * 6 && observed.TextureCubeArray.NumCubes == cubes);
          ComPtr<ID3D11Resource> resource; bound->GetResource(&resource); CHECK(resource);
          ComPtr<ID3D11Texture2D> texture; CHECK(resource.As(&texture) == S_OK);
          D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
          CHECK(desc.ArraySize == cube.faces && desc.MipLevels == cube.levels
            && desc.MiscFlags == D3D11_RESOURCE_MISC_TEXTURECUBE);
          // Ask the original public runtime to interpret the requested sentinel
          // separately; do not pass it the production helper's descriptor.
          D3D11_SHADER_RESOURCE_VIEW_DESC publicDesc{}; publicDesc.Format = DXGI_FORMAT_R32_FLOAT;
          publicDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
          publicDesc.TextureCubeArray = {mip, requested, first * 6, cubes};
          ComPtr<ID3D11ShaderResourceView> reference;
          CHECK(f.backend->CreateShaderResourceView(resource.Get(), &publicDesc, &reference) == S_OK);
          D3D11_SHADER_RESOURCE_VIEW_DESC publicObserved{}; reference->GetDesc(&publicObserved);
          CHECK(publicObserved.Format == observed.Format && publicObserved.ViewDimension == observed.ViewDimension);
          CHECK(publicObserved.TextureCubeArray.MostDetailedMip == mip && publicObserved.TextureCubeArray.MipLevels == levels
            && publicObserved.TextureCubeArray.First2DArrayFace == first * 6 && publicObserved.TextureCubeArray.NumCubes == cubes);
          const auto nativeData = sampler.sample(bound.Get(), levels, cubes);
          const auto referenceData = sampler.sample(reference.Get(), levels, cubes);
          std::vector<UINT> expected;
          for (UINT relativeCube = 0; relativeCube < cubes; ++relativeCube)
            for (UINT relativeMip = 0; relativeMip < levels; ++relativeMip)
              for (UINT face = 0; face < 6; ++face)
                expected.push_back(texel((first + relativeCube) * 6 + face, mip + relativeMip));
          CHECK(nativeData.size() == expected.size() && referenceData.size() == expected.size());
          for (size_t i = 0; i < expected.size(); ++i) {
            CHECK(nativeData[i] == expected[i]); CHECK(referenceData[i] == expected[i]); ++sampledWords;
          }
          const UINT metadata[] = {mip, requested, first * 6, cubes, levels, static_cast<UINT>(expected.size())};
          save(profile, index, "metadata", metadata, sizeof(metadata));
          save(profile, index, "native.words", nativeData.data(), nativeData.size() * sizeof(UINT));
          save(profile, index, "public.words", referenceData.data(), referenceData.size() * sizeof(UINT));
          ++index; ++views;
        }
}

template<bool Modern> static void failedRanges(Fixture<Modern>& f, Cube<Modern>& cube) {
  View<Modern> retained(f, viewArgs(cube, {1, UINT(-1), 0, 1}));
  auto bound = retained.bind();
  auto args = viewArgs(cube, {0, UINT(-1), 0, 1});
  Storage failed(f.output.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args));
  failed.poison(); const auto original = failed.words;
  const D3D10DDI_HSHADERRESOURCEVIEW handle{failed.data()};
  f.core.pfnSetErrorCb = alternateError;
  const UINT before = alternateCallbacks;
  auto reject = [&](const typename Fixture<Modern>::ViewArgs* input) {
    failedStorage = &failed; failedOriginal = &original;
    failedContext = f.context.Get(); retainedBinding = bound.Get();
    const UINT errors = callbacks;
    f.output.table.pfnCreateShaderResourceView(f.device, input, handle, {});
    CHECK(callbacks == errors + 1 && lastError == dxvk::umd::ddiResult(E_INVALIDARG)); lastError = S_OK;
    failedStorage = nullptr; failedOriginal = nullptr; failedContext = nullptr; retainedBinding = nullptr;
    CHECK(failed.words == original); failed.guards();
    ComPtr<ID3D11ShaderResourceView> current;
    f.context->PSGetShaderResources(0, 1, &current); CHECK(current.Get() == bound.Get());
  };
  const D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW invalid[] = {
    {cube.levels, UINT(-1), 0, 1}, {UINT(-1), UINT(-1), 0, 1}, {1, 0, 0, 1},
    {1, cube.levels, 0, 1}, {1, UINT(-2), 0, 1}, {0, UINT(-1), 1, 1},
    {0, UINT(-1), 5, 1}, {0, UINT(-1), cube.faces, 1},
    {0, UINT(-1), UINT(-1), 1}, {0, UINT(-1), UINT(-1) - 3, 1},
    {0, UINT(-1), 0, 0}, {0, UINT(-1), 0, cube.faces / 6 + 1},
    {0, UINT(-1), 0, UINT(-1)}, {0, UINT(-1), 0, 0x2aaaaaabu}
  };
  for (const auto& range : invalid) { args.TexCube = range; reject(&args); }
  args = viewArgs(cube, {0, UINT(-1), 0, 1}); args.Format = DXGI_FORMAT_R8G8B8A8_UNORM; reject(&args);
  reject(nullptr);
  Cube<Modern> plain(f, cube.levels, 6, false);
  args = viewArgs(plain, {0, UINT(-1), 0, 1}); reject(&args);
  Cube<Modern> noBinding(f, cube.levels, 6, true, D3D10_DDI_BIND_RENDER_TARGET);
  args = viewArgs(noBinding, {0, UINT(-1), 0, 1}); reject(&args);
  CHECK(alternateCallbacks == before + 18);
  // Reuse exactly the failed storage for a later valid creation; no failed
  // object had to be destroyed and no partial view could hide in it.
  args = viewArgs(cube, {cube.levels - 1, UINT(-1), 0, 1});
  f.output.table.pfnCreateShaderResourceView(f.device, &args, handle, {}); ok(); failed.guards();
  f.output.table.pfnDestroyShaderResourceView(f.device, handle); ok(); failed.guards();
}

int main() {
  callerThread = GetCurrentThreadId();
  CHECK(descriptorContract());
  std::puts("BACKEND=WARP\nproduction_DDI=true\nhardware_admission=false\nregistration=false");
  { Fixture<false> f; Cube<false> cube(f, 4, 6); validRanges(f, cube); failedRanges(f, cube); }
  { Fixture<true> f; Cube<true> cube(f, 5, 18); validRanges(f, cube); failedRanges(f, cube); }
  CHECK(backendCalls == 2 && views == 102 && callbacks == 36 && alternateCallbacks == 36);
  std::printf("native D3D10.1/D3D11 cube SRV mip ranges verified checks=%u views=%u words=%u callbacks=%u WARP controls\n",
    checks, views, sampledWords, callbacks);
  return 0;
}
