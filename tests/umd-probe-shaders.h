#pragma once
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include "../src/umd/umd_shader.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

inline bool compileHlslTokens(const char* source, const char* entry, const char* profile,
    std::vector<uint32_t>& tokens, ID3DBlob** original = nullptr) {
  Microsoft::WRL::ComPtr<ID3DBlob> shader, errors;
  HRESULT hr = D3DCompile(source, std::strlen(source), "umd-probe", nullptr, nullptr,
    entry, profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
    0, &shader, &errors);
  if (errors) std::fwrite(errors->GetBufferPointer(), 1, errors->GetBufferSize(), stderr);
  if (FAILED(hr) || !shader || shader->GetBufferSize() < 32) return false;
  const auto data = static_cast<const unsigned char*>(shader->GetBufferPointer());
  const size_t size = shader->GetBufferSize();
  auto word = [&](size_t offset) { uint32_t v; std::memcpy(&v, data + offset, 4); return v; };
  if (word(0) != 0x43425844 || word(24) != size || word(28) > (size - 32) / 4) return false;
  for (uint32_t i = 0; i < word(28); i++) {
    const size_t offset = word(32 + size_t(i) * 4);
    if (offset > size - 8) return false;
    const uint32_t tag = word(offset), bytes = word(offset + 4);
    if (bytes > size - offset - 8) return false;
    if (tag != 0x52444853 && tag != 0x58454853) continue;
    if (bytes < 8 || bytes % 4) return false;
    tokens.resize(bytes / 4);
    std::memcpy(tokens.data(), data + offset + 8, bytes);
    if (tokens[1] != tokens.size()) return false;
    if (original) *original = shader.Detach();
    return true;
  }
  return false;
}

inline bool compileProbeShader(bool vertex, std::vector<uint32_t>& tokens, bool vertexBuffer = false) {
  constexpr char source[] = R"(
float4 vs_main(uint id : SV_VertexID) : SV_Position {
  float2 xy = float2((id << 1) & 2, id & 2);
  return float4(xy * float2(2,-2) + float2(-1,1), 0, 1);
}
float4 vs_buffer(float2 position : POSITION) : SV_Position {
  return float4(position, 0, 1);
}
cbuffer PixelConstants : register(b0) { float4 pixelColor; };
Texture2D<float4> sourceColor : register(t0);
SamplerState sourceSampler : register(s0);
float4 ps_main() : SV_Target {
  return pixelColor * sourceColor.SampleLevel(sourceSampler, float2(0.5,0.5), 0);
}
)";
  return compileHlslTokens(source, vertex ? (vertexBuffer ? "vs_buffer" : "vs_main") : "ps_main",
    vertex ? "vs_4_0" : "ps_4_0", tokens);
}

inline bool compileLinkageProbeShader(bool vertex, std::vector<uint32_t>& tokens,
    std::vector<dxvk::umd::ShaderSignatureEntry>& inputs,
    std::vector<dxvk::umd::ShaderSignatureEntry>& outputs, bool inspect = false,
    ID3DBlob** originalContainer = nullptr) {
  constexpr char source[] = R"(
struct Varyings {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint4 bits : TEXCOORD1;
  nointerpolation float4 rawFloats : TEXCOORD2;
  nointerpolation uint4 fixedBits : TEXCOORD3;
};
Varyings vs_main(float2 position : POSITION) {
  static const uint4 patterns[3] = {
    uint4(0x80000000,0xfedcba98,0xffffffff,1),
    uint4(4,3,2,1), uint4(8,7,6,5)
  };
  Varyings value;
  value.position = float4(position, 0, 1);
  value.uv = position * float2(0.5,-0.5) + 0.5;
  value.bits = patterns[min((uint)max(position.x,0),2)];
  value.rawFloats = asfloat(uint4(0x7fc01234,0x80000000,0x7f800000,0xff800000));
  value.fixedBits = uint4(0x12345678,0x87654321,0,1);
  return value;
}
cbuffer PixelConstants : register(b0) { float4 pixelColor; };
Texture2D<float4> sourceColor : register(t0);
SamplerState sourceSampler : register(s0);
float4 ps_main(Varyings value) : SV_Target {
  uint failed = 0;
  if (!all(abs(value.uv - value.position.xy / 64.0) < 0.001)) failed |= 1;
  if (!all(asuint(value.rawFloats) == uint4(0x7fc01234,0x80000000,0x7f800000,0xff800000))) failed |= 2;
  if (!all(value.bits == uint4(0x80000000,0xfedcba98,0xffffffff,1))) failed |= 4;
  if (!all(value.fixedBits == uint4(0x12345678,0x87654321,0,1))) failed |= 8;
  return failed ? float4(0,float(failed)/255.0,0,1) :
    pixelColor * sourceColor.SampleLevel(sourceSampler, value.uv, 0);
}
float4 ps_inspect(Varyings value) : SV_Target {
  uint selector = (uint)value.position.x & 15;
  uint word;
  if (selector < 4) word = value.bits[selector];
  else if (selector < 8) word = asuint(value.rawFloats)[selector-4];
  else if (selector < 12) word = value.fixedBits[selector-8];
  else if (selector < 14) word = asuint(value.uv)[selector-12];
  else word = asuint(value.position.xy)[selector-14];
  return float4(word & 255,(word >> 8) & 255,(word >> 16) & 255,word >> 24)/255.0;
}
)";
  Microsoft::WRL::ComPtr<ID3DBlob> original;
  if (!compileHlslTokens(source, vertex ? "vs_main" : inspect ? "ps_inspect" : "ps_main", vertex ? "vs_4_0" : "ps_4_0",
      tokens, &original)) return false;
  Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflected;
  if (FAILED(D3DReflect(original->GetBufferPointer(), original->GetBufferSize(),
      __uuidof(ID3D11ShaderReflection), &reflected))) return false;
  D3D11_SHADER_DESC desc = {};
  if (FAILED(reflected->GetDesc(&desc))) return false;
  inputs.clear(); outputs.clear();
  for (bool input : {true, false}) {
    const UINT count = input ? desc.InputParameters : desc.OutputParameters;
    auto& destination = input ? inputs : outputs;
    for (UINT i = 0; i < count; i++) {
      D3D11_SIGNATURE_PARAMETER_DESC entry = {};
      const HRESULT hr = input ? reflected->GetInputParameterDesc(i, &entry) : reflected->GetOutputParameterDesc(i, &entry);
      if (FAILED(hr)) return false;
      // DDI encodes pixel target registers as UNDEFINED. Deliberately omit
      // semantic strings and types, as the Microsoft runtime does.
      const uint32_t sysval = !vertex && !input ? 0 : uint32_t(entry.SystemValueType);
      destination.push_back({sysval,entry.Register,entry.Mask});
    }
  }
  if (originalContainer) *originalContainer = original.Detach();
  return true;
}

inline bool compileImmediateProbeShader(std::vector<uint32_t>& tokens) {
  constexpr char source[] = R"(
float4 vs_main(uint id : SV_VertexID) : SV_Position {
  static const float4 positions[3] = {
    float4(-1,1,0,1), float4(3,1,0,1), float4(-1,-3,0,1)
  };
  return positions[id % 3];
}
)";
  return compileHlslTokens(source,"vs_main","vs_4_0",tokens);
}
