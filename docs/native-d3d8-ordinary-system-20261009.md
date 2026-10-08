# Ordinary Microsoft SYSTEM D3D8 preparation

The existing `--front-offscreen` and `--front-present` modes use Microsoft's
genuine x86 D3D8 runtime, but select a diagnostic UMD by replacing that runtime's
KMT filename-query IAT result. They are controlled runtime evidence, not ordinary
installed WoW driver selection. They remain unchanged. The older `--offscreen`
mode also remains unchanged: it has six scenes/384 pixels and cannot establish
this new exact payload, current LUID or Present requirement.

The new `--system-offscreen` and `--system-present` modes never install that
selector, enable its permission, load an application D3D8 DLL, or preload a GPU
module. They require an actual x86 KMT legacy filename equal to the unchanged
I386 core, then call `Direct3DCreate8(220)` from the independently joined genuine
Microsoft system D3D8 image. They reuse the original seven 8x8 scenes and exact
448 readback pixels. Present additionally retains the existing real 64 desktop
RGB observations. HAL/hardware vertex processing, VirtIO 1af4/1050, the bounded
VS1.1/PS1.4 profile, exact monitor LUID/source, nonsoftware KMT identity and all
private payload pins remain mandatory. Paired160 identity is compared before
the factory, at the public selected adapter and before the held checkpoint.

## Contract and operands

Microsoft's local `microsoft-windows-vista-display-driver-64-bit-issues.md`
requires a separate 32-bit UMD registered through `UserModeDriverNameWow`.
The DX10/11 installation docs order the tuple DX9/DX10/DX11. Retained original
Microsoft D3D8 machine code calls `OpenAdapter` with literal Interface8 and its
imports contain no D3D9 API dependency. It uses the legacy DX9 filename slot0.
MAIN already exports public `OpenAdapter` accepting immutable API8/9 owners;
no new production admission, export or capability bit is needed for this probe.

The GPU invocation has exactly nine operands after the executable:

```
--system-offscreen <core-path> <core-sha256> <source-commit40> <luid16> <source-id> <output-prefix> <hold-event> 60000
--system-present   <core-path> <core-sha256> <source-commit40> <luid16> <source-id> <output-prefix> <hold-event> 60000
```

`luid16` is the existing DX8 little-endian byte spelling, not the shared
controller's `high:low` spelling. The core path retains the existing contract:
`C:\Users\Public\DxvkD3D8Candidate-<commit7>-<actual-ci-run>[-icd02]\viogpudxvk.dll`.
Core/source/run provenance must come from the actual selected successful x86
producer. All new core hashes/run bindings remain unknown until then.

The unchanged exact x86 loader, ICD, original JSON and separately derived
module-relative JSON are locked beside that core. `VK_DRIVER_FILES` and
`VK_ICD_FILENAMES` must start absent; the probe sets/restores them itself. The
worker must skip its ordinary D9/10/11 Vulkan environment setup for API8.
Output prefixes start `C:\Users\Public\DxvkD8Lifecycle-`; hold events are
`Local\VioGpuD8Validation-<32hex>` created by the existing owner.

The success checkpoint atomically publishes `<output-prefix>.held.json` after
all pixels, while the public device and original surface remain alive. The
module references were acquired only after the factory actually loaded the
exact I386 core/loader/ICD. The event wait is bounded to 60000 ms. Failure holds
record `pending_exit=1`, `device_alive=false` after COM unwinding, and the actual
remaining module census. Missing modules never become success. The event alone
does not prove restoration. Final successful output follows real release of
the device, API object, private environment and owned module/event references.

## Read-only effective WoW names

The native ARM64 owner's KMT queries report native names and cannot prove x86
WoW selection. The new helper is separately callable in the owner's session0:

```
--system-names <luid16> <expected-WoW-slot0> <expected-WoW-slot1> <expected-WoW-slot2> <output-prefix>
```

It opens the supplied LUID through genuine x86 system GDI, validates the actual
hardware/nonsoftware adapter and paired160 LUID, queries all three versions,
requires each exact expected filename, and closes the adapter. It uses no
monitor/HDC, USER token requirement, graphics factory, core, selector or registry
write. Atomic `<output-prefix>.names.json` retains all 260 UTF16 words per slot
and the original paired160 bytes. Names are read from the saved actual WoW tuple,
not copied from native ARM64 observations.

## Same-owner lifecycle derivation

Extend the bee7 owner only with the explicit API8/x86 branch. Alias its existing
Front role to the direct core for API8; there is no separate diagnostic frontend.
Replace only slot0 of the original `UserModeDriverNameWoW` value in the actual
64-bit display-class key. Preserve the remaining WoW entries and native tuple.
Keep the existing six raw-value backup/replay, protected job, watchdog, exact
device forward/reverse restart, source/payload files, original desktop SID/session
and signed KMD checks. Native effective all3 names must remain the original tuple.

Run the bounded x86 read-only helper before mutation, after forward restart
with the candidate WoW slot0, and after reverse with the original WoW tuple.
Use the genuine fresh forward LUID for the GPU probe. The held record fields are
`schema,pid,timeout_ms,pending_exit,hold_event,output,stage,pixels_passed,device_alive,
modules_exact,selector_installed,restoration_proved_by_event` with schema
`ordinary-system-d3d8-held-v1`. The three name records use schema
`ordinary-system-d3d8-names-v1` and fields
`schema,luid16,names,identity,runtime_calls,core_loads,registry_writes`.

Before executing this descendant, freeze/review the changed probe and include
`src/umd/umd_runtime_identity.h` and its `umd_identity.h` dependency in the native
packet. The previous native CPU packet cannot supply this source build unchanged.
One affected local strict I386 compilation passed with the original SDK KMT
headers, and 19 focused synthetic controls passed for this new
literal reader. Their synthetic pixels are not new GPU observations. The native
EWDK probe-only build, lifecycle-original review integration and genuine new
I386 core remain pending. Historical 448/64 diagnostic controls were not repeated.
No target call, binding change or hardware admission was performed in preparing
this source. Default VM driver remains Mesa.
