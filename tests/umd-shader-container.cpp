#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include "umd-probe-shaders.h"
#include <d3d11.h>
#include <d3d11shader.h>
#endif
#ifdef VIOGPU_SHADER_SPIRV_TEST
#include <dxbc/dxbc_api.h>
#include <spirv/spirv_builder.h>
#endif

using namespace dxvk::umd;
static unsigned checks;
static void checkAt(bool value, unsigned line) {
  checks++;
  if (!value) { std::fprintf(stderr,"failed check %u line=%u\n",checks,line); std::abort(); }
}
#define check(...) checkAt((__VA_ARGS__), __LINE__)

#ifdef _WIN32
static void checkReferencePixels(const void* vs, size_t vsBytes, const void* ps, size_t psBytes,
    const char* semantic, const char* label, const void* gs = nullptr, size_t gsBytes = 0) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
  check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&requested,1,
    D3D11_SDK_VERSION,&device,nullptr,&context)));
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11PixelShader> pixel;
  ComPtr<ID3D11GeometryShader> geometry;
  check(SUCCEEDED(device->CreateVertexShader(vs,vsBytes,nullptr,&vertex)));
  check(SUCCEEDED(device->CreatePixelShader(ps,psBytes,nullptr,&pixel)));
  if (gs) check(SUCCEEDED(device->CreateGeometryShader(gs,gsBytes,nullptr,&geometry)));
  D3D11_INPUT_ELEMENT_DESC element = {semantic,0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
  ComPtr<ID3D11InputLayout> layout;
  check(SUCCEEDED(device->CreateInputLayout(&element,1,vs,vsBytes,&layout)));
  FLOAT positions[] = {-1,1,3,1,-1,-3};
  D3D11_BUFFER_DESC bufferDesc = {};
  bufferDesc.ByteWidth = sizeof(positions); bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA initial = {positions,0,0};
  ComPtr<ID3D11Buffer> buffer;
  check(SUCCEEDED(device->CreateBuffer(&bufferDesc,&initial,&buffer)));
  FLOAT constants[] = {0.5f,0,0,1};
  bufferDesc.ByteWidth = sizeof(constants); bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  initial.pSysMem = constants;
  ComPtr<ID3D11Buffer> constant;
  check(SUCCEEDED(device->CreateBuffer(&bufferDesc,&initial,&constant)));
  D3D11_TEXTURE2D_DESC textureDesc = {};
  textureDesc.Width = textureDesc.Height = 64; textureDesc.MipLevels = textureDesc.ArraySize = 1;
  textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; textureDesc.SampleDesc.Count = 1;
  textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> target, staging;
  check(SUCCEEDED(device->CreateTexture2D(&textureDesc,nullptr,&target)));
  textureDesc.BindFlags = 0; textureDesc.Usage = D3D11_USAGE_STAGING; textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  check(SUCCEEDED(device->CreateTexture2D(&textureDesc,nullptr,&staging)));
  ComPtr<ID3D11RenderTargetView> view;
  check(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&view)));
  D3D11_RASTERIZER_DESC rasterDesc = {};
  rasterDesc.FillMode = D3D11_FILL_SOLID; rasterDesc.CullMode = D3D11_CULL_NONE; rasterDesc.DepthClipEnable = TRUE;
  ComPtr<ID3D11RasterizerState> raster;
  check(SUCCEEDED(device->CreateRasterizerState(&rasterDesc,&raster)));
  D3D11_VIEWPORT viewport = {0,0,64,64,0,1};
  ID3D11RenderTargetView* views[] = {view.Get()};
  ID3D11Buffer* buffers[] = {buffer.Get()}; const UINT stride = 8, offset = 0;
  context->OMSetRenderTargets(1,views,nullptr);
  context->RSSetState(raster.Get()); context->RSSetViewports(1,&viewport);
  context->IASetInputLayout(layout.Get()); context->IASetVertexBuffers(0,1,buffers,&stride,&offset);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vertex.Get(),nullptr,0); context->PSSetShader(pixel.Get(),nullptr,0);
  ID3D11Buffer* constantBuffers[] = {constant.Get()};
  context->VSSetConstantBuffers(0,1,constantBuffers);
  if (gs) {
    const UINT masks[] = {0x11111111,0,0,0};
    initial.pSysMem = masks;
    ComPtr<ID3D11Buffer> geometryConstant;
    check(SUCCEEDED(device->CreateBuffer(&bufferDesc,&initial,&geometryConstant)));
    ID3D11Buffer* geometryBuffers[] = {geometryConstant.Get()};
    context->GSSetConstantBuffers(1,1,geometryBuffers);
    D3D11_TEXTURE2D_DESC sampledDesc = {};
    sampledDesc.Width = sampledDesc.Height = sampledDesc.MipLevels = sampledDesc.ArraySize = 1;
    sampledDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sampledDesc.SampleDesc.Count = 1;
    sampledDesc.Usage = D3D11_USAGE_IMMUTABLE; sampledDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const UINT white = 0xffffffff; D3D11_SUBRESOURCE_DATA whiteData = {&white,4,4};
    ComPtr<ID3D11Texture2D> sampledTexture;
    ComPtr<ID3D11ShaderResourceView> sampledView;
    check(SUCCEEDED(device->CreateTexture2D(&sampledDesc,&whiteData,&sampledTexture)));
    check(SUCCEEDED(device->CreateShaderResourceView(sampledTexture.Get(),nullptr,&sampledView)));
    ID3D11ShaderResourceView* sampledViews[] = {sampledView.Get()};
    context->GSSetShaderResources(0,1,sampledViews);
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    check(SUCCEEDED(device->CreateSamplerState(&samplerDesc,&sampler)));
    ID3D11SamplerState* samplers[] = {sampler.Get()};
    context->GSSetSamplers(0,1,samplers);
    context->GSSetShader(geometry.Get(),nullptr,0);
  }
  context->Draw(3,0); context->CopyResource(staging.Get(),target.Get());
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)) && mapped.pData && mapped.RowPitch >= 256);
  uint32_t expected[] = {0x80000000,0xfedcba98,0xffffffff,1,
    0x7fc01234,0x80000000,0x7f800000,0xff800000,0x12345678,0x87654321,0,1,
    0x3e480000,0x3c000000,0x41680000,0x3f000000};
  if (gs) expected[8] ^= 0x11111111;
  unsigned mismatches = 0;
  std::fprintf(stderr,"WARP_LINKAGE_WORDS source=%s",label);
  for (unsigned i = 0; i < 16; ++i) {
    uint32_t word; std::memcpy(&word,static_cast<unsigned char*>(mapped.pData)+i*4,4);
    std::fprintf(stderr," %08x",word); mismatches += word != expected[i];
  }
  std::fprintf(stderr," mismatches=%u\n",mismatches);
  context->Unmap(staging.Get(),0);
  if (mismatches) {
    ComPtr<ID3DBlob> assembly;
    if (SUCCEEDED(D3DDisassemble(vs,vsBytes,0,label,&assembly)))
      std::fwrite(assembly->GetBufferPointer(),1,assembly->GetBufferSize(),stderr);
  }
  check(!mismatches);
}
#endif

