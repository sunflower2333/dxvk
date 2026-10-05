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
chains. The test is built for ARM64/x64/x86 and executed on x64/x86 in Windows
CI; the ARM64 artifact is also eligible for native ARM64 runner/guest execution.
These checks establish DDI semantics, not ordinary runtime activation or Turnip
hardware acceptance.

Contract references are the selected WDK `dxgiddi.h`, Microsoft's render-only
sample `RosUmdDevice::RotateResourceIdentities`, and the existing Mesa D3D10
UMD rotation implementation in this workspace.
