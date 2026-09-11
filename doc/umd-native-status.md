# Native VIOGPU DXVK UMD development status

The Windows native UMD is still a development subset. `viogpudxvk.dll` has no
OpenAdapter export and is not selected by the paired driver's INF. The INF
continues to select the existing Mesa UMD. The long-term goal remains native
D3D9/10/11 Display+Render through DXVK, not application-local replacement DLLs.

## Implemented path

`src/umd` embeds DXVK's D3D11 frontend without `d3d11_main.cpp` or public D3D
creation imports. It requires a nonzero exact Windows adapter LUID and exactly
one Vulkan physical device with that valid LUID and the Mesa Turnip driver ID.
The private D3D11 object is constructed directly; no DXGI factory is used to
select an adapter and no first-adapter/software fallback exists.

The development D3D10 DDI table currently includes resource creation/destruction
for buffers and Texture2D, RTV clear/copy, staging and resource Map/Unmap, Flush,
restricted VS/PS creation/binding/destruction, rasterizer state, one viewport,
one RGBA8 render target, triangle-list topology and Draw. Driver-private memory
belongs to the caller; objects retain an owning-device pointer and backend COM
references. DDI CPU access and DO_NOT_WAIT bits are translated explicitly to
the different D3D11 API bit values.

Dynamic IA/constant-buffer/resource Map DDIs share the checked map path.
Busy and device-loss errors are translated to native DDI codes; failed maps
leave output pointers and pitches zero. The GPU probe uploads its positions
through a dynamic vertex buffer's MapDiscard/Unmap pair. CPU tests cover
partially written backend failure results and the distinct API/DDI flag values.

ResourceUpdateSubresourceUP and ResourceCopyRegion support buffers and
single-sample RGBA8/BGRA8 Texture2D subresources, with mip/array/box bounds,
usage and pitch checks. Empty boxes are no-ops. The device probe verifies an
offset buffer update/copy byte-for-byte before drawing; checkpoint272a067
passed that check on the target.

VS/PS constant-buffer binding supports owned buffers, explicit null unbinding
and bounded slot ranges. The pixel probe now obtains its red color from a real
constant buffer rather than a shader literal; Windows CPU CI still independently
compiles and reflects the reconstructed shader container.

Single-sample RGBA8/BGRA8 Texture2D shader-resource views and VS/PS sampler
bindings now route to the embedded DXVK objects, with ownership, slot and
subresource validation. The pixel probe samples an immutable white texture
and multiplies it by the constant-buffer color, so its pixel result requires
both the descriptor/sampler path and the constant-buffer path to work.

D3D10.0 blend-state conversion follows DXVK's existing per-target enable/mask
mapping. Index-buffer binding and indexed/instanced draw variants now call the
embedded context. The device probe uses an index buffer and additive blending:
a half-red destination plus sampled half-red shader output must become full
red. Skipping the draw, blend, texture or constant-buffer operation fails pixels.
Scissor rectangles also route to the backend with explicit slot clearing;
the pixel probe enables scissoring and sets its full-target rectangle.

Depth/stencil state and Texture2D views now translate to DXVK, including
array/MSAA view descriptors and the native DDI's separate front/back stencil
enables. Render targets and depth bind atomically even when all color slots
are cleared. The device probe uses a D24S8 attachment and requires occlusion
counts of 0, 4096 and 0 for depth rejection, a passing draw and stencil
rejection. These checks prevent additive color saturation from hiding an
incorrect extra draw. Checkpoint272a067 passed these target depth/stencil checks.

Event, occlusion, timestamp and timestamp-disjoint query DDIs now use the
embedded backend's actual query objects. Pending results become the DDI busy
status, device loss is translated to the DDI removed status, and output is
copied only on completion. Predicate/statistics queries are still rejected.
The completion helper has CPU failure-path tests; the pixel probe additionally
requires a completed GPU event. Checkpoint272a067 passed the event/occlusion
checks on target; timestamps still need dedicated hardware validation.

The shader interface supports SM4.0 VS with optional SV_VertexID, generic
vertex inputs and SV_Position output, plus generic VS/PS varyings and PS
SV_Position input with one float SV_Target0 output. Pixel input declarations
select F32 for interpolated values and a bit-preserving U32 interface for flat
values. The active pixel shader determines the matching vertex output variant;
unmatched registers/components fail before drawing. Native register-index
semantics preserve linkage without guessing application names. Original shader
tokens remain byte-for-byte intact with the proper DXBC checksum. Packed
mixed-type registers, other system values/stages, custom data and additional
render targets are still rejected. The linkage extension is newer than the
target-tested272a067 checkpoint and needs its own CI and device validation.

Native input-layout and vertex-buffer DDIs now map numeric input registers to
the same generated semantics on both sides of DXVK's API boundary. The initial
formats are one through four 32-bit float/unsigned/signed components. A generic
VS retains its original tokens until a bound layout supplies the real input
scalar types; only then is the executable DXVK shader compiled and cached.
No provisional type reaches the shader compiler. Missing layout registers
fail before drawing. The device probe now reads fullscreen positions from
an actual vertex buffer, and CPU tests check nonzero registers and all three
scalar types plus reflection of a real compiled vertex-input shader.

