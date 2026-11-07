#pragma once
#include <cstdint>

namespace dxvk::umd {
#pragma pack(push, 4)
struct NativeSurfaceInfo {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 128, reserved = 0;
  uint32_t opcode = 8, flags = 0;
  uint64_t expected = 0, key = 0, size = 0, generation = 0, modifier = 0, offset = 0;
  uint32_t resource = 0, context = 0, width = 0, height = 0, fourcc = 0;
  uint32_t stride = 0, planes = 0, layoutFlags = 0;
  uint64_t reserved2[3] = {};
};
struct NativeResourceShare {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 48, reserved = 0;
  uint64_t key = 0;
  uint32_t stride = 0, flags = 1;
  uint64_t reserved2[2] = {};
};
struct NativeSurfaceResource {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 64, reserved = 0;
  uint32_t opcode = 10, flags = 0, allocation = 0, resource = 0;
  uint64_t key = 0, size = 0, generation = 0, reserved2 = 0;
};
struct NativeSurfacePublish {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 40, reserved = 0;
  uint32_t opcode = 11, flags = 0;
  uint64_t key = 0, reserved2 = 0;
};
struct NativeSurfaceCapabilities {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 40, reserved = 0;
  uint32_t opcode = 15, flags = 0;
  uint64_t generation = 0;
  uint32_t capabilities = 0, reserved2 = 0;
};
#pragma pack(pop)
static_assert(sizeof(NativeSurfaceInfo) == 128 && sizeof(NativeResourceShare) == 48);
static_assert(sizeof(NativeSurfaceResource) == 64 && sizeof(NativeSurfacePublish) == 40);
static_assert(sizeof(NativeSurfaceCapabilities) == 40);
constexpr uint32_t NativeSurfaceCapabilityBlt = 1;

inline bool validNativeSurfaceCapabilities(const NativeSurfaceCapabilities& value) {
  return value.magic == 0x504d5644 && !value.version && value.headerSize == sizeof(value)
    && !value.reserved && value.opcode == 15 && !value.flags && value.generation
    && !(value.capabilities & ~NativeSurfaceCapabilityBlt) && !value.reserved2;
}

inline bool validNativeSurface(const NativeSurfaceInfo& value, uint64_t generation,
    uint32_t width, uint32_t height, uint32_t fourcc) {
  return generation && width && height && fourcc && value.magic == 0x504d5644
    && !value.version && value.headerSize == sizeof(value) && !value.reserved
    && value.opcode == 8 && !value.flags && !value.expected
    && value.key && value.key <= UINT32_MAX && value.size && value.size <= (UINT32_MAX & ~uint64_t(4095))
    && value.size % 4096 == 0 && value.generation == generation && !value.modifier && !value.offset
    && value.resource && !value.context && value.width == width && value.height == height
    && value.fourcc == fourcc && value.stride >= uint64_t(width) * 4 && value.stride % 4 == 0
    && value.planes == 1 && value.layoutFlags == 1 && value.size >= uint64_t(value.stride) * height
    && !value.reserved2[0] && !value.reserved2[1] && !value.reserved2[2];
}
inline bool validNativeResourceShare(const NativeResourceShare& share, uint32_t width) {
  return width && share.magic == 0x504d5644 && !share.version
    && share.headerSize == sizeof(share) && !share.reserved && share.key && share.key <= UINT32_MAX
    && share.stride >= uint64_t(width) * 4 && share.stride % 4 == 0 && share.flags == 1
    && !share.reserved2[0] && !share.reserved2[1];
}
}
