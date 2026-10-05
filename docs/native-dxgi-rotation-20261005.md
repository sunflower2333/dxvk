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

The [DX8/DX9 audit](native-dx8-dx9-roadmap-20261005.md) records the separate
native D3D9 bridge and DX8 system-runtime proof still required for the broader
DX8-DX11 goal.
