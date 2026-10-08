# Progress

2026-10-08: Read pinned root plan and planning skill, created independent
worktree at actual 488b484. Inspected typed DX11 table, OM/CS UAV handlers,
compute/graphics readiness, resource-copy handlers and local Microsoft docs.
Chose same-family bit-copy semantics; ROOT notified before editing production.
No target calls or native executions.

Actual first optimized official-SDK production x64/x86 compilation passed with
empty diagnostics (PIDs3117636/3117635). Typed fixture compilation found two
source errors (deduced call definition order and incorrect guessed native usage
enum name); both originals preserved. Fix uses actual SDK
D3D10_DDI_RESOURCE_USAGE and defines dispatch template before constructor.

Final production/typed fixture x64/x86 compilation02 passed all four actual
children with empty diagnostics. Portable policy GCC compile passed; first
Clang link failed because resolving clang++ symlink changed argv0 to clang
and omitted C++ libraries. Correction preserves clang++ driver name; GCC
successful original binary reused without recompilation. All originals retained.

Strict compile02 production/native fixture four children plus policy COFF two
children all passed O2/Wall/Wextra/Werror with zero diagnostics; six LLVM
reopens passed actual machine/directive checks. ROOT-style owned verifier
3129791 exited0/reaped with regular raw files closed, reopened receipts and
current selected dependency pins (current scope, not full historical toolchain).
Portable GCC/Clang ASan+UBSan each 74036 checks/36864 pairs pass.
Synthetic reader control3123526 exited0/reaped, one640-file positive and12
mutation failures passed; explicitly no Windows or GPU execution. Docs and
Meson targets prepared. All production admission/discovery files unchanged.
