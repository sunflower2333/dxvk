# Typed D3D9 declarations and user-memory drawing, 2026-10-06

The development device adds vertex declaration create/bind/delete, supported
render states, scene capture, viewport, depth range, scissor, stream-zero user
memory and the nonindexed fast draw path. It calls the embedded DXVK D3D9
renderer through private COM objects. Production D3D9 `OpenAdapter`, positive
caps and ordinary DX8–DX11 admission remain closed.

Declarations snapshot the bounded element array before callbacks, append a
canonical terminator, check identity after renderer creation and publish a
unique driver token. Resource/device/declaration handles cannot alias by type.
Deletion unbinds and flushes before retirement; retryable failure retains the
object. Terminal release runs on the pumped renderer worker.

Drawing serializes the device and validates the bound declaration, target and
user stream before reading memory. It checks primitive/stride/address ranges
and copies the selected vertices on the DDI caller before starting the callback
pump. Later callbacks cannot change that draw's arguments or bytes. The native
UM binding survives the embedded public UP call's stream-zero reset; the next
draw snapshots the current caller bytes. Nested and concurrent DDIs return
`WASSTILLDRAWING`. Native-only legacy render commands are not silently cast
to ignored public states. Scene capture and software vertex processing use
their corresponding renderer operations.

Native ARM64 MSVC19.44/SDK26100 controlled fixture passes89,329 checks,
compile4.3458338s and execution0.07881s. Independently compiled mutations
fail at their intended assertions: reread vertices (check16172/line768), skip
declaration identity (22001/840), skip declaration flush (19651/822).
All52 source hashes, archive hashes, measured after-run files, native executable
identity and unchanged installed driver/desktop independently verify. The
controlled renderer verifies transport/ownership; it does not render on GPU.
Evidence is workspace `artifacts/dxvk-native-d3d9-draw-20261006/`.

The `--draw` target probe retains the original192-pixel clear oracle and adds
192 pixels for a full quad, scissor clipping and partial colour writes.
Expected draw checksum is `53a03d45`, computed independently from the pixel
cases. It checks a nonzero vertex-start offset, updates to already-bound user
memory, padded readback guards, actual nonempty submissions and balanced
lifetimes. Full production architecture CI and target draw proof are pending.

Textures/buffers, programmable shaders, indexed/multistream/edge-flag drawing,
depth/stencil, remaining legacy state, sharing/queries, presentation/reset and
ordinary system-runtime activation remain required for full DX8–DX11 support.

Contracts: [native DrawPrimitive](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_drawprimitive),
[user-memory stream](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_setstreamsourceum),
[vertex declaration](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createvertexshaderdecl).
