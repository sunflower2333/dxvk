# Native DXGI buffer rotation

The DXVK UMD now supplies `DXGI_DDI_BASE_FUNCTIONS::pfnRotateResourceIdentities`.
It rotates `[A,B,C]` to `[B,C,A]`, keeping runtime resource handles stable.
This closes one missing swapchain callback; the runtime admission gate remains
closed and the DXGI base table still needs five other callbacks and primary
ownership before it can serve an ordinary Windows swapchain.

The whole chain is checked before copying or publishing: every resource must
be live on the same device, unique, an identically shaped default Texture2D,
and have the same shared/present/private allocation ownership kind. Kernel
allocation aliases and staging resources are rejected. A failed shared publish
leaves allocation identities unchanged, with dirty data available for retry.

Private and presentable textures keep their backend objects so existing views
and bindings remain valid. Their pixels rotate through a cached scratch image
on the ordered GPU context; kernel allocation identities rotate alongside them.
Shared surfaces publish dirty caches into their old allocation before rotating
kernel identities, then invalidate every cache. The next real read reloads the
new allocation; clean shared rotation needs no GPU image copies.

`tests/umd-rotation.cpp` calls the published production DXGI callback, with an
explicit test-only WARP backend and controlled allocation/lock/present callbacks.
It checks repeated three-buffer order, exact readback pixels, present allocation
handles, existing RTVs, old-backing publication, failed-publish retry and invalid
chains. It also models the runtime's rotated kernel association, verifies the
exact resource retired by DeallocateCb, rejects nested scratch reuse and retires
a resource with immediately poisoned private storage during a publish callback.
Snapshot reads and the callback-free identity commit hold the resource registry
lock; local COM references keep backend objects alive across nested retirement.
The test is built for ARM64/x64/x86 and executed on x64/x86 in Windows
CI; the ARM64 artifact is also eligible for native ARM64 runner/guest execution.
These checks establish DDI semantics, not ordinary runtime activation or Turnip
hardware acceptance.

Contract references are the selected WDK `dxgiddi.h`, Microsoft's render-only
sample `RosUmdDevice::RotateResourceIdentities`, and the existing Mesa D3D10
UMD rotation implementation in this workspace.

## Exact validation

