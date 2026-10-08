#include "umd-d3d8-runtime-policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::fprintf(stderr, "D3D8 runtime policy CHECK failed line=%u check=%u: %s\n", \
    unsigned(__LINE__), checks, #condition); std::exit(1); } } while (0)
using View = std::u16string_view;
constexpr char16_t installed[] = u"C:\\WINDOWS\\System32\\DriverStore\\FileRepository\\viogpu\\viogpu_legacy_x86.dll";
constexpr char16_t selected[] = u"C:\\Users\\Public\\DxvkD3D8Runtime-owned01\\viogpu-d3d8-runtime-front.dll";
constexpr char16_t coreCommit[] = u"0123456789abcdef0123456789abcdef01234567";
constexpr char16_t corePath[] = u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-37500000001\\viogpudxvk.dll";
constexpr size_t queryBytes = sizeof(uint32_t) + 260 * sizeof(char16_t);
struct GuardedName { uint32_t before = 0xa1b2c3d4; char16_t value[260]{}; uint32_t after = 0xb1c2d3e4; };
GuardedName name(View text) {
  GuardedName result;
  CHECK(text.size() < std::size(result.value));
  std::copy(text.begin(), text.end(), std::begin(result.value));
  return result;
}
bool select(GuardedName& output, int32_t status = 0, uint32_t type = 1,
            size_t bytes = queryBytes, uint32_t version = 0,
            View expected = installed, View replacement = selected) {
  return dxvk::test::runtime8::selectDriverName(status, type, bytes, queryBytes, version,
                                              output.value, expected, replacement);
}
}

