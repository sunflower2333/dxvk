# Probe-only D3D9 runtime callback diagnostics

Lifecycle04 created each back buffer, then rejected an internal vertex buffer;
cleanup reached its first shared queue submission and returned device-lost.
The original queue diagnostic collapsed shared validation and callback failures
into one Vulkan result, without an underlying HRESULT. The separate read-only
audit/proof in the DX10-completeness worktree retains original archive03bd...,
coreaffe, Mesa8443 and installed KMDc1b9; it does not establish a GPU timeout.

The diagnostic frontend now wraps only the original typed Allocate,
CreateContext, Render and Escape callbacks in its separately permitted
lifecycle mode. It logs complete synchronous `SYSTEM_D3D9_CALLBACK_BEGIN`
and `SYSTEM_D3D9_CALLBACK_END` records. Matching `kind`, runtime handle and
per-owner `call` join the two records. Each includes its actual thread and
original request address. Begin records also include the original table and
function address; `forwarded=1` proves the original function was invoked once.
End records contain its exact HRESULT, including positive non-S_OK values.

Render logs the original context/offset/length, allocation and patch counts,
flags/broadcast count, and all authoritative replacement pointers/sizes and
queued-buffer count. CreateContext logs node/engine/context flags and its
original private header/flags words, then the returned context/buffers/lists.
Allocate logs resource/count/private data/list addresses and the returned
kernel resource/first allocation handle. Escape logs original adapter/device/
context handles, flags, private bytes and up to64 diagnostic bytes before/
after; raw word offsets permit joining actual context/fence replies to the
exact private protocol. Private logging does not reinterpret unknown data as
a valid supported interface.

Escape's first argument is the runtime adapter handle; the optional request
hDevice identifies its device owner. The wrapper preserves both. A null
hDevice can select the only active owner for that adapter, with ambiguous
owners rejected instead of chosen arbitrarily. See the original
[Escape callback contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_escapecb).

The frontend copies only the original22-slot Vista runtime callback prefix and
preserves its99-slot typed device-function copy. The four intercepted function
pointers are read from the original runtime table for each invocation; all
other prefix slots retain their original values. Registry owners are installed
before core CreateDevice, held through failed-create or DestroyDevice cleanup,
and independently pinned by active wrappers. No registry mutex is held while
the original callback executes, so reentrant retirement is safe. The original
CreateDevice request/output addresses remain intact; its callback-table input
pointer is temporarily substituted and restored even on C++ unwinding.

The documented legacy pCallbacks field supplies a runtime callback table; it
does not explicitly promise the dynamically mutable D3D11 core-table policy.
This probe retains the original pointer for bounded protected reads. A missing
or unreadable intercepted slot returns E_FAIL with `forwarded=0`; it must never
be counted as a runtime/KMD HRESULT or hidden by falling back to stale data.
Unreadable optional payload logging likewise reports unreadable while forwarding
the original callback and original request untouched. See the
[legacy CreateDevice arguments](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_createdevice).

The original permission and candidate core pin remain affe. This slice changes
no production core/backend/device, shader/caps profile, installed registration,
GPU selection or admission. The original diagnostic cap adjustments and blocked
enumeration/device-contract modes remain as prepared. All target work belongs
to root; none was performed for this change.

## CPU control and build inputs

`tests/umd-d3d9-runtime-callbacks.cpp` includes the actual frontend implementation
and calls it with typed synthetic callbacks. It tests exact input handles/
request identity/thread, one-call forwarding, positive/failed HRESULTs, partial
output writes on failure, in-place callback replacement, protected optional
payloads and original slots, null/protected requests, callback-owner retirement,
failed-create cleanup, successful-create cleanup and restoration of the original
callback-table input. Guard pages immediately after the22 callback and99 device
function prefixes also verify bounded table access; a Vista Render prefix
checks that logging does not read the current header's appended fields. It
never invokes Microsoft CreateDevice, loads a candidate, changes registration
or issues a GPU call.

Strict original MSVC/SDK/WDK-header Clang COFF generation passed for actual
frontend and CPU-control translation units on both x64 and x86 with
`-Wall -Wextra -Werror`, four objects and zero diagnostics per recorded attempt.
The existing case-insensitive header overlay and Windows/winternl prelude are
retained. Native MSVC ARM64 compilation/execution and actual runtime diagnostic
results are pending root; local COFF is not native execution or hardware proof.

For root's native CPU packet, compile the unchanged frontend source/DEF and
probe as before, adding `tests/umd-d3d9-runtime-callbacks.h` to exact Git inputs.
Build the separate CPU-control EXE from only
`tests/umd-d3d9-runtime-callbacks.cpp`; it includes the frontend/header and needs
the same official static CRT and kernel32 import library, with no D3D9 core or
DXVK object. Keep `/W4 /WX /MT /std:c++17 /EHsc /Zc:preprocessor` for every unit.
Do not also link a separate frontend object into that control.

Its success marker is
`^probe D3D9 typed runtime callbacks verified checks=[0-9]+ calls=16 vista_callbacks=22 vista_functions=99 hardware_admission=0$`.
The actual check count must be recorded from native output. The unchanged
probe still needs its existing official d3d9/user32 import libraries and the
existing10 malformed-CLI/3null guards/ordinary enumeration controls. Genuine
lifecycle execution requires root's separate authorization/CPU ownership and
cannot be inferred from this synthetic control.

If all four observed callbacks succeed but Render is absent, or Render succeeds
and shared submission still fails, this slice does not identify the lower
failure. A separately coordinated RuntimeGpu/Mesa identity/status/reference/
packet/replacement stage diagnostic would then be required. Original failures
and partial outputs remain evidence, not successful rendering claims.
