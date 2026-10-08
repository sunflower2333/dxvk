# Findings

Local Microsoft display docs require ordered adapter registration slots:
DX9, DX10, DX11. D10 runtime loads selected UMD and invokes OpenAdapter10;
D11 DDI uses OpenAdapter10_2/GetSupportedVersions/GetCaps. No process-local
registry selector or live refresh contract is documented.

Actual project findings2238+ preserve the earlier first-slot temporary
registration trial: effective KMT name remained old Mesa; frontend was never
called. Registry bytes alone are not proof of selection on this target.
No adapter reset or PnP operation belongs to this source preparation.

Production OpenAdapter10 remains closed by runtimeSupportsNativeInterface.
Private VioGpuDxvkOpenAdapterForTest bypasses only development admission and
still uses actual supplied runtime callbacks/identity/backend creation.
The separate modern private entry retains zero pipeline caps from current
production requirements. A forwarding frontend cannot resolve that D11 gate.

Modern ARM64X owner has exact857a5fe frontend EWDK recipe from CPU08 actual
native/EC build. Reuse its reviewed tools/libs/merge contract; do not duplicate
target preparation or infer capability from module-negative evidence.

Concrete probe stops before public creation if effective KMT D10/D11 name is
not the requested candidate. It uses genuine SYSTEM factory entrypoints,
positive private forwarding telemetry, actual created-device LUID and exact
frontend/core paths. Actual samples retain Map/Present/device-removal HRESULTs,
row pitches and two original literal frames. Mandatory hold event is session
local and never claims the external tuple was restored by signalling alone.

Local03: four actual O2/Werror x86/x64 compiler children and four LLVM
inspections pass. Host independent-reader synthetic controls56pass, including
pixel edge/channel corruption, exceptional exits, identity/module/capability
and closed-process failures. These are source/reader evidence only. Native
typed module-negative execution, ARM64X merge, effective driver selection,
ordinary CreateDevice/draw/readback/Present and KMD/desktop joins remain pending.
