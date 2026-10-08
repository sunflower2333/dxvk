# Findings

Production missing-requirement masks are intentionally nonzero. Generic modern ForTest advertises three DDI versions but pipeline caps remain zero. This prevents ordinary SYSTEM D3D11 from reaching the existing typed device path, so its required ordinary-runtime proof cannot be collected through forwarding alone.

The new dedicated entry publishes only D3D11_0_DDI_SUPPORTED and FL10.0 pipeline bit1, with optional threading/shader bits zero. It leaves generic ForTest and production policy untouched. CreateDevice keeps production-strength kernel/DXGI callbacks, live core/DXGI table pointers, current 160-byte identity checks, selected DXGI revision, actual backend factory, and atomic output publication.

Local official references: d3d10umddi.h (WDK26100), GetCaps/GetSupportedVersions/3DPIPELINESUPPORT_CAPS/3DPIPELINELEVEL Microsoft DDI documentation. Runtime DXGI revision is independently selected by IS_DXGI1_1_BASE_FUNCTIONS.

The embedded renderer promotes its implementation minimum to FL11.0 even for logical FL10.0. ROOT must bind a genuine current selected-LUID Turnip backend supporting that private renderer requirement, and separately collect real typed11 baseline hardware and ordinary SYSTEM factory/pixel/Present/lifetime evidence. The validation tag is not a production or whole-profile admission.
