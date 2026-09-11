#include "../src/umd/umd_runtime_identity.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace dxvk::umd;
static unsigned checks;
static void check(bool value) { checks++; if (!value) std::abort(); }
int main() {
  std::vector<uint8_t> reply(RuntimeIdentityReplySize);
  auto set = [&](size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; i++) reply[offset+i] = uint8_t(value >> (8*i));
  };
  set(0, 0x504d5644); set(8, 128); set(128, 0x44494c56);
  set(132, 1); set(136, 32); set(140, 1); set(152, 1);
  for (unsigned i = 0; i < 8; i++) reply[144+i] = uint8_t(i+1);
  AdapterLuid luid = {};
  check(readProposedRuntimeIdentity(reply.data(), reply.size(), luid));
  check(luid == AdapterLuid{1,2,3,4,5,6,7,8});
  const auto valid = reply;
  for (size_t size = 0; size < reply.size(); size++) {
    luid.fill(0xff);
    check(!readProposedRuntimeIdentity(reply.data(), size, luid) && luid == AdapterLuid{});
  }
  check(!readProposedRuntimeIdentity(nullptr, reply.size(), luid));
  check(!readProposedRuntimeIdentity(reply.data(), reply.size()+1, luid));
  for (size_t offset : std::array<size_t,10>{0,4,8,12,128,132,136,140,152,156}) {
    reply = valid;
    reply[offset] ^= 1;
    check(!readProposedRuntimeIdentity(reply.data(), reply.size(), luid) && luid == AdapterLuid{});
  }
  reply = valid;
  for (unsigned i = 0; i < 8; i++) reply[144+i] = 0;
  check(!readProposedRuntimeIdentity(reply.data(), reply.size(), luid));
  // Existing KMD fills only the original prefix. A zeroed larger caller
  // buffer must not accidentally activate the identity path.
  reply = valid;
  for (size_t i = 128; i < reply.size(); i++) reply[i] = 0;
  check(!readProposedRuntimeIdentity(reply.data(), reply.size(), luid));
  std::printf("proposed runtime identity PASS checks=%u\n", checks);
}
