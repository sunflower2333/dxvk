# Native D3D11 DDI port

## Goal
Implement a typed SDK D3D11 UMD device/DDI backed by the private DXVK core; coordinate the DX10 adapter interfaces. Keep normal runtime acceptance gated until genuine hardware/presentation is validated.

## Phases
- [complete] Audit current device implementation and official D3D11 SDK contracts.
- [complete] Implement typed D3D10.1 and D3D11 tables with lifetime and error handling.
- [complete] Add meaningful interface/DDI tests and run available local checks.
- [complete] Commit isolated reviewable changes with exact limitations for root integration.
- [complete] Reconstruct SM5 graphics/control-point/patch signatures and dynamic interface metadata from raw native tokens.
- [complete] Connect typed graphics/GS stream output/tessellation/class-interface operations to the existing DXVK core and add meaningful fixtures.
- [complete] Freeze next reviewable slice; target verification remains root/DX10-owned.

## Constraints
No app-local runtime substitution, WARP hardware claims, fake S_OK, interface table casts, advertised blanket-unsupported versions, target commands without root coordination, CI pushes, image/VM changes. Ownership: umd_ddi.cpp/.h and dedicated D3D11 implementation/tests/docs.

## Errors
Local clang18 ARM64 syntax check is blocked by a compiler/official VC44 intrinsics mismatch; x64 official-header compilation passes. Native ARM64 MSVC fixture execution remains root-coordinated, not claimed complete.
