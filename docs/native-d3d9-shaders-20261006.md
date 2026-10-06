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
vertex-start offset remain part of the oracle. Source `8c4e097` full
[CI37486995183](https://github.com/sunflower2333/dxvk/actions/runs/37486995183)
passes all six jobs, and offline37486999026 passes all four. Independently
retained artifacts verify all three architectures, 107,385 device checks per
architecture and 14 actual native ARM64 execution hashes. API37486881619 and
package37486881554 also pass. Exact ARM64 artifact11424158285 is retained.

First target22 passes the previous384 clear/draw pixels, creates both SM1
shaders and submits stage7 successfully, but its first shader pixel retains
the clear colour. The original probe omitted `dcl_position v0` from the
SM1 vertex program. D3D9 requires this semantic association even in vs_1_1;
DXVK binds declaration elements through that signature. The correction adds
the missing declaration to the probe, preserving production source8c4e097,
the pixel/checksum oracle, Mesa8443 and installed58624. Probe source `b3e83c8`
passes offline37491825113 (all four jobs), API37491604110 and package37491604172.
Native ARM64 MSVC builds it in 0.7457505s; all51 source/archive/after-run hashes,
native PE/imports, six invalid CLI cases and exact KMT identity independently
verify. The probe executable SHA256 is
`ac03c39595a52f149742e29a08aa1cf77f3a7f50945001e9945f7643b716ebc7`.
Failed22 archive `24ec8d89ae202daf8405b14c203bcad6c00422d6882e8bb4b9d03f4228dcb6d0`
remains immutable. Fresh exact signed driver/PnP/SYSsha, DWM1552/Explorer2488
and58 selected failure/reset/readiness values remain unchanged through22.

Corrected target23/24 independently pass all576 pixels with Mesa diagnostics
enabled/disabled. Each returns clear `ffefa655`, draw `53a03d45` and shader
`1384c5a5`, with intact row padding. Nine nonempty KMT submissions succeed;
context1/1, allocation12/12, lock11/11, residency12/12/remaining0 and
wrong-thread0. Native probe execution takes 4.0513128s and 4.3172908s.
Eight payloads, three runner scripts, production source8c4e097/probeb3e83c8,
exact Mesa8443, native ARM64 and Limited USER/session1 independently verify.
Both owned tasks are removed with exit0. Evidence archives SHA256:
`28c9f39bd0820b1903fb1dbe2a695a06f001dd4455a37fadcb45f7bae5a9a547`
and `dbe2cc26de78c10f2f7d4ead6f1e0730d21ea308ed559ca2b6c72691700ce334`.
Fresh after each run retains signed58624/oem17/binding0002/PnP0 and the exact
SYS hash, DWM1552/Explorer2488 lifetimes and all58 selected failure/reset/epoch/
timeout/admission values. These are offscreen programmable rendering proofs;
ordinary Windows runtime admission remains closed.

Before22, Explorer had restarted. Its Application Error1000 record identifies
the installed `viogpud3d.dll`, exceptionc0000005, RVA10db80, for oldPID4184.
That fault predates the new candidate GPU run and remains a separate driver
stability issue. The actual installed binary SHA256 is
`9d72bfeef5ea7e3fb754c0c1549aeee7be144bf711df064b681711cbd3ce2fab`.
It matches signed58624's Mesa D3D10 Zink UMD, source
`eaad15fa96d476d7476d704015e7570a952ede24`, with image size11833344 and
`viogpud3d_zink.pdb` RSDS identity. It is separate from the retained fdfd legacy
shim and the uninstalled DXVK8c4e097 candidate. Fault source diagnosis still
requires that matching PDB or dump; no cause is inferred from another binary.

Production D3D9 OpenAdapter and rendering caps remain closed. Textures/buffers,
indexed/multistream/edge-flag drawing, depth/stencil, remaining legacy state,
sharing/queries, presentation/reset and ordinary DX8–DX11 system-runtime
acceptance remain required.

Contracts: [shader token format](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/shader-code-format),
[native vertex shader creation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createvertexshaderfunc),
[pixel code byte count](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_createpixelshader),
[constant register counts](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_setvertexshaderconst).
The [vs_1_1 input register contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-1-1)
requires DCL; [dcl_usage](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-usage-input-register---vs)
is supported by shader models1–3.
