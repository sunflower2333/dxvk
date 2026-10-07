# Modern development adapter access

`VioGpuDxvkOpenAdapter10_2ForTest` returns the exact WDK
`D3D10_2DDI_ADAPTERFUNCS` table through `pAdapterFuncs_2`. It forwards the
existing internal modern development path. Initial `Interface` and `Version`
are ignored by that ABI; each device request still selects an exact supported
native interface, build and pipeline flag combination. The original
`VioGpuDxvkOpenAdapterForTest` continues to return the legacy adapter table.

This separate development export is needed to exercise typed D3D11 on the
hardware backend. The legacy helper rejects D3D11 and an opened legacy
D3D10.1 adapter cannot subsequently create a D3D11 device. Neither public
`OpenAdapter10_2` admission nor public capability reporting changes.

The existing source-linked adapter fixture now calls the named modern export
for every exact D3D10.0, D3D10.1 and D3D11 creation case. Its Windows guard
pages bound the adapter/device/DXGI output tables. New controls bound the open
input and modern output, reject absent callbacks/handles/output, reject
nonexact successful query status, and reject eleven malformed typed D3D11
creation inputs before a query or backend call. Failed creation preserves the
device and DXGI output bytes. The original production caps/version gate
controls remain intact. Actual Windows execution of these new controls is
pending; local optimized official-SDK x64/x86 COFF and the new development-export
policy row pass. The full existing loadability script has two baseline failures:
it compares newer typed-table slots with the D3D10.0 list, and its negotiation
pattern predates the current per-interface predicates. These unchanged failures
are retained and are not hardware or admission success.

The reusable kernel transport is the real KMT path in
`tests/umd-d3d9-device-probe.cpp`: opaque callback cookies map to owned KMT
handles, contexts/allocations/locks/escapes/renders use actual KMT calls, and
WDDM 2 residency uses a paging queue and bounded CPU fence waits. The
publication-only `tests/umd-kmt-publication.h` cannot serve this profile: it
does not provide the render/escape callbacks required by `RuntimeGpu`.
No runtime kernel callback or wire reply may be replaced with a success stub.

Local primary references are the Microsoft WDK pages for
`D3D10DDIARG_OPENADAPTER`, `D3D10_2DDI_ADAPTERFUNCS`,
`D3D10DDIARG_CREATEDEVICE`, `D3D11DDI_CORELAYER_DEVICECALLBACKS`,
`PFND3DDDI_RENDERCB`, `D3DDDICB_RENDER` and `D3DKMT_RENDER` in
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/`, plus the
installed Microsoft SDK/WDK 10.0.26100.0 definitions. Core callbacks retain
their runtime-owned live table. Render replacement buffers remain authoritative
after the actual KMT call, including failure.

This change is test access, not hardware acceptance or ordinary Microsoft
runtime activation. A subsequent compute/UAV probe must use a newly built,
immutable CI core and exact selected LUID; require per-element readback and
balanced ownership; and reject software adapters and all fallback factories.
Future core source, core hashes and execution evidence remain unset until
actual successful CI artifacts are supplied.
