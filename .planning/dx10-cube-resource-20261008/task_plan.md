# Base D3D10 cube resource port

Implement the missing legacy six-face cube resource, shader-resource, render-target and depth-view
translation against original Microsoft DDI docs. Keep public feature admission
closed. Baseline trunk3d39760. ROOT owns umd_view.h and umd_ddi.cpp.

- [x] Read local original Microsoft create-resource and cube-SRV contracts.
- [x] Implement strict six-face/square/mip/sample guards and cube SRV -1 sentinel.
- [x] Add actual production DDI fixture and malformed-range/shape/atomicity controls.
- [x] Cross-compile new fixture for ARM64/x64/x86 with original WDK ABI headers.
- [ ] Compile production with native MSVC/SDK and run native reference fixture.
- [ ] Verify all six actual face/mip readbacks through production DDI.
- [ ] Integrate only after required native verification; hardware is separate.

Target calls remain forbidden while DX10 owns the slot.
