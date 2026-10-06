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
lifetimes. Exact source `c874d55af7928c1ebcce3d0a4ec6c6311580f3a5`
passes full CI37477931517 (all six jobs), offline37477937872 (all four),
API37477846431 and package37477846467. Downloaded ARM64/x64/x86 artifacts
independently verify their PE architecture, closed exports/imports,
42429 adapter/49 backend/45 rejection/89329 device checks per architecture
and 14 hashes from actual ARM64 execution.

Target runs `draw-c874d55-58624-20` and `-21` pass with Mesa diagnostics
enabled and disabled respectively. Each independently verifies all384 pixels:
192 clear pixels (`ffefa655`) and192 quad/scissor/colour-mask pixels
(`53a03d45`), intact padding, vertex-start1 and three draw stages. Six nonempty
KMT submissions succeed; context1/1, allocations10/10, locks9/9, residency10/10
and zero wrong-thread callbacks balance. Elapsed times are3.7381376s and
2.9324823s. The eight payload hashes, native ARM64 executable, three script
hashes and Limited USER/session1 token verify independently for both runs.

The exact UMD hash is
`f4061b32d91fc136ce975ed9af8a706cd6d3f3037482a86b9483010c98ce80b8`;
the same-source probe hash is
`31767d9126c95c4f7f599f85d3f69963128412ada40dbd47eda429ec6e58741a`.
Matched Mesa remains8443c71; installed signed KMD remains58624/oem17,
PnP0/binding0002 with SYS hash
`d48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a`.
Fresh readiness records retain DWM1552/Explorer4184 and show no new native
render-failure/reset/epoch/timeout/admission values. Both owned tasks are
removed. No driver installation, production registration or paired pin changes.
Evidence archive hashes are
`22b0e6fcb1a61c60e8c294bf4a1364b1288ae6201f498faf5c64402f276e375f`
and `92c86b386e833d37c120234037b9a875ed0559a7accbb48b749457d9832a913e`.

Textures/buffers, programmable shaders, indexed/multistream/edge-flag drawing,
depth/stencil, remaining legacy state, sharing/queries, presentation/reset and
ordinary system-runtime activation remain required for full DX8–DX11 support.

Contracts: [native DrawPrimitive](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_drawprimitive),
[user-memory stream](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_setstreamsourceum),
[vertex declaration](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createvertexshaderdecl).
