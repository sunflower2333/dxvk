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

inline bool streamOutputDeclaration(const D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT& native,
    const D3D10DDIARG_STAGE_IO_SIGNATURES& signature, StreamOutput& output) {
  output = {};
  if (native.NumEntries > 64 || (native.NumEntries && !native.pOutputStreamDecl)
      || signature.NumOutputSignatureEntries > 32
      || (signature.NumOutputSignatureEntries && !signature.pOutputSignature)) return false;
  StreamOutput candidate;
  std::array<UINT, D3D11_SO_BUFFER_SLOT_COUNT> declarations{};
  bool onlySlotZero = true;
  for (UINT i = 0; i < native.NumEntries; ++i) {
    const auto& entry = native.pOutputStreamDecl[i];
    if (entry.OutputSlot >= D3D11_SO_BUFFER_SLOT_COUNT || !entry.RegisterMask || (entry.RegisterMask & ~15u)) return false;
    onlySlotZero &= entry.OutputSlot == 0;
    const char* semantic = nullptr;
    UINT semanticIndex = 0;
    if (entry.RegisterIndex != D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP) {
      const D3D10DDIARG_SIGNATURE_ENTRY* matched = nullptr;
      for (UINT j = 0; j < signature.NumOutputSignatureEntries; ++j) {
        const auto& value = signature.pOutputSignature[j];
        if (value.Register == entry.RegisterIndex) {
          if (matched) return false;
          matched = &value;
        }
      }
      if (!matched || (matched->Mask & entry.RegisterMask) != entry.RegisterMask
          || (matched->SystemValue != D3D10_SB_NAME_UNDEFINED && matched->SystemValue != D3D10_SB_NAME_POSITION)) return false;
      semantic = matched->SystemValue == D3D10_SB_NAME_POSITION ? "SV_Position" : varyingRegisterSemantic;
      semanticIndex = matched->SystemValue == D3D10_SB_NAME_POSITION ? 0 : entry.RegisterIndex;
    }
    ++declarations[entry.OutputSlot];
    // Native masks can select separated components; API declarations describe
    // contiguous runs. Emit runs in xyzw order without inventing padding.
    for (UINT start = 0; start < 4;) {
      if (!(entry.RegisterMask & (1u << start))) { ++start; continue; }
      UINT end = start+1;
      while (end < 4 && (entry.RegisterMask & (1u << end))) ++end;
      candidate.entries.push_back({0,semantic,semanticIndex,BYTE(semantic ? start : 0),BYTE(end-start),BYTE(entry.OutputSlot)});
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
