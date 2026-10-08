# Native DXGI primary and display boundary

This slice adds real primary allocation metadata and the typed DXGI1.0/1.1
SetDisplayMode callback for the existing DXVK-backed D3D10.0, D3D10.1 and
D3D11 device owners. It keeps ordinary Microsoft-runtime feature-level
admission closed. A bounded primary implementation does not establish a
complete DXGI or Direct3D profile.

Primary creation accepts one default, single-sample Texture2D subresource,
RENDER_TARGET and PRESENT bindings, source0 and an exact RGBA8, BGRA8 or
BGRX8 UNORM mode with nonzero refresh. Creation passes the actual runtime
resource cookie to AllocateCb with two allocations: the non-CPU-visible
primary (Flags.Primary and primary wire flag) and a synchronized CPU-visible
staging allocation. Both belong to that resource. Creation does not program
scanout. PRIMARY_OPTIONAL uses the existing copy allocation and reports the
documented NO_SCANOUT output; it cannot reach SetDisplayModeCb.

Publication copies the embedded renderer's completed pixels to staging,
submits the original KMD64-byte opcode2 command with two real allocation-list
handles, retains RenderCb's replacement buffers, and waits through a
synchronized staging lock. The primary itself is never mapped. SetDisplayMode
then forwards its actual primary allocation to the original typed runtime
callback on the DDI caller. Kernel callback fields come from the owned Vista
prefix snapshot; DXGI and core callback table ownership remains unchanged.
Callback failure, malformed replacement buffers and identity retirement are
reported. INCOMPATIBLEPRIVATEFORMAT is retained without a fake conversion or
retry. This is a staging correctness bridge, not zero-copy presentation.

The allocation ledger owns in-flight allocation and lock requests. Retirement
before acquisition has no kernel object to deallocate. Retirement after
returned kernel handles or a real successful staging mapping balances that
ownership before DestroyDevice returns. It detaches records before callbacks,
closes the staging map before DeallocateCb and follows moved allocation
owners. A resource-only destroy retires private bytes immediately and defers
the pinned physical owner's release until the outer DDI drains its retirement
queue. No returned mapped pointer is read after terminal or direct owner
retirement.

An unsuccessful UnlockCb leaves its acquired map owned. An explicit in-flight
record follows moved owners, and a later transfer or barrier must finish that
unlock before any new map or scheduled copy. Rotation rejects a mapped
participant. If Resource destruction cannot unlock, storage reserved before
acquisition retains the physical owner in the device ledger after the resource
private bytes are reclaimed; terminal cleanup can retry on the original caller.
A still-live AllocateCb that returns real handles after direct owner cancellation
also balances that late acquisition. Allocation failure or cancellation without
handles does not synthesize DeallocateCb.

DestroyDevice nested inside an unfinished UnlockCb cannot observe its result:
the official const request contains input handles, not completion status. That
case reports device removal, detaches local ownership, and issues neither a
second unlock nor a potentially mapped deallocation. A terminal unlock failure
also reports removal and omits DeallocateCb. The actual runtime must finish
those outstanding terminal allocations; no UMD callback is issued after
DestroyDevice returns. The fixture records that synthetic runtime cleanup with
separate counters and never counts it as successful UMD cleanup. This boundary
remains part of the closed ordinary-runtime admission requirement.

GetGammaCaps is an exact typed callback that returns the DDI unsupported
result without touching the output. The current adapter identity reply lacks
an actual gamma-capability query. The KMD's conditional1025-point color path
and Mesa's static17-point declaration cannot establish current hardware caps.
The DXGI base function tables have no SetGammaControl entry to invent.

Validation has two new fixtures. The portable policy fixture checks5637 shape,
flag, format and byte-arithmetic cases with independent expectations. The
typed fixture uses the production10.0/10.1/11 tables and both DXGI revisions,
with WARP only replacing the embedded renderer. It retains24 images /768
pixels /72 raw files; an independent reader checks literal bytes, metadata,
file closure and the hardware_admission=0 marker. Runtime callbacks validate
the exact primary/staging metadata and copy command, guard backing pages,
require every acquired map to be unlocked before deallocation, and exercise
pre-acquisition/after-output, first-lock/wait-lock, resource/device retirement
and moved-owner cleanup. Failed first-map/barrier unlocks, persistent failed
resource cleanup, moved failed-unlock ownership and unresolved terminal unlocks
have explicit controls. Actual native fixture execution is still pending.

The original46 ARM64 execution cases and33 shipped fixture names remain
unchanged; these two fixtures extend them to48 and35. The owned raw runner,
30-second fixture deadline, ordinary admission masks and reported caps remain
unchanged. The root integration must also retain arm64-dxgi-primary-originals
alongside its existing Resolve/Blt raw-output upload paths.

Remaining public-runtime work includes real primary/flip and rotated/display
ownership acceptance, shared/opened primary metadata, private-format
conversion, stereo/HDR scanout and a queried gamma-capability route. Renderer
Vulkan allocations still need genuine VidMm residency ownership rather than
the staging allocation's status. Full resource, shader, MRT, predication,
stream-output and runtime-threading requirements remain in the production
admission mask until their full contracts and actual ordinary-runtime/Turnip
acceptance are demonstrated.

ABI references are the original local Microsoft DDI documents
[primary descriptor](../../windows-driver-docs-ddi/wdk-ddi-src/content/dxgiddi/ns-dxgiddi-dxgi_ddi_primary_desc.md),
[SetDisplayMode arguments](../../windows-driver-docs-ddi/wdk-ddi-src/content/dxgiddi/ns-dxgiddi-dxgi_ddi_arg_setdisplaymode.md),
[kernel SetDisplayMode callback](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_setdisplaymodecb.md),
[CreateResource](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_createresource.md)
and [gamma arguments](../../windows-driver-docs-ddi/wdk-ddi-src/content/dxgiddi/ns-dxgiddi-dxgi_ddi_arg_get_gamma_control_caps.md).
The ownership follow-on uses the original [UnlockCb contract](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_unlockcb.md)
and [DestroyDevice cleanup contract](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_destroydevice.md).
The local reference sources are KMD ef8a493f03aec2d5f5fae90578f01e69e317e031
and Mesa4e62ab9ffed1fa7dd389e92330710c03601687f8; their source identities are
retained separately from any actual installed-driver/runtime proof.
The inspected KMD working copy additionally has an uncommitted MapAperture
mapping change. Its actual bytes and diff are retained separately from the
named Git source, with no claim that either represents the installed SYS.
