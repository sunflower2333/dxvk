# Native VIOGPU DXVK UMD development status

The Windows native UMD is still a development subset. `viogpudxvk.dll` now
exports typed OpenAdapter10/OpenAdapter10_2, but its incomplete interfaces and
feature levels remain unadvertised and it is not selected by the paired INF. The INF
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
restricted VS/GS/PS creation/binding/destruction, rasterizer state, up to16viewports,
one RGBA8 render target, D3D10 topology state and Draw. Driver-private memory
belongs to the caller; objects retain an owning-device pointer and backend COM
references. DDI CPU access and DO_NOT_WAIT bits are translated explicitly to
the different D3D11 API bit values.

Runtime initialization now supplies default constant-buffer updates, table
relocation and accurate zero-performance-counter capability responses, validated
by cf492c9 CI34613950771 ALL5PASS. The topology setter accepts UNDEFINED as a
legal reset and all D3D10 primitive types. Viewports replace the entire binding
atomically; native null slots retain their indices as zero-area API viewports.
Zero count unbinds viewport/scissor state even when the clear hint is zero.
Source ef4b174 CI34615056755 passes all five jobs. Later GS work is described
below; stream output remains unfinished.

GenMips is wired at source c9e389d through native auto-mip resource flags and
DXVK's actual GPU mip blits. Ownership, creation flags, MIP range/type and format
support are checked before dispatch. Production compilation passes. Independent
API-only controls at 68d6bb4, CI34617903693, localize the Windows runner's WARP
failure to views starting at array slice1: its lower mip stays green, while
default views and slice0 produce the expected red. The debug layer is active
with zero messages. The controls bypass every native descriptor helper.
The strict CPU oracle now generates slice0/mips0-1 and checks all eight
subresources, including untouched neighboring slices and out-of-view mips.
6dfb092 CI34619152294 passes all five jobs, including the strict 488-check
WARP test on x86/x64 and ARM64 compilation. Existing typeless clear/sample tests still use
slice1. Production is unchanged; DXVK mip generation on nonzero array slices
and ordinary runtime activation remain unvalidated on the target.

Source 7c0b9e3 adds native GS creation/binding and constant
buffer, SRV and sampler slots. It transports generic VS->GS inputs as raw32,
keeps position F32, and compiles GS outputs using the active PS interface.
Changing or removing GS relinks the VS to its actual next stage. Unused entries
in the runtime's union input signature no longer cause false PS/GS rejection.
The new WARP fixture compares original and rebuilt VS->GS->PS payloads with a
GS constant-buffer XOR and texture sample; SPIR-V checks include per-vertex
GS input arrays. CI34620365858 passes all five jobs: ARM64/x64/x86 production
builds, x64/x86 WARP and 2,257 shader checks, plus Linux memory checks.
The actual GS native DDI has not run on the target. Stream output and additional
shader system values remain unfinished, and this does not enable registration.

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
mixed-type registers, other system values/stages and additional
render targets are still rejected. Final SPIR-V checks verify scalar types,
locations and flat decorations; they do not establish correct GPU values.

Immediate constant-buffer blocks and inert comment/debug blocks are accepted
with their distinct second-dword length encoding and unchanged payloads.
Duplicate, empty, truncated or non-vec4-aligned immediate buffers and unknown
custom-data classes fail before compilation. The linked GPU probe also reads
its flat integer payload from a dynamically indexed immediate lookup table.

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

The9c55e92 linkage probe failed all4096 final target pixels while creation,
copy, depth/stencil and event completion passed. Independent Microsoft WARP
execution reproduced one invalid test expectation: FXC emitted positive zero
for the source's negative-zero float constant, including when it was selected
from an immediate array containing only zero/denormal float patterns. The
replacement probe obtains that bit pattern from runtime VS constants, retains
the exact NaN/signed-zero/infinity and integer checks, and verifies original
and rebuilt containers against WARP. Both now match all16 diagnostic words;
the fast shader fixture passes887checks. The replacement target run
seven-dxvkb8-ddi-03 passed in249ms:4096correct pixels,64-byte buffer copy,
depth/stencil and real KMT allocation publication, with original DWM2088 and
Explorer5820 retained. The host trace started after this DXVK run finished,
so no correlated GPU-trace claim is made for it. A failed image prints separate UV, raw-float, immediate-array and
fixed-integer masks, then one bounded diagnostic draw returns their actual
payload words. WARP/interface agreement and standalone target DDI proof remain
distinct from Microsoft runtime activation and visible Present acceptance.

The next native resource step translates 2D shader-resource and render-target
views with nonzero mip/array ranges, typed views of typeless textures, and
multisampled arrays. The previous SRV restriction to single RGBA8/BGRA8
textures is removed; the embedded device validates format compatibility.
MSAA render targets now select the correct native view dimension. Native
CheckFormatSupport maps API bits explicitly, CheckMultisampleQualityLevels
queries the selected backend, and ResourceResolveSubresource validates typed
source/destination ownership, sample counts, subresource bounds and equal
dimensions before queuing a supported resolve. Typeless resolves remain
rejected pending a format-family compatibility implementation.

Independent Microsoft WARP tests exercise translated views by clearing only
slice1/mip1 of a typeless texture array, sampling it using relative coordinates,
and resolving a selected MSAA slice. The next standalone native probe adds a
4x MSAA clear/resolve from source slice1 to destination slice1/mip1; all640
pixels, including untouched neighboring subresources, must match. Source
1a7cf3b passed all five CI34608704564 jobs, including246independent WARP
view/sampling/MSAA checks. Parent targetseven-dxvk1a-resources-04 passed in259ms:
640MSAApixels and4096drawpixels correct, with copy/depth/stencil/KMTpublication
also passing. LUID5805/ICD9AA5/KMD58386; DWM1036/Explorer5640 retained. The
host trace began before execution; its final analysis belongs to the parent.
This remains standalone native-DDI evidence, not Microsoft runtime activation.

The next native synchronization step implements ResourceIsStagingBusy using
the embedded context's command sequence and GPU access tracker. It covers all
staging subresources without changing Map state and treats pending CS chunks
as busy. Untracked resources synchronize only CPU command processing. Native
buffer/SRV read-after-write notifications validate resource ownership and view
correspondence; DXVK inserts Vulkan barriers at actual use, with no forced GPU
idle. The target probe checks a mapped buffer and four simultaneously mapped
texture subresources, plus clear-to-sample and update-to-index-buffer hazards.
Checkpoint5b0983d passed all five CI34612034025 jobs across ARM64/x64/x86.
Its exact matched ARM64 DLL/probe are ready; target execution remains pending.

`VioGpuDxvkOpenAdapterForTest` now wires the real WDK OpenAdapter, private
device-size, CreateDevice and CloseAdapter signatures into the development
table. It queries the original runtime handle for the exact LUID, accepts
only the D3D10.0 table and compatible runtime builds, and rejects unsupported
flags rather than silently ignoring threading restrictions. Devices retain
adapter identity after the adapter handle is closed. The CPU lifecycle test
uses mocked runtime/backend callbacks, including failure paths; this is not
Windows runtime activation. The later native entry implementation and strict
production capability gate are documented in [native-runtime-entry.md](native-runtime-entry.md).

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

The current native adapter/version/table and ARM64/x64/x86 registration gates
are detailed in [runtime-activation-gaps.md](runtime-activation-gaps.md). The
parent's independent f6da604 ordinary-runtime probe, using the actual system
D3D/DXGI modules and exact loaded UMD/hash, is the next activation acceptance
path. Separate architecture builds cannot prove native or emulated UMD loading.

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
