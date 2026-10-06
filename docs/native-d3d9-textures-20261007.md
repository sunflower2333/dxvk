# Native D3D9 texture development, 2026-10-07

The closed-admission development device adds coherent static2D A8R8G8B8 and
X8R8G8B8 mip chains. One worker-owned DXVK texture supplies the indexed mip
surfaces; resource metadata, runtime handle reservation and fresh identity
checks precede publication. Partial creation releases surfaces before their
parent. Cube/volume, compressed formats, dynamic/autogen and sharing remain
outside this slice.

Pixel sampler slots0–15 and vertex slots257–260 use device/type-safe resource
tokens. Deletion unbinds every slot and any render target, flushes, then retires
the mip surfaces and parent on the worker. Retryable failures retain ownership;
device destruction drains remaining bindings and locks. Static video textures
cannot be locked through the public renderer path; system levels preserve
caller-owned backing, pitches and NotifyOnly locks.

Native texture-stage state explicitly maps fixed-function D3DTSS and D3DSAMP
values. Fixed-function stages remain0–7. Native colorkey states and TEXTUREMAP
are rejected rather than interpreted as public enum values. The displacement
sampler remains outside this slice.

TexBlt matches the destination's top dimensions to a corresponding source mip
and copies every common level. It validates the complete copy before starting
callbacks and snapshots cropped caller-owned system pixels with their original
pitch. Individual-level Blt remains available. Creation metadata is prepared
after device serialization, so reentrant callbacks cannot read an in-progress
surface list.

The native controlled fixture covers caller mutation, malformed chains,
partial creation, null/S_FALSE replies, common mip copying, padding, state
mapping, stale/foreign handles, reset/reentry and retryable unbind/flush.
Its first source archive transferred to the existing VM, but Windows SSH
stopped responding before any build result returned. Native fixture execution
and architecture CI are pending; no texture acceptance is claimed.

The `--texture` GPU oracle retains the previous576 clear/draw/SM1–3 pixels and
adds512 pixels over eight sampling cases. It checks three source levels, a
smaller two-level destination, point/linear filtering, border/clamp/wrap and
unchanged caller padding. Expected texture checksum is `d3afb9c5`; total1088
pixels. This target run is pending. Production exports/caps and ordinary
DX8–DX11 runtime admission remain closed.

Use the local Microsoft DDI references under workspace
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/`:
`d3dumddi/nc-d3dumddi-pfnd3dddi_createresource.md`,
`d3dumddi/nc-d3dumddi-pfnd3dddi_texblt.md`,
`d3dumddi/ns-d3dumddi-_d3dddiarg_texturestagestate.md`, and
`d3dukmdt/ns-d3dukmdt-_d3dddi_surfaceinfo.md`.
The inspected documentation revision is7515063cea4c9e98db6a92986c5b4ddb0463fd16.
