# Selected frontend lifetime

The actual current offscreen attempt opened the frontend successfully while
Microsoft D3D8 constructed its internal caps device. Its backend, allocations,
locks and context then closed successfully. Public `CreateDevice` subsequently
made seventeen `OpenAdapter` attempts, from runtime version69632 through8195,
with no second core pin or `OPEN_END`, and returned8876086a. No pixels or
Present were executed. The original attempt remains a failure.

The frontend's `Core` owns a loader reference for the process lifetime and
rejects an already loaded core during first construction. The selector only
changes the exact registered filename; it held no frontend loader reference.
If the runtime unloads the frontend after its internal caps device and reloads
it for public creation, the newly initialized `Core` will reject the still
loaded original core. This explains the observed control flow, but the old
trace does not directly record that failure location. `Version` is an opaque
runtime build value in Microsoft's original local
`D3DDDIARG_OPENADAPTER` documentation; this is not evidence of a version8195
contract rejection.

The selected probe now holds its own reference to the exact independently
byte-pinned I386 frontend from before `Direct3DCreate8` until all runtime
objects have closed. It checks the canonical owned path, actual loaded path,
I386 machine and `OpenAdapter` export, and rejects preloaded frontends. Its
reference remains separate from references owned by the Microsoft runtime.
Success explicitly releases it after selector and environment restoration;
exception cleanup releases it after the later-declared runtime COM objects.
The frontend preserves its arbitrary preloaded-core rejection and now records
the exact construction failure stage and HRESULT, plus the corresponding
adapter-open rejection. These diagnostics must be read from a fresh actual
attempt before claiming the precise native cause or working public rendering.

No capability, interface, permission, filename selector, core provenance or
callback contract is changed. The new source requires a fresh native build
and separately reviewed runtime evidence; old37b binaries, HAL acceptance and
the failed offscreen attempt retain their original identities.
