#include "../src/umd/umd_legacy_api.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static unsigned checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr,"legacy API CHECK line=%d: %s\n",__LINE__,#value); std::_Exit(1); } } while (0)
int main() {
  using namespace dxvk::umd;
  for (uint32_t api = 0; api < 128; ++api) {
    CHECK(validLegacyD3DApi(api) == (api == 8 || api == 9));
    CHECK(legacyD3DApiMatches(LegacyD3DApi::D3D8,api) == (api == 8));
    CHECK(legacyD3DApiMatches(LegacyD3DApi::D3D9,api) == (api == 9));
    CHECK(!legacyD3DApiMatches(LegacyD3DApi(0),api));
    CHECK(!legacyD3DApiMatches(LegacyD3DApi(UINT32_MAX),api));
  }
  for (const uint32_t stage : {0xfffe0000u,0xffff0000u,0u,0xffffffffu})
    for (uint32_t major = 0; major < 5; ++major)
      for (uint32_t minor = 0; minor < 8; ++minor)
        for (const bool vertex : {false,true}) {
          const uint32_t version = stage | major * 256 | minor;
          CHECK(legacyShaderModelAllowed(LegacyD3DApi::D3D8,version,vertex)
              == (stage == (vertex ? 0xfffe0000u : 0xffff0000u) && major == 1 && minor <= (vertex ? 1u : 4u)));
          CHECK(legacyShaderModelAllowed(LegacyD3DApi::D3D9,version,vertex));
          CHECK(!legacyShaderModelAllowed(LegacyD3DApi(0),version,vertex));
        }
  for (uint32_t limit = 0; limit < 300; ++limit) {
    CHECK(legacyFloatConstantLimit(LegacyD3DApi::D3D8,true,limit) == (limit < 96 ? limit : 96));
    CHECK(legacyFloatConstantLimit(LegacyD3DApi::D3D8,false,limit) == (limit < 8 ? limit : 8));
    CHECK(legacyFloatConstantLimit(LegacyD3DApi::D3D9,true,limit) == limit);
    CHECK(legacyFloatConstantLimit(LegacyD3DApi(0),false,limit) == 0);
  }
  std::printf("legacy8/9 API bounds PASS checks=%u; renderer framing follows, admission unchanged\n",checks);
}