Source `4f59adf77cf3d6207c85af85f7a39adff3d8e367` passed personal-fork
[offline CI37310474949](https://github.com/sunflower2333/dxvk/actions/runs/37310474949).
Both x64 and x86 executed the production callback: 1281 checks and 43 synchronized
locks passed. The actual ARM64 production DDI and regression compiled in the
same run. Local GCC and Clang sanitizer policy suites each passed 172822 transfer
checks and 1125 shared-state checks.

ARM64 artifact `11345428856` from that exact source executed successfully in the
existing Windows guest at 2026-10-05T12:42:20Z, exit 0, with the same 1281 checks.
The runner verified native ARM64 PE machine and executable SHA256
`9c4f107af80d7a3c4a86614cc6d37e9c65705f6da34fc198d5105ca8df0bc1f7` before execution.
DWM PID5384 and Explorer PID3704 remained present. Local workspace evidence is
under `artifacts/dxvk-native-rotation-20261005/guest-4f59adf/`; the raw guest
receipt, normalized receipt, stdout/stderr and digests are retained. Normalization
only strips PowerShell filesystem metadata attached to the stdout string.

This guest test explicitly uses Microsoft WARP. The installed driver58623,
driver registration and existing VM configuration were preserved.
[Native UMD CI37310476652](https://github.com/sunflower2333/dxvk/actions/runs/37310476652)
also passed on the exact source: full ARM64/x64/x86 embedded DXVK builds,
identity and shader checks, and native ARM64 fixture execution. The runtime
admission gate remains closed after those checks.

## Runtime callback table lifetime

The DXGI runtime owns its callback table and may replace callback addresses
between UMD entries. The UMD retains that table pointer and Present reads its
current callback slot. Kernel/core callbacks remain copied before entering the
embedded backend. Fixtures therefore keep the DXGI table alive through device
destruction while still testing kernel/core table copying.

Source `af84b74d69df73d3863bae7cf377e53af586ebb9` passed
[offline CI37335070338](https://github.com/sunflower2333/dxvk/actions/runs/37335070338):
Linux sanitizer contracts, actual x64/x86 production DDI execution, and ARM64
compilation. The allocation fixture changes PresentCb after initialization and
verifies the replacement is invoked. Rotation still passes 1281 checks and 43
synchronized locks on x64/x86.
[Full CI37335069804](https://github.com/sunflower2333/dxvk/actions/runs/37335069804)
also passed all six jobs on that exact source, including ARM64/x64/x86 embedded
backend builds and native ARM64 fixture execution. Evidence is retained under
`artifacts/dxvk-native-rotation-20261005/ci-37335070338/`.

## Present retirement and nested calls

Present now pins its allocation, readback texture and backend references outside
the runtime's resource private storage. A registry reservation identifies the
original live resource even if a callback destroys it and reuses that address.
Resource retirement cancels the outer Present, balances acquired locks and
defers allocation release until the operation unwinds. The allocator rechecks
that reservation after identity/context callbacks, before calling PresentCb.

A device guard rejects nested Present and rotation while presentation is
active. Present also rejects entry during rotation, protecting staging maps
and allocation identities across callback reentry. The shared publication
sweep pins its surface list before callbacks can change the weak registry.
The production fixtures cover immediate private-storage poisoning from LockCb,
CreateContextCb and PresentCb, no submission after early retirement, balanced
release, and nested Present/rotation rejection. CI validation of this additional
repair is recorded separately from the earlier source checkpoints above.

Source `34ff484ac87b66f77e1c9f6d8db914783b24788e` passed
[offline CI37338141939](https://github.com/sunflower2333/dxvk/actions/runs/37338141939),
including actual x64/x86 production DDI execution and ARM64 compilation.
Rotation passed 1428 checks on x64 and 1400 on x86, with 44 synchronized locks
each. The check count differs because the test asserts each byte of
architecture-sized reclaimed private storage. Full backend/native lifetime
fixture and paired signed-package validation remain separately tracked.

[Full native CI37338144307](https://github.com/sunflower2333/dxvk/actions/runs/37338144307)
also passed all six jobs on source34ff484: ARM64/x64/x86 embedded backend
builds, identity and shader checks, and native ARM64 runtime fixtures. The
paired signed-package result remains independent of that source validation.

The exact offline ARM64 rotation fixture also passed in the existing guest:
1428 checks, 44 synchronized locks, exit 0 and empty stderr. Its SHA256 is
`a03ba1df30eb753394ab248ee0d7702985101b6d1274b731a4065da4bcde8cd6`.
DWM PID1644 and Explorer PID5828 retained their start times across execution.
The compact receipt includes source identity and CI37338141939; evidence is
retained under `artifacts/dxvk-native-rotation-20261005/guest-34ff484/`.
This is WARP regression execution with runtime admission still closed.

The changed native ARM64 lifetime fixture passed in the same guest with 10834
checks, exit 0 and empty stderr. Its exact-source input was artifact11357009608
from [paired CI37338579561](https://github.com/sunflower2333/gunyah-guest-drivers-windows/actions/runs/37338579561),
with SHA256
`1a5618ccbb46c6f7c753ab1a0e572140e85242bf1839c9b7a1c359b1e9e88615`.
DWM and Explorer identities remained continuous through both fixture runs.
Captured process creation UTC labels are later than fixture wall-clock labels;
absolute guest chronology is inconsistent. Matching before/after identities
support continuity independently of that clock discrepancy.
The lifetime fixture substitutes a complete-contract policy only in its test
executable; the production DLL still exposes no incomplete native feature level.

[Paired CI37338579561](https://github.com/sunflower2333/gunyah-guest-drivers-windows/actions/runs/37338579561)
passed on driver source `b6bf4c4f849f5ca597637fc2ce591f23a826f158`, pinned to
DXVK34ff484. Driver compilation, signing, actual catalog membership, common
signer/package identity and native ARM64/EC/x86 signed runtime loading passed.
The runner repair cross-builds ARM64 driver targets on matching VS2022/x64,
while native functional and signed load checks remain mandatory ARM64 jobs.

Its version100.6.101.58522 and older Mesa sources remain a candidate baseline.
Mesa stays registered and DXVK/VKD3D stay unregistered. This package validates
integration and loadability; it was not installed over active driver58623.
Signed package evidence is retained under
`artifacts/dxvk-native-rotation-20261005/paired-ci-37338579561/`.

The separate user-requested KMD build in the Windows VM also succeeded on
exact paired sourceb6bf: native ARM64 portable MSVC/SDK/WDK,5.114s runner,
4.113s KMD stages plus0.648s VirtIO. Its unsigned SYS was verified but not
installed; active58623 and desktop identities were retained. Build setup,
source/output/PE receipts and scripts are in
`artifacts/dxvk-native-rotation-20261005/guest-kmd-b6bf4c4/`.
This provides rapid KMD compiler feedback independently of UMD runtime gates.

The [DX8/DX9 audit](native-dx8-dx9-roadmap-20261005.md) records the separate
native D3D9 bridge and DX8 system-runtime proof still required for the broader
DX8-DX11 goal.