#ifdef VIOGPU_SHADER_SPIRV_TEST
static void checkSpirvInterface(const std::vector<unsigned char>& binary, bool vertex,
    const std::vector<ShaderSignatureEntry>& expected, bool geometryInput = false) {
  using namespace dxbc_spv;
  auto ir = dxbc::compileShaderToLegalizedIr(binary.data(),binary.size(),{},{});
  check(bool(ir));
  spirv::BasicResourceMapping mapping;
  spirv::SpirvBuilder::Options options;
  options.supportedRoundModesF32 = ir::RoundMode::eNearestEven | ir::RoundMode::eZero;
  options.supportedDenormModesF32 = ir::DenormMode::eFlush | ir::DenormMode::ePreserve;
  options.supportsZeroInfNanPreserveF32 = true;
  spirv::SpirvBuilder builder(*ir,mapping,options);
  builder.buildSpirvBinary();
  const auto words = builder.getSpirvBinary();
  check(words.size() >= 5 && words[0] == spv::MagicNumber && words[3] < 1024*1024);
  struct Id {
    uint32_t type = 0, base = 0, storage = uint32_t(-1), location = uint32_t(-1);
    uint32_t width = 0, signedness = 0, count = 1;
    spv::Op kind = spv::OpNop;
    bool flat = false;
  };
  std::vector<Id> ids(words[3]);
  for (size_t offset = 5; offset < words.size();) {
    const auto opcode = spv::Op(words[offset] & 0xffff);
    const uint32_t length = words[offset] >> 16;
    check(length && length <= words.size() - offset);
    const auto* op = words.data() + offset;
    if (opcode == spv::OpTypeFloat || opcode == spv::OpTypeInt) {
      check(length == (opcode == spv::OpTypeFloat ? 3u : 4u) && op[1] < ids.size());
      auto& id = ids[op[1]]; id.kind = opcode; id.width = op[2];
      if (opcode == spv::OpTypeInt) id.signedness = op[3];
    } else if (opcode == spv::OpTypeVector || opcode == spv::OpTypePointer || opcode == spv::OpTypeArray) {
      check(length == 4 && op[1] < ids.size());
      auto& id = ids[op[1]]; id.kind = opcode;
      id.base = opcode == spv::OpTypePointer ? op[3] : op[2];
      if (opcode == spv::OpTypeVector) id.count = op[3];
    } else if (opcode == spv::OpVariable) {
      check(length >= 4 && op[2] < ids.size());
      ids[op[2]].type = op[1]; ids[op[2]].storage = op[3];
    } else if (opcode == spv::OpDecorate) {
      check(length >= 3 && op[1] < ids.size());
      auto& id = ids[op[1]];
      if (op[2] == spv::DecorationLocation) { check(length == 4); id.location = op[3]; }
      if (op[2] == spv::DecorationFlat) id.flat = true;
    }
    offset += length;
  }
  for (const auto& entry : expected) if (!entry.systemValue) {
    unsigned matches = 0;
    for (const auto& id : ids) {
      if (id.storage != uint32_t(vertex ? spv::StorageClassOutput : spv::StorageClassInput) ||
          id.location != entry.registerIndex) continue;
      matches++;
      check(id.type < ids.size() && ids[id.type].kind == spv::OpTypePointer);
      uint32_t type = ids[id.type].base;
      check(type < ids.size());
      if (geometryInput) {
        check(ids[type].kind == spv::OpTypeArray);
        type = ids[type].base; check(type < ids.size());
      }
      uint32_t components = 1;
      if (ids[type].kind == spv::OpTypeVector) { components = ids[type].count; type = ids[type].base; }
      std::fprintf(stderr,"SPIRV_INTERFACE stage=%s register=%u components=%u mask=%u kind=%u width=%u flat=%u expected=%u\n",
        vertex ? "VS" : "PS",entry.registerIndex,components,unsigned(entry.mask),unsigned(ids[type].kind),
        ids[type].width,unsigned(id.flat),unsigned(entry.scalar));
      check(type < ids.size() && ids[type].width == 32);
      const bool raw = entry.scalar == ShaderScalar::Uint32;
      check(ids[type].kind == (raw ? spv::OpTypeInt : spv::OpTypeFloat));
      if (raw) check(ids[type].signedness == 0 && (vertex || geometryInput || id.flat));
      check(components == unsigned((entry.mask & 1) + ((entry.mask >> 1) & 1) +
        ((entry.mask >> 2) & 1) + ((entry.mask >> 3) & 1)));
    }
    if (matches != 1) std::fprintf(stderr,"SPIRV_INTERFACE stage=%s register=%u matches=%u\n",
      vertex ? "VS" : "PS",entry.registerIndex,matches);
    check(matches == 1);
  }
}
#endif

