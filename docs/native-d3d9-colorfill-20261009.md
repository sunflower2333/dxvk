# Ordinary legacy ColorFill

The Vista typed device table previously left `pfnColorFill` null. This adds
that real DDI path for the existing DEFAULT-pool color surfaces, through the
embedded DXVK device's `ColorFill`. It fills the selected subresource and
rectangle without changing the bound render target. No capability mask,
format list, admission predicate or Microsoft-runtime selector changes.

The local Microsoft contracts are
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_colorfill.md`,
`ns-d3dumddi-_d3dddiarg_colorfill.md` and
`ns-d3dumddi-_d3dddi_colorfillflags.md`. They define a rectangle fill with an
A8R8G8B8 `D3DCOLOR`. `PresentToDwm` requests separate presentation ownership;
this slice returns `D3DERR_NOTAVAILABLE` for that bit. Unknown flags, unknown
or retired resource owners, depth, SYSTEMMEM, locked surfaces, bad subresource
indices and out-of-bounds or empty rectangles are rejected before rendering.
The actual renderer HRESULT is preserved through existing legacy error
normalization, and callback-observed identity retirement after the renderer
call returns a device-lost error.

The existing typed fixture gains BGRA A8/X8 rectangle and overlapping-fill
readbacks across literal interfaces8/9, both plain and render-target surfaces,
unchanged bound-target checks, a nonzero texture mip, error/reentry/retirement
controls and the new nonnull slot in its exact99-function table expectation.
X8 checks RGB because its high byte is unused. Its renderer remains controlled
test storage; these controls do not prove a native Vulkan fill or ordinary
SYSTEM device. Original-header x86/x64 COFFs check the actual DDI signature.

Local verification retained six successful original-SDK COFFs: four DDI and
fixture objects at `-O2 -Wall -Wextra -Werror`, and two actual renderer bridge
objects with the repository's existing renderer warning exceptions. Final
diagnostics are empty. The first two DDI compile failures remain intact; the
correction declares the lambda's `HRESULT` return type explicitly. GCC and
Clang ASan+UBSan each passed 359 controls replaying unchanged production
callback bodies against modeled storage, service and identity. Native
Windows fixture behavior and real Vulkan execution remain pending.

The unchanged ordinary SYSTEM D9/D9Ex probe739 remains a useful first gate:
windowed A8 single-sample HAL factory, Clear/FVF draw, actual SYSTEMMEM readback
and two Present calls. All required slots for that bounded source sequence
exist. Standard core and ARM64X frontend `OpenAdapter` use the same legacy
adapter implementation as the test name; no modern profile gate excludes it.
The frontend requires the exact `arm64\\viogpudxvk.dll` or
`x64\\viogpudxvk.dll` sibling, plus separately verified private loader/ICD
inputs. Actual SYSTEM initialization calls and Present flags/results must be
captured before attributing another failure to an unsupported slot.

Broader replacement remains incomplete: fullscreen/primary SetDisplayMode,
destination/flip and source-less Present, DWM ColorFill, sharing, cube/volume,
MSAA, autogen, and unsupported formats are separate legacy gaps. This change
does not claim those semantics or authorize installation. The lifecycle and
binding controller, Mesa payloads and frozen probe remain untouched.