## Validation and evidence limits

`scripts/test-native-umd.sh` exercises adapter matching, optional identity
trailer decoding and malformed shader containers under ASan/UBSan on Linux.
The Windows CPU test uses real D3DCompile output and checks the reconstructed
containers with Microsoft's D3DReflect. Windows CI builds ARM64, x64 and x86
targets and checks PE architecture, exports and absence of public D3D creation
imports. These checks require no GPU and establish no hardware rendering claim.

`VioGpuDxvkOpenAdapterForTest` now wires the real WDK OpenAdapter, private
device-size, CreateDevice and CloseAdapter signatures into the development
table. It queries the original runtime handle for the exact LUID, accepts
only the D3D10.0 table and compatible runtime builds, and rejects unsupported
flags rather than silently ignoring threading restrictions. Devices retain
adapter identity after the adapter handle is closed. The CPU lifecycle test
uses mocked runtime/backend callbacks, including failure paths; this is not
Windows runtime activation. No OpenAdapter10 export is supplied.

The development adapter now optionally supplies a windowed-blit DXGI Present
entry when the real allocation/context/Present callbacks are provided. Single
RGBA8/BGRA8 present-source textures own runtime-associated CPU-visible kernel
allocations. Present synchronously maps a GPU readback, locks and fills that
allocation, unlocks it, and calls PresentCb on a callback-created context with
the original opaque DXGI context. An incomplete allocation or failed copy
cannot reach PresentCb. This correctness path copies pixels; it is not zero
copy. Checkpoint272a067 has real target proof of backing publication through
the harness callback; actual display Present remains unproven. Shared opens, primaries, flips and
explicit destinations remain rejected; the rest of the DXGI table is pending.

Two device-only executables are packaged for coordinated testing:

- `dxvk-umd-backend-probe.exe <16 hex digits>` creates the private backend,
  clears magenta, copies and verifies 4096 pixels.
- `dxvk-umd-ddi-probe.exe <16 hex digits>` uses real WDK DDI function pointers,
  clears half-red, blends an indexed full-screen triangle and verifies all
  4096 final red pixels plus the depth/stencil occlusion counts above.
  Optional `--adapter` opens that exact LUID through KMT, reads the real KMD
  private reply and creates the device through the new adapter harness. Its
  runtime callbacks are supplied by the probe; this is still not Microsoft
  D3D runtime activation. Old KMDs without the identity trailer fail closed.
  `--native-copy` additionally exercises the production Present DDI through
  harness callbacks backed by actual KMT device/allocation/context/Lock
  operations. Its final callback checks all 4096 pixels in the real KMD
  allocation. It does not call screen Present or activate the Microsoft
  runtime; the output explicitly labels this allocation-publication evidence.

The argument encodes the eight LUID bytes in memory order; it is not an adapter
index or a printed 64-bit integer. Both executables at272a067 passed on target:
the backend verified4096magenta pixels, and the DDI/native-copy probe verified
the64-byte copy, occlusion0/4096/0,4096red pixels/event1 and4096pixels in
the actual KMT allocation backing, all with zero mismatches and exit0.
The matched58386 KMD and process-local56bd30c Turnip used LUID2A58000000000000;
existing DWM/Explorer processes remained live with no new appfault/timeout.
The CPU lifecycle tests use mocks; the target probe uses real KMT-backed
callbacks supplied by the harness, so Microsoft runtime activation and
native application/display acceptance remain separate unfinished requirements.
The coordinated KMD producer has separately passed ARM64 WDK compilation
and 377 production-reply/real-decoder CPU checks; see the identity contract
document for commits. This does not establish installed-device validation.

## Required follow-up

1. Complete paired package and device validation of the KMD LUID producer in
   `umd-identity-proposal.md`, including real stop/restart/reset transitions.
2. Expand shaders, resources, state, queries, hazards and remaining mandatory
   D3D10/10.1/11 DDIs before publishing any runtime callback table or caps.
3. Bind backend allocations to Windows kernel allocation/resource ownership,
   shared handles, residency, fences and device loss. An internally created
   Vulkan image is not automatically a runtime-owned primary.
4. Implement DXGI Present and scanout ownership with the existing display KMD,
   then test real native runtime activation and visible accelerated UI.
5. Build a distinct D3D9 DDI bridge. The existing DXVK D3D9 factory enumerates
   displays with adapter-index fallback and constructs an implicit swapchain;
   it must not be called unchanged from a native UMD. Reuse the D3D9 rendering
   core with exact adapter binding and runtime-owned presentation instead.
6. Validate D3D9/10/11 workloads for ARM64, x64 and x86, including the requested
   TestD3D and stress applications, after real runtime/display integration.
