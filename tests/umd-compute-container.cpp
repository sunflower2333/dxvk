#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_parser.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "compute container failure line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)

int main() {
  using namespace dxbc_spv;
  const uint32_t ret = (1u << 24) | uint32_t(dxbc::OpCode::eRet);
  const uint32_t group = (4u << 24) | uint32_t(dxbc::OpCode::eDclThreadGroup);
  std::vector<uint32_t> code = {0x50050, 7, group, 4, 2, 1, ret};
  std::vector<unsigned char> binary;
  CHECK(dxvk::umd::buildComputeContainer(code.data(), code.size(), binary));
  dxbc::Container container(binary.data(), binary.size());
  CHECK(container.validateHash());
  CHECK(container.getCodeChunk().getSize() == (code.size() + 2) * sizeof(uint32_t));
  CHECK(!std::memcmp(container.getCodeChunk().getData(8), code.data(), code.size() * sizeof(uint32_t)));
  dxbc::Parser parser(container.getCodeChunk());
  CHECK(parser.getShaderInfo());
  unsigned instructions = 0;
  while (parser) { CHECK(parser.parseInstruction()); ++instructions; }
  CHECK(instructions == 2);
  auto reject = [&](std::vector<uint32_t> bad) {
    binary.assign(8, 0xcc);
    CHECK(!dxvk::umd::buildComputeContainer(bad.data(), bad.size(), binary));
    CHECK(binary.empty());
  };
  auto bad = code; bad[0] = 0x50040; reject(bad);
  bad = code; bad[0] = 0x50; reject(bad);
  bad = code; bad[1]++; reject(bad);
  bad = code; bad[2] = (5u << 24) | uint32_t(dxbc::OpCode::eDclThreadGroup); reject(bad);
  bad = code; bad[3] = 0; reject(bad);
  bad = code; bad[3] = 1025; reject(bad);
  bad = code; bad[4] = 1025; reject(bad);
  bad = code; bad[5] = 65; reject(bad);
  bad = code; bad[3] = 1024; bad[4] = 2; reject(bad);
  bad = code; bad[6] = (1u << 24) | 0x7ff; reject(bad);
  bad = {0x50050, 3, ret}; reject(bad);
  bad = code; bad.insert(bad.end(), code.begin() + 2, code.begin() + 6); bad[1] = uint32_t(bad.size()); reject(bad);
  bad = {0x50050, 8, group, 1, 1, 1, uint32_t(dxbc::OpCode::eCustomData), 100}; reject(bad);
  CHECK(!dxvk::umd::buildComputeContainer(nullptr, 0, binary));
  std::printf("compute container PASS checks=%u exact tokens/hash and malformed SM5 controls\n", checks);
}
