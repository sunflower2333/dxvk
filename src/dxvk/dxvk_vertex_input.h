#pragma once

#include <cstdint>

namespace dxvk {

  // The state owns its description arrays. Compare those arrays rather than
  // the Vulkan create-info pointers, which can refer to a different copy.
  template<typename State>
  bool equalVertexInputState(const State& state, const State& other) {
    bool eq = state.iaInfo.topology                         == other.iaInfo.topology
           && state.iaInfo.primitiveRestartEnable           == other.iaInfo.primitiveRestartEnable
           && state.viInfo.vertexBindingDescriptionCount    == other.viInfo.vertexBindingDescriptionCount
           && state.viInfo.vertexAttributeDescriptionCount  == other.viInfo.vertexAttributeDescriptionCount
           && state.viDivisorInfo.vertexBindingDivisorCount == other.viDivisorInfo.vertexBindingDivisorCount
           && state.viUseDynamicVertexStrides               == other.viUseDynamicVertexStrides;

    for (uint32_t i = 0; i < state.viInfo.vertexBindingDescriptionCount && eq; i++) {
      const auto& a = state.viBindings[i];
      const auto& b = other.viBindings[i];

      eq = a.binding    == b.binding
        && a.stride     == b.stride
        && a.inputRate  == b.inputRate;
    }

    for (uint32_t i = 0; i < state.viInfo.vertexAttributeDescriptionCount && eq; i++) {
      const auto& a = state.viAttributes[i];
      const auto& b = other.viAttributes[i];

      eq = a.location   == b.location
        && a.binding    == b.binding
        && a.format     == b.format
        && a.offset     == b.offset;
    }

    for (uint32_t i = 0; i < state.viDivisorInfo.vertexBindingDivisorCount && eq; i++) {
      const auto& a = state.viDivisors[i];
      const auto& b = other.viDivisors[i];

      eq = a.binding    == b.binding
        && a.divisor    == b.divisor;
    }

    return eq;
  }

}
