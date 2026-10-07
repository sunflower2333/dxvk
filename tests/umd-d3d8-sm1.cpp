#include "../src/umd/umd_legacy_api.h"
#ifdef _WIN32
#include "../src/d3d9/d3d9_shader_code.h"
#endif
#include <sm3/sm3_converter.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
using ShaderWord = DWORD;
#else
using ShaderWord = uint32_t;
#endif
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, \
  "SM1 bridge CHECK line=%d: %s\n", __LINE__, #value); std::_Exit(1); } } while (0)

class BridgeLogger final : public dxbc_spv::util::Logger {
public:
  unsigned errors = 0;
  void message(dxbc_spv::util::LogLevel severity, const char*) override {
    if (severity == dxbc_spv::util::LogLevel::eError) ++errors;
  }
  dxbc_spv::util::LogLevel getMinimumSeverity() override {
    return dxbc_spv::util::LogLevel::eWarn;
  }
};

template<size_t N> static void convert(const char* label, const std::array<ShaderWord,N>& code,
                                       bool vertex, bool sampled) {
  using namespace dxbc_spv;
  CHECK(dxvk::umd::legacyShaderModelAllowed(dxvk::umd::LegacyD3DApi::D3D8, code[0], vertex));
#ifdef _WIN32
  CHECK(dxvk::validateD3D9ShaderCode(code.data(), sizeof(code), vertex));
#endif
  BridgeLogger logger;
  sm3::Converter::Options options{};
  options.name = label;
  options.forceDynamicTextureType = !vertex;
  sm3::Converter converter(util::ByteReader(code.data(), sizeof(code)), options);
  ir::Builder builder;
  CHECK(converter.convertShader(builder));
  CHECK(logger.errors == 0);
  unsigned entries = 0, outputs = 0, samples = 0;
  for (const auto& op : builder) {
    if (op.getOpCode() == ir::OpCode::eEntryPoint) {
      ++entries;
      CHECK(op.getOperandCount() == 2);
      CHECK(uint32_t(op.getOperand(1)) == uint32_t(vertex ? ir::ShaderStage::eVertex : ir::ShaderStage::ePixel));
    }
    outputs += op.getOpCode() == ir::OpCode::eOutputStore;
    samples += op.getOpCode() == ir::OpCode::eImageSample;
  }
  CHECK(entries == 1 && outputs > 0);
  CHECK(sampled ? samples > 0 : samples == 0);
  std::printf("actual DXVK SM converter %s entry=1 output=%u samples=%u\n", label, outputs, samples);
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  // Identical legacy instruction streams travel through typed DDI into the
  // private D3D9 compiler. Shader1.x instruction lengths are implicit.
  convert("vs_1_1_position_color", std::array<ShaderWord,8>{
    0xfffe0101,1,0xc00f0000,0x90e40000,1,0xd00f0000,0x90e40001,0x0000ffff}, true, false);
  convert("ps_1_1_color", std::array<ShaderWord,5>{
    0xffff0101,1,0x800f0000,0x90e40000,0x0000ffff}, false, false);
  convert("ps_1_4_color", std::array<ShaderWord,5>{
    0xffff0104,1,0x800f0000,0x90e40000,0x0000ffff}, false, false);
  // TEX t0 is a one-register SM1.1 opcode; TEXLD r0,t0 is two-register
  // SM1.4. Both must create actual sample IR rather than just pass framing.
  convert("ps_1_1_tex", std::array<ShaderWord,7>{
    0xffff0101,0x42,0xb00f0000,1,0x800f0000,0xb0e40000,0x0000ffff}, false, true);
  convert("ps_1_4_texld", std::array<ShaderWord,5>{
    0xffff0104,0x42,0x800f0000,0xb0e40000,0x0000ffff}, false, true);
  for (const uint32_t invalid : {0xfffe0102u,0xffff0105u,0xfffd0101u,0xffffffffu}) {
    const std::array<ShaderWord,2> code{invalid,0x0000ffff};
    // Converter assumes its caller has validated header/bytecode. The typed
    // DDI does that before entry; feeding an invalid header directly into the
    // converter bypasses that contract and may assert during finalization.
    dxbc_spv::util::ByteReader reader(code.data(),sizeof(code));
    const dxbc_spv::sm3::ShaderInfo info(reader);
    CHECK(!bool(info));
    CHECK(!dxvk::umd::legacyShaderModelAllowed(dxvk::umd::LegacyD3DApi::D3D8,invalid,true));
    CHECK(!dxvk::umd::legacyShaderModelAllowed(dxvk::umd::LegacyD3DApi::D3D8,invalid,false));
  }
  std::printf("actual DXVK SM1.1/1.4 compiler bridge PASS checks=%u; no GPU execution\n", checks);
}
