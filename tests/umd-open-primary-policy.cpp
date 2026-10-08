// SPDX-License-Identifier: MIT
#include "../src/umd/umd_open_primary_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <initializer_list>

using dxvk::umd::PrimaryStatus;
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) std::abort(); } while (0)
// Independent numeric input, without including or substituting Windows ABI.
struct Wire {
  uint32_t magic=0x504d5644, version=0, headerSize=80, reserved=0;
  uint64_t size=200, alignment=4096, requestedIova=0, resetGeneration=0;
  uint32_t flags=1, format=3, width=7, height=5, pitch=40;
  uint32_t refreshNumerator=60000, refreshDenominator=1001, contextId=0;
};
int main() {
  for (uint32_t format=1;format<=3;++format)
    for (uint32_t width=1;width<=33;++width)
      for (uint32_t height=1;height<=17;++height) {
        Wire wire;wire.format=format;wire.width=width;wire.height=height;
        wire.pitch=width*4+12;wire.size=uint64_t(wire.pitch)*height;
        CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Valid);
        const auto original=wire;
        wire.pitch=width*4-1;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Invalid);wire=original;
        wire.size=uint64_t(wire.pitch)*height-1;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Invalid);wire=original;
        wire.pitch|=1;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Invalid);
      }
  Wire wire;
  for (auto field : {&Wire::magic,&Wire::headerSize,&Wire::width,&Wire::height,&Wire::refreshNumerator,&Wire::refreshDenominator}) {
    auto changed=wire;changed.*field=0;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Invalid);
  }
  for (auto field : {&Wire::version,&Wire::reserved}) {
    auto changed=wire;changed.*field=1;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Invalid);
  }
  for (uint32_t flags : {0u,2u,3u,4u,0xffffffffu}) {
    auto changed=wire;changed.flags=flags;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Unsupported);
  }
  for (uint32_t format : {0u,4u,28u,87u,88u,0xffffffffu}) {
    auto changed=wire;changed.format=format;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Unsupported);
  }
  for (auto field : {&Wire::requestedIova,&Wire::resetGeneration}) {
    auto changed=wire;changed.*field=4096;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Unsupported);
  }
  for (uint64_t size : {uint64_t(1)<<32,std::numeric_limits<uint64_t>::max()}) {
    auto changed=wire;changed.size=size;CHECK(dxvk::umd::openedPrimaryStatus(changed)==PrimaryStatus::Unsupported);
  }
  wire.width=16384;wire.height=16384;wire.pitch=65536;wire.size=uint64_t(wire.pitch)*wire.height;
  CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Valid);
  wire.width=16385;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Invalid);
  wire=Wire{};wire.height=16385;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Invalid);
  wire=Wire{};wire.contextId=1;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Unsupported);
  wire=Wire{};wire.alignment=8192;CHECK(dxvk::umd::openedPrimaryStatus(wire)==PrimaryStatus::Unsupported);
  std::printf("Opened primary policy PASS checks=%u\n",checks);
}
