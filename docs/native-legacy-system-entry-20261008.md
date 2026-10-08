# Normal legacy runtime entry

The UMD exports the original WDK `OpenAdapter(D3DDDIARG_OPENADAPTER*)` for
Microsoft DX8 and DX9. It shares the existing typed adapter implementation
with `VioGpuDxvkOpenAdapter9ForTest`; the public entry has no diagnostic
environment switch, substitute callback table or fallback renderer. Literal
Interface8/9 selection, original runtime handle/query ownership, exact
160-byte KMD identity, reset generation, guarded publication and the Vista
device-table prefix are unchanged. Modern OpenAdapter10/10_2 gates remain
independent and closed for incomplete interfaces.

The legacy caps advertise the implemented static/dynamic A8/X8 2D textures,
single render target, D16/D24S8 depth, fixed-function pipeline, conservative
SM2 shader limits and six query types. Cube/volume/MSAA/autogen/share,
instancing, gamma, video and stretched plain-surface operations stay outside
this profile. Device creation still constructs the embedded DXVK renderer
on the exact runtime-selected LUID and genuine kernel callbacks. Backend,
allocation, shader, draw and presentation failures retain their exact error
and cleanup rules; the public export alone cannot prove runtime success.

The driver caps also include the SDK's driver-only `D3DPMISCCAPS_FOGINFVF`
(`0x2000`, `um/d3dhal.h:2315` in SDK26100). The public
`D3DPMISCCAPS_FOGANDSPECULARALPHA` uses `0x10000`
(`shared/d3d9caps.h:326`). These are distinct bits. Existing runtime diagnostics
identified the missing driver bit during HAL validation. The renderer accepts
`D3DDECLUSAGE_FOG` separately from `COLOR1`, classifies `HasFog` and generates
a separate `eFog` fixed-function input and legacy fog state. The fix advertises
that existing implementation; it does not inject a public-cap bit into the
driver payload or claim unsupported fog behavior.

Both adapter guard executables compile the same original malformed-input,
bounded caps/output, callback mutation, reentry, close/reset and Interface8/9
tests. `dxvk-umd-d3d9-public-adapter-test` selects the real `OpenAdapter` symbol
and adds controlled factory success for both APIs, exact Vista table-tail
preservation and cleanup after close/exception callbacks occur after backend
creation. The fixture backend is deliberately a mock: execution establishes
handshake/lifetime behavior and cannot establish hardware drawing or Present.
CI requires the new public export and rejects application runtime factories.

Local validation retains original optimized I386/x64/ARM64 COFFs against the
official SDK/WDK definitions. The ARM64 local clang build uses clang's original
intrinsic header before the MSVC STL header, resolving an existing incompatible
`__prefetch` declaration without changing source, SDK types or warning flags.
Actual MSVC fixture execution, genuine Microsoft runtime creation/draw/Present,
and installed/default registration are separate pending gates. Existing cores
and their evidence remain immutable.

Deployment must use the DX9 slot of `UserModeDriverName` (also used by DX8) and
the separate WoW64 registration. The registered ARM64X entry needed by native
ARM64 and emulated x64 processes is an architecture-packaging issue; an ARM64
core must not be claimed as that dual-view entry. A staged legacy registration
must preserve the existing modern paths, record original registry kinds and
values, and support exact rollback. Neither the older app-local DXVK bundle
script nor private GPU probes provide this default system-driver registration.
