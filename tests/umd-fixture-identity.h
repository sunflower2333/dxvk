#pragma once
#include "../src/umd/umd_adapter_identity.h"

// CPU fixtures substitute only the kernel transport. Production links
// umd_kernel_identity.cpp, which owns a real D3DKMT adapter handle.
class FixtureIdentitySource final : public dxvk::umd::AdapterIdentitySource {
public:
  FixtureIdentitySource(HANDLE runtime, PFND3DDDI_QUERYADAPTERINFOCB callback)
  : m_runtime(runtime), m_query(callback) { }
  HRESULT query(dxvk::umd::RuntimeIdentity& result,
      dxvk::umd::RuntimeQueryStage stage) const override {
    return dxvk::umd::queryRuntimeIdentity(m_runtime, m_query, result, stage);
  }
private:
  HANDLE m_runtime;
  PFND3DDDI_QUERYADAPTERINFOCB m_query;
};