int main() {
  namespace policy = dxvk::test::runtime8;
  using Mode = policy::Mode;
  CHECK(policy::permissionMode(View(u"read-only-d3d8-interface8-v1")) == Mode::ReadOnly);
  CHECK(policy::permissionMode(View(u"device-render-d3d8-interface8-v1")) == Mode::Device);
  CHECK(policy::permissionMode(View(u"enumerate-device-d3d8-interface8-v1")) == Mode::EnumerateDevice);
  for (View bad : {View(u""), View(u"device-render-d3d8-interface8-v1 "),
      View(u"DEVICE-render-d3d8-interface8-v1"), View(u"device-render-d3d9-interface8-v1"),
      View(u"device-render-d3d8-interface9-v1"), View(u"enumerate-device-d3d8-interface8-v1 "),
      View(u"Enumerate-device-d3d8-interface8-v1")})
    CHECK(policy::permissionMode(bad) == Mode::Denied);
  std::u16string embeddedPermission(u"device-render-d3d8-interface8-v1");
  embeddedPermission.push_back(0); embeddedPermission.append(u"extra");
  CHECK(policy::permissionMode(View(embeddedPermission)) == Mode::Denied);
  for (Mode captured : {Mode::Denied, Mode::ReadOnly, Mode::Device, Mode::EnumerateDevice})
    for (Mode current : {Mode::Denied, Mode::ReadOnly, Mode::Device, Mode::EnumerateDevice}) {
      CHECK(policy::mayCreateDevice(captured, current, 8)
        == ((captured == Mode::Device || captured == Mode::EnumerateDevice) && current == captured));
      CHECK(policy::mayRender(captured, current)
        == (captured == Mode::Device && current == captured));
    }
  for (uint32_t badApi : {0u, 7u, 9u, 10u, 11u, 0xffffffffu}) {
    CHECK(!policy::mayCreateDevice(Mode::Device, Mode::Device, badApi));
    CHECK(!policy::mayCreateDevice(Mode::EnumerateDevice, Mode::EnumerateDevice, badApi));
    CHECK(!policy::enumerationContract(badApi, 69632, 69632, 0));
  }
  for (uint32_t version : {0u, 12u, 69632u, 0xffffffffu}) {
    CHECK(policy::enumerationContract(8, version, version, 0));
    CHECK(!policy::enumerationContract(8, version, version ^ 1u, 0));
    for (uint32_t bit = 0; bit < 32; ++bit)
      CHECK(!policy::enumerationContract(8, version, version, 1u << bit));
  }
  CHECK(policy::capsBytes == 212);
  CHECK(policy::hexIdentity(View(coreCommit), 40));
  CHECK(policy::hexIdentity(View(u"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"), 64));
  for (View invalid : {View(u""), View(u"0123456789abcdef"),
      View(u"0123456789abcdef0123456789abcdef0123456g"),
      View(u"0123456789abcdef0123456789abcdef012345678"),
      View(u"0123456789abcdef0123456789abcdef0123456 ")}) CHECK(!policy::hexIdentity(invalid, 40));
  auto nulCommit = std::u16string(coreCommit); nulCommit[20] = 0;
  CHECK(!policy::hexIdentity(View(nulCommit), 40));
  CHECK(policy::ownedCorePath(View(corePath), View(coreCommit)));
  CHECK(policy::ownedCorePath(View(u"c:\\users\\public\\dxvkd3d8candidate-0123456-1\\VIOGPUDXVK.DLL"), View(coreCommit)));
  CHECK(!policy::ownedCorePath(View(corePath), View(u"abcdef6789abcdef0123456789abcdef01234567")));
  CHECK(!policy::ownedCorePath(View(corePath), View(u"0123456")));
  for (View invalid : {View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-\\viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-0\\viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-01\\viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-1a\\viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-123456789012345678901\\viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-1\\..\\viogpudxvk.dll"),
      View(u"C:/Users/Public/DxvkD3D8Candidate-0123456-1/viogpudxvk.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-1\\viogpudxvk.dll:stream"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-0123456-1\\viogpudxvk-x86.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Candidate-b75d6d5-x86\\viogpudxvk.dll"),
      View(u""), View(u"viogpudxvk.dll")}) CHECK(!policy::ownedCorePath(invalid, View(coreCommit)));
  auto nulCore = std::u16string(corePath); nulCore[40] = 0;
  CHECK(!policy::ownedCorePath(View(nulCore), View(coreCommit)));
  CHECK(policy::ownedFrontPath(View(selected)));
  CHECK(policy::ownedFrontPath(View(u"c:\\users\\public\\dxvkd3d8runtime-other\\VIOGPU-D3D8-RUNTIME-FRONT.DLL")));
  for (View invalid : {View(u"C:\\Users\\Public\\DxvkD3D8Runtime-a\\..\\viogpu-d3d8-runtime-front.dll"),
      View(u"C:/Users/Public/DxvkD3D8Runtime-a/viogpu-d3d8-runtime-front.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Runtime-a\\viogpu-d3d8-runtime-front.dll:stream"),
      View(u"C:\\Users\\Public\\DxvkD3D9Runtime-a\\viogpu-d3d8-runtime-front.dll"),
      View(u"C:\\Users\\Public\\DxvkD3D8Runtime-a\\viogpu-d3d9-runtime-front.dll"),
      View(u"viogpu-d3d8-runtime-front.dll"), View(u""),
      View(u"C:\\Users\\Public\\DxvkD3D8Runtime-\\viogpu-d3d8-runtime-front.dll")})
    CHECK(!policy::ownedFrontPath(invalid));
  auto embeddedNul = std::u16string(selected);
  embeddedNul[40] = 0; CHECK(!policy::ownedFrontPath(View(embeddedNul)));
  auto tooLong = std::u16string(u"C:\\Users\\Public\\DxvkD3D8Runtime-");
  tooLong.append(260, u'x'); tooLong.append(u"\\viogpu-d3d8-runtime-front.dll");
  CHECK(!policy::ownedFrontPath(View(tooLong)));

  auto output = name(installed);
  CHECK(select(output));
  CHECK(policy::equalName(View(output.value), View(selected)));
  CHECK(output.before == 0xa1b2c3d4 && output.after == 0xb1c2d3e4);
  for (size_t i = std::size(selected) - 1; i < std::size(output.value); ++i) CHECK(output.value[i] == 0);
  // The original installed name is ASCII case-insensitive, but no substring,
  // alternate architecture, truncated filename or replacement alias matches.
  output = name(u"c:\\windows\\system32\\driverstore\\filerepository\\viogpu\\VIOGPU_LEGACY_X86.DLL");
  CHECK(select(output));
  for (int32_t status : {int32_t(-1), int32_t(1), int32_t(0x40000000), int32_t(0x80000001u)}) {
    output = name(installed); const auto before = output;
    CHECK(!select(output, status)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  }
  for (uint32_t type : {0u, 2u, 33u, 71u, 0xffffffffu}) {
    output = name(installed); const auto before = output;
    CHECK(!select(output, 0, type)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  }
  for (uint32_t version : {1u, 2u, 3u, 0xffffffffu}) {
    output = name(installed); const auto before = output;
    CHECK(!select(output, 0, 1, queryBytes, version)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  }
  for (size_t bytes : {size_t(0), queryBytes - 1, queryBytes + 1, size_t(-1)}) {
    output = name(installed); const auto before = output;
    CHECK(!select(output, 0, 1, bytes)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  }
  for (View original : {View(u"viogpu_legacy_x86.dll"),
      View(u"C:\\WINDOWS\\System32\\DriverStore\\FileRepository\\viogpu\\viogpu_legacy_arm64.dll"),
      View(u"C:\\WINDOWS\\System32\\DriverStore\\FileRepository\\viogpu\\viogpu_legacy_x86.dll.extra"),
      View(selected)}) {
    output = name(original); const auto before = output;
    CHECK(!select(output)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  }
  output = name(installed);
  std::fill(std::begin(output.value), std::end(output.value), u'x');
  auto before = output;
  CHECK(!select(output)); CHECK(!std::memcmp(&before, &output, sizeof(output)));
  output = name(installed); before = output;
  CHECK(!select(output, 0, 1, queryBytes, 0, View{}));
  CHECK(!std::memcmp(&before, &output, sizeof(output)));
  CHECK(!select(output, 0, 1, queryBytes, 0, installed,
                u"C:\\Users\\Public\\DxvkD3D8Runtime-other\\viogpu-d3d9-runtime-front.dll"));
  CHECK(!std::memcmp(&before, &output, sizeof(output)));
  std::printf("D3D8 runtime selector policy PASS checks=%u; exact original status/name, UTF16 canaries and owned paths; no Windows runtime or GPU\n", checks);
}
