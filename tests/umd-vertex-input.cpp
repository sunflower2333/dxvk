// SPDX-License-Identifier: MIT
#include "dxvk_vertex_input.h"
#include <vulkan/vulkan.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

// Use the Vulkan description types consumed by the production comparison.
// This fixture needs neither a graphics device nor the Windows runtime.
struct VertexInputState {
  VkPipelineInputAssemblyStateCreateInfo iaInfo = {};
  VkPipelineVertexInputStateCreateInfo viInfo = {};
  VkPipelineVertexInputDivisorStateCreateInfoEXT viDivisorInfo = {};
  std::array<VkVertexInputBindingDescription, 32> viBindings = {};
  std::array<VkVertexInputAttributeDescription, 32> viAttributes = {};
  std::array<VkVertexInputBindingDivisorDescriptionEXT, 32> viDivisors = {};
  VkBool32 viUseDynamicVertexStrides = VK_FALSE;
};

static unsigned checks;
static void check(bool condition, unsigned line) {
  checks++;
  if (!condition) {
    std::fprintf(stderr, "FAIL vertex input line=%u\n", line);
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x), __LINE__)

struct Equal {
  bool operator()(const VertexInputState& a, const VertexInputState& b) const {
    return dxvk::equalVertexInputState(a, b);
  }
};

// Equality must distinguish layouts even when the hash table collides.
struct CollisionHash {
  size_t operator()(const VertexInputState&) const { return 0; }
};

static void different(const VertexInputState& a, const VertexInputState& b) {
  CHECK(!dxvk::equalVertexInputState(a, b));
  CHECK(!dxvk::equalVertexInputState(b, a));
  CHECK(dxvk::equalVertexInputState(a, a));
  CHECK(dxvk::equalVertexInputState(b, b));
}

int main() {
  VertexInputState base;
  base.iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  base.viInfo.vertexBindingDescriptionCount = 2;
  base.viInfo.vertexAttributeDescriptionCount = 2;
  base.viDivisorInfo.vertexBindingDivisorCount = 1;
  base.viBindings[0] = {0, 28, VK_VERTEX_INPUT_RATE_VERTEX};
  base.viBindings[1] = {1, 0, VK_VERTEX_INPUT_RATE_INSTANCE};
  base.viAttributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
  base.viAttributes[1] = {1, 0, VK_FORMAT_B8G8R8A8_UNORM, 24};
  base.viDivisors[0] = {1, 0}; // Missing shader inputs use a zero-divisor buffer.

  auto color = base;
  color.viAttributes[1].offset = 12;
  // A matching null-input divisor must not erase this earlier inequality.
  CHECK(!dxvk::equalVertexInputState(base, color));
  different(base, color);

  for (unsigned field = 0; field < 15; field++) {
    auto other = base;
    switch (field) {
      case 0: other.iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
      case 1: other.iaInfo.primitiveRestartEnable = VK_TRUE; break;
      case 2: other.viInfo.vertexBindingDescriptionCount = 1; break;
      case 3: other.viInfo.vertexAttributeDescriptionCount = 1; break;
      case 4: other.viDivisorInfo.vertexBindingDivisorCount = 0; break;
      case 5: other.viUseDynamicVertexStrides = VK_TRUE; break;
      case 6: other.viBindings[0].binding = 3; break;
      case 7: other.viBindings[0].stride = 16; break;
      case 8: other.viBindings[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE; break;
      case 9: other.viAttributes[1].location = 3; break;
      case 10: other.viAttributes[1].binding = 1; break;
      case 11: other.viAttributes[1].format = VK_FORMAT_R8G8B8A8_UNORM; break;
      case 12: other.viAttributes[1].offset = 16; break;
      case 13: other.viDivisors[0].binding = 0; break;
      case 14: other.viDivisors[0].divisor = 2; break;
    }
    different(base, other);
  }

  // Later matching divisors must also preserve a mismatch in an earlier one.
  auto many = base;
  many.viDivisorInfo.vertexBindingDivisorCount = 32;
  for (uint32_t i = 0; i < 32; i++) many.viDivisors[i] = {i, i};
  for (uint32_t i = 0; i < 32; i++) {
    auto other = many;
    other.viDivisors[i].divisor++;
    different(many, other);
    other = many;
    other.viDivisors[i].binding++;
    different(many, other);
  }

  // Array storage, create-info pointer addresses and unused entries are not keys.
  auto copy = base;
  copy.viInfo.pVertexBindingDescriptions = copy.viBindings.data();
  copy.viInfo.pVertexAttributeDescriptions = copy.viAttributes.data();
  copy.viDivisorInfo.pVertexBindingDivisors = copy.viDivisors.data();
  copy.viBindings[31].stride = 999;
  copy.viAttributes[31].offset = 999;
  copy.viDivisors[31].divisor = 999;
  CHECK(dxvk::equalVertexInputState(base, copy));
  CHECK(dxvk::equalVertexInputState(copy, base));

  auto noDivisors = base;
  noDivisors.viDivisorInfo.vertexBindingDivisorCount = 0;
  auto noDivisorsColor = noDivisors;
  noDivisorsColor.viAttributes[1].offset = 12;
  different(noDivisors, noDivisorsColor);
  CHECK(dxvk::equalVertexInputState(VertexInputState{}, VertexInputState{}));

  std::unordered_map<VertexInputState, unsigned, CollisionHash, Equal> cache;
  std::array<VertexInputState, 4> layouts = {base, color, color, base};
  layouts[2].viBindings[0].stride = 16;
  layouts[3].viAttributes[1].offset = 16;
  for (unsigned i = 0; i < layouts.size(); i++) {
    CHECK(cache.emplace(layouts[i], i).second);
    CHECK(cache.at(layouts[i]) == i);
  }
  CHECK(cache.size() == layouts.size());
  CHECK(!cache.emplace(copy, 99).second);
  for (unsigned i = 0; i < layouts.size(); i++) CHECK(cache.at(layouts[i]) == i);

  std::printf("vertex input equality PASS checks=%u; colliding layouts retained, no GPU runtime\n", checks);
}
