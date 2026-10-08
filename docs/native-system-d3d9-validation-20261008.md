# Ordinary SYSTEM D3D9 and D3D9Ex validation

This separate application probe exercises Microsoft's SYSTEM factories with
the current production D9 frontend and core. It changes no adapter caps,
frontend exports, registry values, admission masks, Meson tests or CI selection.
ROOT owns native build, the common reversible binding controller, and target
execution. Local compilation and reader self-tests do not admit a driver tuple.

The old D9 frontend fixture expects historical caps (including the old fog
mask) and its lifecycle probe only clears a surface. Reusing that fixture would
not test current ordinary Draw or Present. The old DX8 system failure was an
INDEX16 creation rejected by a vertex-only locking hint; the current production
source already limits that hint to vertex buffers. Existing DX8 descendant
preparation and its 448 offscreen / 64 visible-pixel gate remain unchanged.
The current-core gate correctly rejects CI66's failed source/run: a failed
x86/x64 BC fixture and skipped ARM64 runtime are not the six successful job
originals required to admit a new core.

## Probe and literal images

`tests/umd-d3d9-system-validation.cpp` dynamically loads the exact SYSTEM
`d3d9.dll` and uses `Direct3DCreate9` or `Direct3DCreate9Ex`. Before creation,
it requires an interactive Limited USER token, the exact effective KMT DX9
frontend name, a render-capable non-software adapter, and the actual private
160-byte runtime identity matching the requested LUID and generation. The
public adapter LUID, vendor 0x1af4, device 0x1050, HAL creation parameters and
hardware vertex processing must agree. It checks the actual loaded frontend,
core, private Vulkan loader and ICD full paths after creation and rendering.
The binding controller must independently pin those candidate files and the
admitted current CI tuple before and after execution; module paths alone are
not a byte identity or hardware-admission claim.

Each run renders two asymmetric 16 by 16 A8R8G8B8 frames. The first uses Clear
rectangles and the second uses untextured fixed-function `DrawPrimitiveUP`
triangle strips. Rectangles use D3D9's half-pixel coordinates. All four color
channels are checked, including alpha 0xff. The public readback is a matching
single-sample render target copied to a matching SYSTEMMEM plain surface with
`GetRenderTargetData`; only meaningful bytes of each actual mapped row are
saved, before comparing against the fixed literal oracle. Microsoft's
[readback contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getrendertargetdata)
requires matching size/format and excludes multisampled sources. The probe
sets FVF before UP draws and keeps the input vertices alive through the call,
as required by the
[UP draw contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup).

`offscreen` runs save 512 literal pixels and make no Present call. `present`
runs also make exactly two ordinary Present/PresentEx calls, outside scene
brackets with null rectangles for the DISCARD swap chain, then check all 512
actual visible desktop RGB pixels from `GetPixel`. Screen planes contain
unmodified COLORREF words (0x00BBGGRR); no alpha is fabricated. Each screen
observation has a bounded two-second retry. Microsoft's
[Present contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-present)
defines these scene and rectangle restrictions. There is no format, vertex
processing, software-device or pixel-oracle fallback.

## Common controller interface

The CLI has ten operands after the executable:

```text
probe <9|9ex> <offscreen|present> <8hex-high:8hex-low-LUID>
      <absolute-frontend> <absolute-core> <absolute-private-loader> <absolute-ICD>
      <fresh-raw-directory> <Local\unique-hold-event> <1..60000-ms>
```

The candidate basenames normally are `viogpudxvk9x.dll` and the actual selected
`arm64` or `x64` core/loader/ICD siblings. Paths are explicit arguments, never
inferred from old artifacts. The probe creates a fresh named event and retains
the device, factory and loaded tuple through its bounded wait. After both
frames, it rechecks KMT name and the complete identity reply. It writes,
flushes and closes `<fresh-raw-directory>.held.json` before waiting. This
checkpoint is a sibling of the raw directory, so buffered runner stdout need
not be polled. On a render failure it records `pending_exit=1` when the output
directory was created; the controller must still restore its own snapshot.

The exact hold object is:

```json
{"schema":"ordinary-system-d3d9-held-v1","pid":1234,"timeout_ms":60000,"pending_exit":0,"hold_event":"Local\\unique","restoration_proved_by_event":false}
```

The common controller must retain the actual process object, verify its
executable/start time and publish its worker-held checkpoint. Registry restore,
original all-three effective KMT names, event release and actual child reap
remain separate owned observations. Signaling the event proves none of those
steps. The probe's four phase markers are `D9_SYSTEM_RESULTS_READY`,
`D9_SYSTEM_HELD`, `D9_SYSTEM_DEVICE_RELEASE remaining=0`, and
`D9_SYSTEM_DONE exit=0 production_admission=0 registry_changes=0`.

Raw directory closure is exactly `clear.raw`, `draw.raw`, `manifest.json` for
offscreen, plus `clear-screen.raw` and `draw-screen.raw` for present. Runner
receipts/stdout/stderr, the hold file and reader JSON must remain outside it.
`tests/verify-d3d9-system-originals.py` uses independent literal row tables to
verify every original byte, exact API/phase/LUID/tuple metadata and the closed
hold checkpoint. It can additionally check actual stdout/stderr. It explicitly
leaves process closure, registry restoration and production admission false;
the common controller and ROOT must supply those proofs.

## Build-only packet

`scripts/build-native-d3d9-system-probes.ps1` builds genuine ARM64 and x64
executables using the previously accepted copied Hostarm64 14.50.35717 tools
and SDK28000 views manifest. It rechecks all selected inputs rather than
assuming they still match. The only additional include is the same SDK's
`winrt` directory for WRL. Exact source inputs and the unchanged d8cf owned
runner are part of the prepared packet. Before object compilation, one strict
`/Zs /showIncludes` discovery per view captures every reported selected SDK/STL
header. That discovery itself is not a pre-snapshot compile claim. Actual
object compile/link/header/import stages then capture retained process/exit
and closed raw streams, with a 60-second deadline per tool. All selected
inputs are rehashed in finalization. There are ten successful build children
when both views complete. These build steps never load a candidate or run the
graphics probe.

Local checks compile the current probe to genuine ARM64, x64 and x86 COFF
with Clang18 O2 and official SDK26100 headers, with strict source warnings.
Selected inputs include actual dependency files and the explicit virtual
overlay before/after. The independent reader self-test covers four synthetic
API/phase layouts and fifteen byte/metadata/closure mutations. Native PS5.1
parse, MSVC/SDK28000 link, public SYSTEM factory execution, binding/cache
selection, real draw/readback/Present and current core admission remain pending.