int main() {
  // RET is sufficient to test container reconstruction, not executable
  // rendering. The separate DDI probe compiles and draws real HLSL shaders.
  uint32_t code[] = {0x10040, 3, 0x0100003e};
  ShaderSignatureEntry input = {6,0,1}, output = {1,0,15};
  std::vector<unsigned char> binary;
  check(buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  dxbc_spv::dxbc::Container container(binary.data(), binary.size());
  check(bool(container) && container.validateHash());
  auto chunk = container.getCodeChunk();
  check(chunk.getSize() == sizeof(code) + 8);
  check(std::memcmp(chunk.getData(8), code, sizeof(code)) == 0);
  dxbc_spv::dxbc::Signature signature(container.getInputSignatureChunk());
  check(signature.begin() != signature.end());
  check(signature.begin()->getScalarType() == dxbc_spv::ir::ScalarType::eU32);
  check(signature.begin()->getSystemValue() == dxbc_spv::dxbc::SignatureSysval::eVertexId);
  check(uint8_t(signature.begin()->getUsedComponentMask()) == 1);
  for (size_t n = 0; n < 3; n++)
    check(!buildShaderContainer(ShaderStage::Vertex, code, n, &input, 1, &output, 1, binary) && binary.empty());
  check(!buildShaderContainer(ShaderStage::Vertex, nullptr, 3, &input, 1, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 1024*1024+1, &input, 1, &output, 1, binary));
  for (unsigned value : {0u,2u,4u,0xffffffffu}) {
    code[1] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[1] = 3;
  for (unsigned value : {0x40u,0x10050u,0x20040u,0xffffffffu}) {
    code[0] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[0] = 0x10040;
  // Illegal opcode and truncated extended/multiword opcodes must fail.
  for (unsigned value : {0x010007ffu,0x8100003eu,0x0200003eu,0u}) {
    code[2] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[2] = 0x0100003e;
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, nullptr, 1, &output, 1, binary));
  ShaderSignatureEntry duplicateInputs[] = {input,input};
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, duplicateInputs, 2, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 33, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, nullptr, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 0, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,1}, {6,32,1}, {6,0,2}, {6,0,0}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &wrong, 1, &output, 1, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,15}, {1,32,15}, {1,0,1}, {1,0,31}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &wrong, 1, binary));
  ShaderSignatureEntry typedInputs[] = {{0,0,3,ShaderScalar::Float32},
    {0,3,1,ShaderScalar::Uint32}, {0,7,1,ShaderScalar::Sint32}};
  check(buildShaderContainer(ShaderStage::Vertex, code, 3, typedInputs, 3, &output, 1, binary));
  dxbc_spv::dxbc::Container typedContainer(binary.data(), binary.size());
  dxbc_spv::dxbc::Signature typedSignature(typedContainer.getInputSignatureChunk());
  const dxbc_spv::ir::ScalarType expectedTypes[] = {dxbc_spv::ir::ScalarType::eF32,
    dxbc_spv::ir::ScalarType::eU32, dxbc_spv::ir::ScalarType::eI32};
  for (unsigned i = 0; i < 3; i++) {
    auto entry = typedSignature.findSemantic(0, inputRegisterSemantic, typedInputs[i].registerIndex);
    check(entry != typedSignature.end());
    check(entry->getRegisterIndex() == int32_t(typedInputs[i].registerIndex));
    check(entry->getScalarType() == expectedTypes[i]);
    check(uint8_t(entry->getUsedComponentMask()) == typedInputs[i].mask);
  }
  typedInputs[0].scalar = ShaderScalar::Unknown;
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, typedInputs, 3, &output, 1, binary));
  code[0] = 0x40; output = {0,0,15};
  check(buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));
  dxbc_spv::dxbc::Container pixel(binary.data(), binary.size());
  check(bool(pixel) && pixel.validateHash());
  dxbc_spv::dxbc::Signature color(pixel.getOutputSignatureChunk());
  check(color.begin()->getSystemValue() == dxbc_spv::dxbc::SignatureSysval::eTarget);
  ShaderSignatureEntry unusedPixelInput = {0,7,15};
  check(buildShaderContainer(ShaderStage::Pixel, code, 3, &unusedPixelInput, 1, &output, 1, binary));
  output.systemValue = 1;
  check(!buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));

  // Register-based linkage works without application semantic names or
  // assuming that SV_Position is stored in output register zero.
  ShaderSignatureEntry vsOutputs[] = {{1,1,15}, {0,3,7}, {0,7,15}};
  ShaderSignatureEntry psInputs[] = {{1,1,3,ShaderScalar::Float32},
    {0,3,3,ShaderScalar::Float32}, {0,7,15,ShaderScalar::Uint32}};
  std::vector<ShaderSignatureEntry> linked, resolved;
  check(linkVertexOutputs(vsOutputs,3,psInputs,3,linked) && linked.size() == 3);
  check(linked[0].scalar == ShaderScalar::Float32 && linked[1].scalar == ShaderScalar::Float32
    && linked[2].scalar == ShaderScalar::Uint32);
  code[0] = 0x10040;
  check(buildShaderContainer(ShaderStage::Vertex,code,3,nullptr,0,linked.data(),linked.size(),binary));
  dxbc_spv::dxbc::Container linkedContainer(binary.data(),binary.size());
  dxbc_spv::dxbc::Signature linkedSignature(linkedContainer.getOutputSignatureChunk());
  auto flatOutput = linkedSignature.findSemantic(0,varyingRegisterSemantic,7);
  check(flatOutput != linkedSignature.end() && flatOutput->getRegisterIndex() == 7
    && flatOutput->getScalarType() == dxbc_spv::ir::ScalarType::eU32);
  psInputs[1].mask = 15;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked) && linked.empty());
  psInputs[1].mask = 3; psInputs[1].registerIndex = 2;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[1].registerIndex = 3; psInputs[2].scalar = ShaderScalar::Unknown;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[2].scalar = ShaderScalar::Sint32;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[2].scalar = ShaderScalar::Uint32; psInputs[0].systemValue = 0;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[0].systemValue = 1;
  check(!linkVertexOutputs(vsOutputs+1,2,psInputs,3,linked));
  check(linkVertexOutputs(vsOutputs,3,nullptr,0,linked));
  check(linked[1].scalar == ShaderScalar::Uint32 && linked[2].scalar == ShaderScalar::Uint32);

  // dcl_input_ps v3.xy followed by ret. These are parsing/signature
  // fixtures only. The device probe below uses executable HLSL.
  uint32_t pixelCode[] = {0x40,6,0x03000062,0x00101032,3,0x0100003e};
  ShaderSignatureEntry varying = {0,3,15}, target = {0,0,15};
  for (uint32_t mode = 1; mode <= 7; mode++) {
    pixelCode[2] = 0x03000062 | (mode << 11);
    check(resolvePixelInputs(pixelCode,6,&varying,1,resolved) && resolved.size() == 1);
    check(resolved[0].mask == 3 && resolved[0].scalar ==
      (mode == 1 ? ShaderScalar::Uint32 : ShaderScalar::Float32));
    check(buildShaderContainer(ShaderStage::Pixel,pixelCode,6,&varying,1,&target,1,binary));
    dxbc_spv::dxbc::Container varyingContainer(binary.data(),binary.size());
    dxbc_spv::dxbc::Signature varyingSignature(varyingContainer.getInputSignatureChunk());
    auto entry = varyingSignature.findSemantic(0,varyingRegisterSemantic,3);
    check(entry != varyingSignature.end() && entry->getRegisterIndex() == 3 &&
      entry->getScalarType() == (mode == 1 ? dxbc_spv::ir::ScalarType::eU32 : dxbc_spv::ir::ScalarType::eF32));
    check(uint8_t(entry->getUsedComponentMask()) == 3);
  }
  for (uint32_t mode : {0u,8u,15u}) {
    pixelCode[2] = 0x03000062 | (mode << 11);
    check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved) && resolved.empty());
  }
  pixelCode[2] = 0x03001062;
  ShaderSignatureEntry pixelUnion[] = {{0,3,15},{0,31,15}};
  check(resolvePixelInputs(pixelCode,6,pixelUnion,2,resolved) && resolved.size() == 1);
  check(resolved[0].registerIndex == 3 && resolved[0].mask == 3);
  check(!resolvePixelInputs(pixelCode,6,nullptr,0,resolved));
  varying.mask = 1;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  varying.mask = 3; varying.registerIndex = 2;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  varying.registerIndex = 3; pixelCode[4] = 32;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  pixelCode[4] = 3;
  check(!resolvePixelInputs(pixelCode,5,&varying,1,resolved));
  uint32_t conflicting[] = {0x40,9,0x03001062,0x00101012,3,0x03000862,0x00101022,3,0x0100003e};
  check(!resolvePixelInputs(conflicting,9,&varying,1,resolved));
  uint32_t immediate[] = {0x10040,9,0x1835,6,0x7fc01234,0x80000000,0x7f800000,0xff800000,0x0100003e};
  ShaderSignatureEntry immediateOutput = {1,0,15};
  check(buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  dxbc_spv::dxbc::Container immediateContainer(binary.data(),binary.size());
  check(immediateContainer.validateHash() && !std::memcmp(immediateContainer.getCodeChunk().getData(8),
    immediate,sizeof(immediate)));
  for (uint32_t length : {0u,1u,2u,3u,5u,7u,8u,0xffffffffu}) {
    immediate[3] = length;
    check(!buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  immediate[3] = 6;
  for (uint32_t type : {2u,4u,5u,0x1fffffu}) {
    immediate[2] = (type << 11) | 0x35;
    check(!buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  for (uint32_t type : {0u,1u}) {
    immediate[2] = (type << 11) | 0x35;
    check(buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  uint32_t duplicateIcb[] = {0x10040,15,0x1835,6,1,2,3,4,0x1835,6,5,6,7,8,0x0100003e};
  check(!buildShaderContainer(ShaderStage::Vertex,duplicateIcb,15,nullptr,0,&immediateOutput,1,binary));
  // dcl_input_siv v[3][0].xyzw, position; dcl_input v[3][3].xy.
  uint32_t geometryCode[] = {0x20040,12,0x05000061,0x002010f2,3,0,1,
    0x0400005f,0x00201032,3,3,0x0100003e};
  ShaderSignatureEntry geometryInputs[] = {{1,0,15},{0,3,15}};
  check(resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved));
  check(resolved.size() == 2 && resolved[0].scalar == ShaderScalar::Float32 &&
    resolved[1].scalar == ShaderScalar::Uint32 && resolved[1].mask == 3);
  ShaderSignatureEntry geometryUnion[] = {{1,0,15},{0,3,15},{0,31,15}};
  check(resolveGeometryInputs(geometryCode,12,geometryUnion,3,resolved) && resolved.size() == 2);
  check(linkVertexOutputs(&immediateOutput,1,resolved.data(),1,linked));
  check(buildShaderContainer(ShaderStage::Geometry,geometryCode,12,geometryInputs,2,
    &immediateOutput,1,binary));
  dxbc_spv::dxbc::Container geometryContainer(binary.data(),binary.size());
  dxbc_spv::dxbc::Signature geometrySignature(geometryContainer.getInputSignatureChunk());
  auto rawGeometryInput = geometrySignature.findSemantic(0,varyingRegisterSemantic,3);
  check(rawGeometryInput != geometrySignature.end() &&
    rawGeometryInput->getScalarType() == dxbc_spv::ir::ScalarType::eU32 &&
    uint8_t(rawGeometryInput->getUsedComponentMask()) == 3);
  for (uint32_t vertexCount : {0u,7u,0xffffffffu}) {
    geometryCode[4] = vertexCount;
    check(!resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved) && resolved.empty());
  }
  geometryCode[4] = 3; geometryCode[10] = 32;
  check(!resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved));
  geometryCode[10] = 3; geometryCode[6] = 6;
  check(!resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved));
  geometryCode[6] = 1;
  check(!resolveGeometryInputs(geometryCode,12,geometryInputs,1,resolved));
  geometryInputs[1].mask = 1;
  check(!resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved));
  geometryInputs[1].mask = 15; geometryInputs[1].systemValue = 1;
  check(!resolveGeometryInputs(geometryCode,12,geometryInputs,2,resolved));
  geometryInputs[1].systemValue = 0;
  check(!resolveGeometryInputs(geometryCode,11,geometryInputs,2,resolved));
#ifdef _WIN32
  for (bool vertex : {true, false}) {
    std::vector<uint32_t> compiled;
    check(compileProbeShader(vertex, compiled));
    output.systemValue = vertex ? 1 : 0;
    const bool rebuilt = buildShaderContainer(vertex ? ShaderStage::Vertex : ShaderStage::Pixel,
      compiled.data(), compiled.size(), vertex ? &input : nullptr, vertex ? 1 : 0,
      &output, 1, binary);
    if (!rebuilt) {
      std::fprintf(stderr, "reconstruction failed stage=%s words=%zu\n", vertex ? "VS" : "PS", compiled.size());
      for (uint32_t word : compiled) std::fprintf(stderr, "%08x ", word);
      std::fputc('\n', stderr);
    }
    check(rebuilt);
    dxbc_spv::dxbc::Container real(binary.data(), binary.size());
    check(bool(real) && real.validateHash());
    auto realCode = real.getCodeChunk();
    check(realCode.getSize() == compiled.size() * 4 + 8);
    check(std::memcmp(realCode.getData(8), compiled.data(), compiled.size() * 4) == 0);
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflected;
    check(SUCCEEDED(D3DReflect(binary.data(), binary.size(), __uuidof(ID3D11ShaderReflection), &reflected)));
    D3D11_SHADER_DESC desc = {};
    check(SUCCEEDED(reflected->GetDesc(&desc)));
    check(desc.InputParameters == (vertex ? 1u : 0u) && desc.OutputParameters == 1);
  }
  std::vector<uint32_t> buffered;
  check(compileProbeShader(true, buffered, true));
  ShaderSignatureEntry bufferedInput = {0,0,3,ShaderScalar::Float32}, positionOutput = {1,0,15};
  check(buildShaderContainer(ShaderStage::Vertex, buffered.data(), buffered.size(),
    &bufferedInput, 1, &positionOutput, 1, binary));
  Microsoft::WRL::ComPtr<ID3D11ShaderReflection> bufferedReflection;
  check(SUCCEEDED(D3DReflect(binary.data(), binary.size(), __uuidof(ID3D11ShaderReflection), &bufferedReflection)));
  D3D11_SIGNATURE_PARAMETER_DESC reflectedInput = {};
  check(SUCCEEDED(bufferedReflection->GetInputParameterDesc(0, &reflectedInput)));
  check(reflectedInput.Register == 0 && reflectedInput.Mask == 3
    && reflectedInput.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32
    && !std::strcmp(reflectedInput.SemanticName, inputRegisterSemantic));

  std::vector<uint32_t> linkedVs, linkedPs;
  std::vector<ShaderSignatureEntry> vsi, vso, psi, pso;
  check(compileLinkageProbeShader(true,linkedVs,vsi,vso));
  check(compileLinkageProbeShader(false,linkedPs,psi,pso));
  check(resolvePixelInputs(linkedPs.data(),linkedPs.size(),psi.data(),psi.size(),resolved));
  check(linkVertexOutputs(vso.data(),vso.size(),resolved.data(),resolved.size(),linked));
  unsigned flatCount = 0, interpolatedCount = 0;
  for (const auto& entry : resolved) if (!entry.systemValue) {
    flatCount += entry.scalar == ShaderScalar::Uint32;
    interpolatedCount += entry.scalar == ShaderScalar::Float32;
  }
  check(flatCount == 3 && interpolatedCount == 1);
  for (auto& entry : vsi) if (!entry.systemValue) entry.scalar = ShaderScalar::Float32;
  for (bool vertex : {true,false}) {
    const auto& tokens = vertex ? linkedVs : linkedPs;
    const auto& ins = vertex ? vsi : resolved;
    const auto& outs = vertex ? linked : pso;
    check(buildShaderContainer(vertex ? ShaderStage::Vertex : ShaderStage::Pixel,tokens.data(),tokens.size(),
      ins.data(),ins.size(),outs.data(),outs.size(),binary));
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflected;
    check(SUCCEEDED(D3DReflect(binary.data(),binary.size(),__uuidof(ID3D11ShaderReflection),&reflected)));
    D3D11_SHADER_DESC desc = {};
    check(SUCCEEDED(reflected->GetDesc(&desc)));
    unsigned reflectedFlat = 0;
    for (UINT i = 0; i < (vertex ? desc.OutputParameters : desc.InputParameters); i++) {
      D3D11_SIGNATURE_PARAMETER_DESC entry = {};
      check(SUCCEEDED(vertex ? reflected->GetOutputParameterDesc(i,&entry) : reflected->GetInputParameterDesc(i,&entry)));
      if (entry.ComponentType == D3D_REGISTER_COMPONENT_UINT32) {
        check(!std::strcmp(entry.SemanticName,varyingRegisterSemantic) && entry.SemanticIndex == entry.Register);
        reflectedFlat++;
      }
    }
    check(reflectedFlat == 3);
#ifdef VIOGPU_SHADER_SPIRV_TEST
    checkSpirvInterface(binary,vertex,vertex ? linked : resolved);
#endif
  }
  std::vector<uint32_t> immediateTokens;
  check(compileImmediateProbeShader(immediateTokens));
  bool hasIcb = false;
  for (size_t i = 2; i < immediateTokens.size();) {
    if ((immediateTokens[i] & 0x7ff) == 0x35) {
      hasIcb |= (immediateTokens[i] >> 11) == 3;
      i += immediateTokens[i+1];
    } else i += (immediateTokens[i] >> 24) & 0x7f;
  }
  check(hasIcb);
  check(buildShaderContainer(ShaderStage::Vertex,immediateTokens.data(),immediateTokens.size(),
    &input,1,&immediateOutput,1,binary));
#ifdef VIOGPU_SHADER_SPIRV_TEST
  checkSpirvInterface(binary,true,{});
#endif
  // Execute both the original Microsoft bytecode and the rebuilt DDI
  // containers on WARP. This distinguishes an invalid test expectation or
  // container reconstruction from the embedded Vulkan compiler/driver path.
  Microsoft::WRL::ComPtr<ID3DBlob> originalVs, originalPs;
  check(compileLinkageProbeShader(true,linkedVs,vsi,vso,true,&originalVs));
  check(compileLinkageProbeShader(false,linkedPs,psi,pso,true,&originalPs));
  check(resolvePixelInputs(linkedPs.data(),linkedPs.size(),psi.data(),psi.size(),resolved));
  check(linkVertexOutputs(vso.data(),vso.size(),resolved.data(),resolved.size(),linked));
  for (auto& entry : vsi) if (!entry.systemValue) entry.scalar = ShaderScalar::Float32;
  std::vector<unsigned char> rebuiltVs, rebuiltPs;
  check(buildShaderContainer(ShaderStage::Vertex,linkedVs.data(),linkedVs.size(),vsi.data(),vsi.size(),linked.data(),linked.size(),rebuiltVs));
  check(buildShaderContainer(ShaderStage::Pixel,linkedPs.data(),linkedPs.size(),resolved.data(),resolved.size(),pso.data(),pso.size(),rebuiltPs));
  checkReferencePixels(originalVs->GetBufferPointer(),originalVs->GetBufferSize(),
    originalPs->GetBufferPointer(),originalPs->GetBufferSize(),"POSITION","original");
  checkReferencePixels(rebuiltVs.data(),rebuiltVs.size(),rebuiltPs.data(),rebuiltPs.size(),inputRegisterSemantic,"rebuilt");
  std::vector<uint32_t> geometryTokens;
  std::vector<ShaderSignatureEntry> gsi, gso, geometryResolved, geometryLinked;
  Microsoft::WRL::ComPtr<ID3DBlob> originalGs;
  check(compileLinkageProbeShader(false,geometryTokens,gsi,gso,true,&originalGs,true));
  check(resolveGeometryInputs(geometryTokens.data(),geometryTokens.size(),gsi.data(),gsi.size(),geometryResolved));
  check(linkVertexOutputs(vso.data(),vso.size(),geometryResolved.data(),geometryResolved.size(),linked));
  check(linkVertexOutputs(gso.data(),gso.size(),resolved.data(),resolved.size(),geometryLinked));
  std::vector<unsigned char> rebuiltGs;
  check(buildShaderContainer(ShaderStage::Vertex,linkedVs.data(),linkedVs.size(),vsi.data(),vsi.size(),
    linked.data(),linked.size(),rebuiltVs));
  check(buildShaderContainer(ShaderStage::Geometry,geometryTokens.data(),geometryTokens.size(),
    geometryResolved.data(),geometryResolved.size(),geometryLinked.data(),geometryLinked.size(),rebuiltGs));
#ifdef VIOGPU_SHADER_SPIRV_TEST
  checkSpirvInterface(rebuiltVs,true,linked);
  checkSpirvInterface(rebuiltGs,false,geometryResolved,true);
  checkSpirvInterface(rebuiltGs,true,geometryLinked);
#endif
  checkReferencePixels(originalVs->GetBufferPointer(),originalVs->GetBufferSize(),
    originalPs->GetBufferPointer(),originalPs->GetBufferSize(),"POSITION","original-gs",
    originalGs->GetBufferPointer(),originalGs->GetBufferSize());
  checkReferencePixels(rebuiltVs.data(),rebuiltVs.size(),rebuiltPs.data(),rebuiltPs.size(),inputRegisterSemantic,
    "rebuilt-gs",rebuiltGs.data(),rebuiltGs.size());
#endif
  std::printf("shader container validation PASS checks=%u\n", checks);
}
