#pragma once
#include <cstdint>
#include <cstddef>

namespace dxvk::umd {

// Numeric fields are deliberately separate from the Windows ABI. The native
// boundary checks these values against the original SDK constants.
struct PrimaryShape {
  uint32_t flags = 0, source = 0, width = 0, height = 0, format = 0;
  uint32_t modeWidth = 0, modeHeight = 0, modeFormat = 0;
  uint32_t numerator = 0, denominator = 0, scanline = 0, rotation = 0, scaling = 0;
};
enum class PrimaryStatus { Valid, Invalid, Unsupported };
struct PrimaryPlan {
  bool scanout = false;
  uint32_t driverFlags = 0;
  uint32_t pitch = 0;
  uint64_t bytes = 0;
};
inline PrimaryStatus primaryPlan(const PrimaryShape& shape, PrimaryPlan& output) {
  if ((shape.flags & ~15u) || !shape.width || !shape.height
      || shape.width > 16384 || shape.height > 16384
      || shape.width != shape.modeWidth || shape.height != shape.modeHeight
      || shape.format != shape.modeFormat || shape.scanline > 3
      || shape.rotation > 4 || shape.scaling > 2) return PrimaryStatus::Invalid;
  // v0 KMD has one present source, no stereo/indirect primary and no rotated
  // or interlaced scanout metadata. No flags can make those paths succeed.
  if (shape.source || (shape.flags & 12u) || shape.rotation > 1
      || shape.scanline > 1 || shape.scaling == 2) return PrimaryStatus::Unsupported;
  const bool optional = (shape.flags & 1u) != 0;
  // BGRA sRGB scanout uses the same encoded v0 BGRA8 bytes, without a gamma
  // transform. BGRX sRGB has no scanout bit; it is an optional copy only.
  const bool linear = shape.format == 28 || shape.format == 87 || shape.format == 88;
  if (!linear && shape.format != 91 && !(optional && (shape.format == 29 || shape.format == 93)))
    return PrimaryStatus::Unsupported;
  if (!optional && (!shape.numerator || !shape.denominator)) return PrimaryStatus::Invalid;
  PrimaryPlan plan;
  plan.scanout = !optional; plan.driverFlags = optional ? 1 : 0;
  plan.pitch = shape.width * 4; plan.bytes = uint64_t(plan.pitch) * shape.height;
  output = plan;
  return PrimaryStatus::Valid;
}

// Exact v0 VIOGPU_WDDM_ALLOCATION_COPY wire packet. Allocation identities
// reside in the runtime allocation list, never in pointer/address fields.
struct PrimaryCopy {
  uint32_t magic = 0x504d5644, version = 0, size = 64, reserved = 0;
  uint32_t opcode = 2, flags = 0, width = 0, height = 0;
  uint32_t padding[8] = {};
};
static_assert(sizeof(PrimaryCopy) == 64);
static_assert(offsetof(PrimaryCopy, opcode) == 16 && offsetof(PrimaryCopy, width) == 24);
}
