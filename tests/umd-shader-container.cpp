#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include "umd-probe-shaders.h"
#include <d3d11shader.h>
#endif

using namespace dxvk::umd;
static unsigned checks;
static void check(bool value) { checks++; if (!value) { std::fprintf(stderr, "failed check %u\n", checks); std::abort(); } }

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
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 2, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, nullptr, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 0, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,1}, {6,1,1}, {6,0,2}, {6,0,0}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &wrong, 1, &output, 1, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,15}, {1,1,15}, {1,0,1}, {1,0,31}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &wrong, 1, binary));
  code[0] = 0x40; output = {0,0,15};
  check(buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));
  dxbc_spv::dxbc::Container pixel(binary.data(), binary.size());
  check(bool(pixel) && pixel.validateHash());
  dxbc_spv::dxbc::Signature color(pixel.getOutputSignatureChunk());
  check(color.begin()->getSystemValue() == dxbc_spv::dxbc::SignatureSysval::eTarget);
  check(!buildShaderContainer(ShaderStage::Pixel, code, 3, &input, 1, &output, 1, binary));
  output.systemValue = 1;
  check(!buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));
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
#endif
  std::printf("shader container validation PASS checks=%u\n", checks);
}
