#pragma once
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

inline bool compileProbeShader(bool vertex, std::vector<uint32_t>& tokens) {
  constexpr char source[] = R"(
float4 vs_main(uint id : SV_VertexID) : SV_Position {
  float2 xy = float2((id << 1) & 2, id & 2);
  return float4(xy * float2(2,-2) + float2(-1,1), 0, 1);
}
float4 ps_main() : SV_Target { return float4(1,0,0,1); }
)";
  Microsoft::WRL::ComPtr<ID3DBlob> shader, errors;
  HRESULT hr = D3DCompile(source, sizeof(source)-1, "umd-probe", nullptr, nullptr,
    vertex ? "vs_main" : "ps_main", vertex ? "vs_4_0" : "ps_4_0",
    D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shader, &errors);
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
    return tokens[1] == tokens.size();
  }
  return false;
}
