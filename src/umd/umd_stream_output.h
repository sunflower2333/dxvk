#pragma once
#include "umd_ddi.h"
#include "umd_shader.h"
#include <d3d11.h>
#include <algorithm>
#include <array>
#include <vector>

namespace dxvk::umd {

struct StreamOutput {
  std::vector<D3D11_SO_DECLARATION_ENTRY> entries;
  std::array<UINT, D3D11_SO_BUFFER_SLOT_COUNT> strides{};
  UINT strideCount = 0;
};

inline bool streamOutputPassthroughSignature(const D3D10DDIARG_STAGE_IO_SIGNATURES& native,
    std::vector<ShaderSignatureEntry>& output) {
  output.clear();
  if (!native.NumOutputSignatureEntries || native.NumOutputSignatureEntries > 32
      || !native.pOutputSignature || native.NumInputSignatureEntries > 32
      || (native.NumInputSignatureEntries && !native.pInputSignature)) return false;
  std::array<UINT,32> used{};
  UINT distanceComponents = 0, distanceRegisters = 0;
  bool position = false;
  std::vector<ShaderSignatureEntry> candidate;
  for (UINT i = 0; i < native.NumOutputSignatureEntries; ++i) {
    const auto& entry = native.pOutputSignature[i];
    if (entry.Register >= used.size() || (used[entry.Register] & entry.Mask) || !entry.Mask || (entry.Mask & ~15u)
        || UINT(entry.SystemValue) > UINT(D3D10_SB_NAME_CULL_DISTANCE)) return false;
    if (entry.SystemValue == D3D10_SB_NAME_POSITION) {
      if (position || entry.Mask != 15) return false;
      position = true;
    }
    if (entry.SystemValue == D3D10_SB_NAME_CLIP_DISTANCE || entry.SystemValue == D3D10_SB_NAME_CULL_DISTANCE) {
      for (UINT component = 0; component < 4; ++component)
        distanceComponents += !!(entry.Mask & (1u << component));
      distanceRegisters |= 1u << entry.Register;
    }
    used[entry.Register] |= entry.Mask;
    candidate.push_back({UINT(entry.SystemValue),entry.Register,entry.Mask,
      entry.SystemValue != D3D10_SB_NAME_UNDEFINED ? ShaderScalar::Float32 : ShaderScalar::Uint32});
  }
  UINT registers = 0;
  for (; distanceRegisters; distanceRegisters &= distanceRegisters - 1) ++registers;
  if (distanceComponents > 8 || registers > 2) return false;
  output = std::move(candidate);
  return true;
}

inline bool streamOutputDeclaration(const D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT& native,
    const D3D10DDIARG_STAGE_IO_SIGNATURES& signature, StreamOutput& output) {
  output = {};
  if (native.NumEntries > 64 || (native.NumEntries && !native.pOutputStreamDecl)
      || signature.NumOutputSignatureEntries > 32
      || (signature.NumOutputSignatureEntries && !signature.pOutputSignature)) return false;
  std::array<UINT,32> signatureMasks{};
  for (UINT i = 0; i < signature.NumOutputSignatureEntries; ++i) {
    const auto& entry = signature.pOutputSignature[i];
    if (entry.Register >= signatureMasks.size() || !entry.Mask || (entry.Mask & ~15u)
        || (signatureMasks[entry.Register] & entry.Mask)) return false;
    signatureMasks[entry.Register] |= entry.Mask;
  }
  StreamOutput candidate;
  std::array<UINT, D3D11_SO_BUFFER_SLOT_COUNT> declarations{};
  bool onlySlotZero = true;
  for (UINT i = 0; i < native.NumEntries; ++i) {
    const auto& entry = native.pOutputStreamDecl[i];
    if (entry.OutputSlot >= D3D11_SO_BUFFER_SLOT_COUNT || !entry.RegisterMask || (entry.RegisterMask & ~15u)) return false;
    onlySlotZero &= entry.OutputSlot == 0;
    const bool gap = entry.RegisterIndex == D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP;
    if (!gap && entry.RegisterIndex >= 32) return false;
    ++declarations[entry.OutputSlot];
    // Native masks can select separated components; API declarations describe
    // contiguous runs. Emit runs in xyzw order without inventing padding.
    for (UINT start = 0; start < 4;) {
      if (!(entry.RegisterMask & (1u << start))) { ++start; continue; }
      const D3D10DDIARG_SIGNATURE_ENTRY* matched = nullptr;
      if (!gap) {
        for (UINT j = 0; j < signature.NumOutputSignatureEntries; ++j) {
          const auto& value = signature.pOutputSignature[j];
          if (value.Register == entry.RegisterIndex && (value.Mask & (1u << start))) {
            if (matched) return false;
            matched = &value;
          }
        }
        if (!matched || !matched->Mask || (matched->Mask & ~15u)
            || UINT(matched->SystemValue) > UINT(D3D10_SB_NAME_CULL_DISTANCE)) return false;
      }
      UINT end = start+1;
      while (end < 4 && (entry.RegisterMask & (1u << end)) && (!matched || (matched->Mask & (1u << end)))) ++end;
      const char* semantic = nullptr;
      UINT semanticIndex = 0, first = 0;
      if (matched) {
        semantic = matched->SystemValue == D3D10_SB_NAME_POSITION ? "SV_Position"
          : matched->SystemValue == D3D10_SB_NAME_CLIP_DISTANCE ? "SV_ClipDistance"
          : matched->SystemValue == D3D10_SB_NAME_CULL_DISTANCE ? "SV_CullDistance" : varyingRegisterSemantic;
        if (matched->SystemValue == D3D10_SB_NAME_UNDEFINED) semanticIndex = entry.RegisterIndex;
        else if (matched->SystemValue != D3D10_SB_NAME_POSITION) {
          // Count full-union registers, including unused earlier distances.
          for (UINT j = 0; j < signature.NumOutputSignatureEntries; ++j) {
            const auto& value = signature.pOutputSignature[j];
            if (value.SystemValue != matched->SystemValue || value.Register >= matched->Register) continue;
            bool earlier = false;
            for (UINT k = 0; k < j; ++k)
              earlier |= signature.pOutputSignature[k].SystemValue == value.SystemValue
                && signature.pOutputSignature[k].Register == value.Register;
            semanticIndex += !earlier;
          }
        }
        while (!(matched->Mask & (1u << first))) ++first;
      }
      // Public components are relative to the semantic's first register lane.
      candidate.entries.push_back({0,semantic,semanticIndex,BYTE(matched ? start-first : 0),BYTE(end-start),BYTE(entry.OutputSlot)});
      candidate.strides[entry.OutputSlot] += (end-start)*4;
      start = end;
    }
    candidate.strideCount = std::max(candidate.strideCount, UINT(entry.OutputSlot)+1);
  }
  if (onlySlotZero) {
    if (native.StreamOutputStrideInBytes % 4 || native.StreamOutputStrideInBytes > D3D10_SO_BUFFER_MAX_STRIDE_IN_BYTES
        || native.StreamOutputStrideInBytes < candidate.strides[0]
        || candidate.strides[0] > D3D10_SO_SINGLE_BUFFER_COMPONENT_LIMIT*4) return false;
    candidate.strides[0] = native.StreamOutputStrideInBytes;
  } else {
    // D3D10 multi-buffer SO permits one output element in each buffer and
    // ignores StreamOutputStrideInBytes; each slot is tightly packed.
    for (UINT count : declarations) if (count > D3D10_SO_MULTIPLE_BUFFER_ELEMENTS_PER_BUFFER) return false;
  }
  output = std::move(candidate);
  return true;
}

}
