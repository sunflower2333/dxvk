#pragma once

#include <d3d9.h>
#include <cstddef>

namespace dxvk {

// Bound the native DDI's token stream before handing it to the SM1-3
// translator. This checks framing, not shader semantics: the renderer still
// validates registers, stage/version restrictions and instruction semantics.
inline bool validateD3D9ShaderCode(const DWORD* code, size_t bytes, bool vertex) {
  if (!code || bytes < 8 || bytes % sizeof(DWORD)) return false;
  const size_t words = bytes / sizeof(DWORD);
  const DWORD version = code[0];
  const UINT major = (version >> 8) & 255, minor = version & 255;
  if ((version >> 16) != (vertex ? 0xfffeu : 0xffffu)
   || !((major == 1 && minor <= (vertex ? 1u : 4u))
      || major == 2 || (major == 3 && !minor))) return false;

  size_t cursor = 1;
  while (cursor < words) {
    const DWORD token = code[cursor++];
    const DWORD opcode = token & 0xffffu;
    if (opcode == D3DSIO_END) return token == DWORD(D3DSIO_END) && cursor == words;
    if (token & 0x80000000u) return false;
    if (opcode == D3DSIO_COMMENT) {
      const size_t count = (token >> 16) & 0x7fffu;
      if (count > words - cursor) return false;
      cursor += count;
      continue;
    }

    // Operand shapes used by the embedded SM parser: D=destination,
    // S=source, L=declaration, 4=vec4 immediate, 1=boolean immediate.
    const char* layout = nullptr;
    switch (opcode) {
      case D3DSIO_NOP: case D3DSIO_RET: case D3DSIO_ENDLOOP:
      case D3DSIO_ENDREP: case D3DSIO_ELSE: case D3DSIO_ENDIF:
      case D3DSIO_BREAK: layout = ""; break;
      case D3DSIO_MOV: case D3DSIO_RCP: case D3DSIO_RSQ:
      case D3DSIO_EXP: case D3DSIO_LOG: case D3DSIO_LIT:
      case D3DSIO_FRC: case D3DSIO_ABS: case D3DSIO_NRM:
      case D3DSIO_MOVA: case D3DSIO_TEXBEM: case D3DSIO_TEXBEML:
      case D3DSIO_TEXREG2AR: case D3DSIO_TEXREG2GB:
      case D3DSIO_TEXM3x2PAD: case D3DSIO_TEXM3x2TEX:
      case D3DSIO_TEXM3x3PAD: case D3DSIO_TEXM3x3TEX:
      case D3DSIO_TEXM3x3VSPEC: case D3DSIO_EXPP: case D3DSIO_LOGP:
      case D3DSIO_TEXREG2RGB: case D3DSIO_TEXDP3TEX:
      case D3DSIO_TEXM3x2DEPTH: case D3DSIO_TEXDP3:
      case D3DSIO_TEXM3x3: case D3DSIO_DSX: case D3DSIO_DSY:
        layout = "DS"; break;
      case D3DSIO_ADD: case D3DSIO_SUB: case D3DSIO_MUL:
      case D3DSIO_DP3: case D3DSIO_DP4: case D3DSIO_MIN:
      case D3DSIO_MAX: case D3DSIO_SLT: case D3DSIO_SGE:
      case D3DSIO_DST: case D3DSIO_M4x4: case D3DSIO_M4x3:
      case D3DSIO_M3x4: case D3DSIO_M3x3: case D3DSIO_M3x2:
      case D3DSIO_POW: case D3DSIO_CRS: case D3DSIO_TEXM3x3SPEC:
      case D3DSIO_BEM: case D3DSIO_TEXLDL: case D3DSIO_SETP:
        layout = "DSS"; break;
      case D3DSIO_MAD: case D3DSIO_LRP: case D3DSIO_SGN:
      case D3DSIO_CND: case D3DSIO_CMP: case D3DSIO_DP2ADD:
        layout = "DSSS"; break;
      case D3DSIO_CALL: case D3DSIO_REP: case D3DSIO_IF:
        layout = "S"; break;
      case D3DSIO_CALLNZ: case D3DSIO_IFC: case D3DSIO_BREAKC:
        layout = "SS"; break;
      case D3DSIO_LOOP: layout = "DS"; break;
      case D3DSIO_LABEL: case D3DSIO_TEXKILL: case D3DSIO_TEXDEPTH:
      case D3DSIO_BREAKP: layout = "D"; break;
      case D3DSIO_DCL: layout = "LD"; break;
      case D3DSIO_DEFB: layout = "D1"; break;
      case D3DSIO_DEFI: case D3DSIO_DEF: layout = "D4"; break;
      case D3DSIO_SINCOS: layout = major <= 2 ? "DSSS" : "DS"; break;
      case D3DSIO_TEXCOORD: layout = major == 1 && minor < 4 ? "D" : "DS"; break;
      case D3DSIO_TEX: layout = major < 2 ? (minor < 4 ? "D" : "DS") : "DSS"; break;
      case D3DSIO_TEXLDD: layout = "DSSSS"; break;
      case D3DSIO_PHASE:
        if (token != DWORD(D3DSIO_PHASE) || vertex || major != 1 || minor != 4) return false;
        layout = ""; break;
      default: return false;
    }
    const bool predicated = (token & D3DSHADER_INSTRUCTION_PREDICATED) != 0;
    if (predicated && major < 2) return false;
    size_t end = words;
    if (major >= 2) {
      const size_t count = (token >> 24) & 15;
      if (count > words - cursor) return false;
      end = cursor + count;
    }
    for (const char* kind = layout; *kind; ++kind) {
      if (*kind == '4' || *kind == '1') {
        const size_t count = *kind == '4' ? 4u : 1u;
        if (count > end - cursor) return false;
        cursor += count;
      } else {
        if (cursor == end) return false;
        const DWORD operand = code[cursor++];
        if (!(operand & 0x80000000u)) return false;
        const bool extraAddress = (*kind == 'S' && major >= 2) || (*kind == 'D' && major >= 3);
        if (extraAddress && (operand & D3DSHADER_ADDRESSMODE_MASK)) {
          if (cursor == end || !(code[cursor++] & 0x80000000u)) return false;
        }
        if (*kind == 'D' && predicated) {
          if (cursor == end || !(code[cursor++] & 0x80000000u)) return false;
        }
      }
    }
    if (major >= 2 && cursor != end) return false;
  }
  return false;
}

}
