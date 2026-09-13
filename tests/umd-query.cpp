#include "../src/umd/umd_query.h"
#include "../src/umd/umd_map.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
  std::fprintf(stderr, "query check %u failed at line %d: %s\n", checks, __LINE__, #condition); \
  std::exit(1); } } while (0)

int main() {
  using namespace dxvk::umd;
  QueryInfo info;
  CHECK(queryInfo(D3D10DDI_QUERY_EVENT, 0, info) && info.size == sizeof(BOOL) && !info.beginRequired);
  CHECK(queryInfo(D3D10DDI_QUERY_OCCLUSION, 0, info) && info.size == sizeof(UINT64) && info.beginRequired);
  CHECK(queryInfo(D3D10DDI_QUERY_TIMESTAMP, 0, info) && info.type == D3D11_QUERY_TIMESTAMP && !info.beginRequired);
  CHECK(queryInfo(D3D10DDI_QUERY_TIMESTAMPDISJOINT, 0, info) && info.size == sizeof(D3D10_DDI_QUERY_DATA_TIMESTAMP_DISJOINT));
  CHECK(!queryInfo(D3D10DDI_QUERY_PIPELINESTATS, 0, info) && info.size == 0);
  CHECK(!queryInfo(D3D10DDI_QUERY_EVENT, D3D10DDI_QUERY_MISCFLAG_PREDICATEHINT, info) && info.size == 0);
  CHECK(!queryInfo(static_cast<D3D10DDI_QUERY>(0x7fffffff), 0, info));
  CHECK(queryInfo(D3D10DDI_QUERY_OCCLUSIONPREDICATE, 0, info));
  CHECK(info.predicate && info.beginRequired && !info.hint && info.size == sizeof(BOOL));
  CHECK(queryInfo(D3D10DDI_QUERY_OCCLUSIONPREDICATE, D3D10DDI_QUERY_MISCFLAG_PREDICATEHINT, info));
  CHECK(info.predicate && info.hint && info.type == D3D11_QUERY_OCCLUSION_PREDICATE);
  CHECK(readQueryData(info, nullptr, 0, 0, [](void*, UINT, UINT) { std::abort(); return S_OK; }) == E_INVALIDARG);
  CHECK(!queryInfo(D3D10DDI_QUERY_OCCLUSIONPREDICATE, 2, info));
  CHECK(queryInfo(D3D10DDI_QUERY_TIMESTAMP, 0, info));
  const UINT64 original = 0xaaaaaaaa55555555ull;
  const UINT64 expected = 0x12345678fedcba98ull;
  UINT64 data = original;
  unsigned calls = 0;
  HRESULT backendResult = S_OK;
  UINT expectedFlags = 0;
  bool statusOnly = false;
  auto backend = [&](void* output, UINT size, UINT flags) -> HRESULT {
    calls++;
    CHECK(flags == expectedFlags);
    CHECK(statusOnly ? (!output && !size) : (output && size == sizeof(UINT64)));
    if (output) {
      CHECK(reinterpret_cast<uintptr_t>(output) % alignof(UINT64) == 0);
      // Deliberately write on failure too: the DDI contract requires caller
      // memory to remain untouched until the query has actually completed.
      std::memcpy(output, &expected, sizeof(expected));
    }
    return backendResult;
  };
  CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == S_OK && data == expected);
  for (HRESULT pending : {S_FALSE, DXGI_ERROR_WAS_STILL_DRAWING}) {
    backendResult = pending; data = original;
    CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == DXGI_DDI_ERR_WASSTILLDRAWING);
    CHECK(data == original);
  }
  for (HRESULT failure : {DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_DEVICE_RESET,
      DXGI_ERROR_DEVICE_HUNG, DXGI_ERROR_DRIVER_INTERNAL_ERROR}) {
    backendResult = failure; data = original;
    CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == D3DDDIERR_DEVICEREMOVED);
    CHECK(data == original);
  }
  backendResult = E_OUTOFMEMORY; data = original;
  CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == E_OUTOFMEMORY && data == original);
  backendResult = 2;
  CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == E_FAIL && data == original);
  backendResult = S_FALSE; expectedFlags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
  CHECK(readQueryData(info, &data, sizeof(data), D3D10_DDI_GET_DATA_DO_NOT_FLUSH, backend) == DXGI_DDI_ERR_WASSTILLDRAWING);
  CHECK(data == original);
  statusOnly = true; backendResult = S_OK;
  CHECK(readQueryData(info, nullptr, 0, D3D10_DDI_GET_DATA_DO_NOT_FLUSH, backend) == S_OK);
  backendResult = S_FALSE;
  CHECK(readQueryData(info, nullptr, 0, D3D10_DDI_GET_DATA_DO_NOT_FLUSH, backend) == DXGI_DDI_ERR_WASSTILLDRAWING);
  const unsigned beforeInvalid = calls;
  CHECK(readQueryData(info, nullptr, sizeof(data), 0, backend) == E_INVALIDARG);
  CHECK(readQueryData(info, &data, 0, 0, backend) == E_INVALIDARG);
  CHECK(readQueryData(info, &data, sizeof(data)+1, 0, backend) == E_INVALIDARG);
  CHECK(readQueryData(info, &data, sizeof(data)-1, 0, backend) == E_INVALIDARG);
  CHECK(readQueryData(info, &data, sizeof(data), 0x80000000, backend) == E_INVALIDARG);
  info.size = 0;
  CHECK(readQueryData(info, nullptr, 0, 0, backend) == E_INVALIDARG);
  info.size = 1024;
  CHECK(readQueryData(info, &data, sizeof(data), 0, backend) == E_INVALIDARG);
  CHECK(calls == beforeInvalid && data == original);
  unsigned mapCalls = 0;
  unsigned char mappedByte = 0;
  HRESULT mapResult = S_OK;
  D3D10DDI_MAPPED_SUBRESOURCE mapping = {};
  auto mapBackend = [&](D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE* mapped) {
    mapCalls++;
    CHECK(type == D3D11_MAP_READ && flags == D3D11_MAP_FLAG_DO_NOT_WAIT);
    // Failed calls may leave internal scratch dirty. None of it is allowed
    // to become a caller-visible mapping or trigger a later native Unmap.
    *mapped = {&mappedByte,128,4096};
    return mapResult;
  };
  CHECK(mapSubresource(D3D10_DDI_MAP_READ, D3D10_DDI_MAP_FLAG_DONOTWAIT, &mapping, mapBackend) == S_OK);
  CHECK(mapping.pData == &mappedByte && mapping.RowPitch == 128 && mapping.DepthPitch == 4096);
  for (const auto& result : {std::array<HRESULT,2>{DXGI_ERROR_WAS_STILL_DRAWING,DXGI_DDI_ERR_WASSTILLDRAWING},
      {DXGI_ERROR_DEVICE_REMOVED,D3DDDIERR_DEVICEREMOVED}, {DXGI_ERROR_DEVICE_RESET,D3DDDIERR_DEVICEREMOVED},
      {DXGI_ERROR_DEVICE_HUNG,D3DDDIERR_DEVICEREMOVED}, {DXGI_ERROR_DRIVER_INTERNAL_ERROR,D3DDDIERR_DEVICEREMOVED},
      {E_OUTOFMEMORY,E_OUTOFMEMORY}, {S_FALSE,E_FAIL}}) {
    mapResult = result[0]; mapping = {&mappedByte,16,32};
    CHECK(mapSubresource(D3D10_DDI_MAP_READ, D3D10_DDI_MAP_FLAG_DONOTWAIT, &mapping, mapBackend) == result[1]);
    CHECK(!mapping.pData && !mapping.RowPitch && !mapping.DepthPitch);
  }
  const unsigned beforeBadMap = mapCalls;
  CHECK(mapSubresource(static_cast<D3D10_DDI_MAP>(0), 0, &mapping, mapBackend) == E_INVALIDARG);
  CHECK(mapSubresource(static_cast<D3D10_DDI_MAP>(6), 0, &mapping, mapBackend) == E_INVALIDARG);
  CHECK(mapSubresource(D3D10_DDI_MAP_READ, 0x80000000, &mapping, mapBackend) == E_INVALIDARG);
  CHECK(mapSubresource(D3D10_DDI_MAP_READ, 0, nullptr, mapBackend) == E_INVALIDARG);
  CHECK(mapCalls == beforeBadMap);
  CHECK(mapSubresource(D3D10_DDI_MAP_READ, 0, &mapping,
    [](D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*) { return S_OK; }) == E_FAIL);
  CHECK(!mapping.pData && !mapping.RowPitch && !mapping.DepthPitch);
  std::printf("query completion PASS checks=%u; mock backend, no GPU\n", checks);
}
