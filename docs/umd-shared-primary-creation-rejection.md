# Shared creation rejection HRESULT

CI60 source `fae491c360a47fee15f45b681cfe96c19496faf0` used one `DXGI_ERROR_UNSUPPORTED` expectation for six shared-resource rejection descriptors. The actual x86 log reports callback `0x887b0002` at the strict rejection check, without identifying a case. The log alone cannot establish which descriptor ran.

The source identifies an exact mismatch: case 5 supplies `pPrimaryDesc` while retaining `MISC_SHARED`. `createResourceData` invokes `primaryResourcePlan` before generic present/shared validation. That helper explicitly rejects nonzero `MiscFlags` with `DXGI_DDI_ERR_UNSUPPORTED` (`0x887b0002`). The first five descriptors have no primary descriptor and reach the generic present/shared rejection status `DXGI_ERROR_UNSUPPORTED` (`0x887a0004`). This is a source-grounded branch classification, not an added native observation.

Local Microsoft D10/D11 `CreateResource` documentation permits `DXGI_DDI_ERR_UNSUPPORTED` through `SetErrorCb`; the DXGI base-functions page describes that code for primary-surface fallback when `BIND_PRESENT` and nonnull `pPrimaryDesc` are supplied. The source's exact primary error remains unchanged. The fixture now requires that error only for case 5, keeps the first five exact statuses, and emits profile/case/descriptor/status/allocation diagnostics on failure. It does not accept either HRESULT interchangeably.

The no-acquisition, zero-internal-allocation and 128-byte unchanged-storage checks remain exact. All success pixels, fixture counts, raw-reader bytes and hardware gates are unchanged. Strict local cross compilation proves the typed fixture builds; actual native CI is still required to execute the corrected rejection and reach the final marker.

Local contract files are under `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/`: `d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_createresource.md`, `d3d10umddi/nc-d3d10umddi-pfnd3d11ddi_createresource.md`, and `dxgiddi/ns-dxgiddi-dxgi_ddi_base_functions.md`. SDK `winerror.h` defines the two distinct HRESULT values.
