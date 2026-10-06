# Native D3D9 programmable shaders, 2026-10-06

The development device adds owned vertex/pixel shader creation, binding and
deletion, plus float4, int4 and Boolean constant uploads for both stages.
Shader models 1–3 use the embedded DXVK translator. The private renderer entry
points carry the native DDI's byte count through analysis and compilation;
they preserve the public D3D9 factories' existing behavior.

The DDI snapshots bytecode and constant arrays on the caller before starting
the callback pump. It checks code bounds, model/stage headers, instruction and
comment framing, relative/predicate tokens and the actual final END token.
END-like immediate/comment data cannot terminate the stream. The renderer
still validates shader semantics. Constants check the existing hardware
limits: 256 vertex/224 pixel float4 registers and 16 int4/Boolean registers.
Zero-count uploads read no data. Float and integer bytes are preserved.

Device/stage ownership and unique tokens keep shaders distinct from resources,
declarations and other devices. Creation publishes only after successful
renderer creation and a fresh identity check. Deletion unbinds and flushes
before retirement; retryable failure retains ownership, and lost-device
cleanup can retire objects. Private COM release runs on the pumped worker.

The native ARM64 MSVC19.44/SDK26100 controlled fixture passes 107,385 checks
(compile 6.5373586s, execution 0.0744307s). Four independently compiled
production mutations fail at the intended checks: shader reread 2440/1044,
constant reread 3603/940, missing shader identity 9303/1084, and missing
shader flush 7451/1069. All 56 source files, archive/after-run hashes, native
executable and unchanged signed58624/desktop independently verify. The
controlled backend validates ownership and transport; it does not render.
Evidence is workspace `artifacts/dxvk-native-d3d9-shaders-20261006/`.

The new `--shader` target probe retains the 192 clear and 192 fixed-function
draw pixels, and adds 192 pixels from actual SM1.1, SM2.0 and SM3.0 programs.
SM3 float/int/Boolean constants affect both vertex placement and pixel colour.
The independent shader checksum is `1384c5a5`; padding and a nonzero
vertex-start offset remain part of the oracle. Production architecture CI and
the target programmable shader run are pending.

Production D3D9 OpenAdapter and rendering caps remain closed. Textures/buffers,
indexed/multistream/edge-flag drawing, depth/stencil, remaining legacy state,
sharing/queries, presentation/reset and ordinary DX8–DX11 system-runtime
acceptance remain required.

Contracts: [shader token format](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/shader-code-format),
[native vertex shader creation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createvertexshaderfunc),
[pixel code byte count](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_createpixelshader),
[constant register counts](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_setvertexshaderconst).
