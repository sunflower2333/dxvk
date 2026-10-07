# Native D3D9 runtime-owned locked vertex buffers

The genuine Microsoft D3D9 and D3D9Ex lifecycle attempt04 accepted both
16x16 X8 backbuffers (resource flags1081), then rejected an internal vertex
buffer with flags02080004, pool SYSTEMMEM, size65536, and borrowed CPU memory.
Both public device creations returned80070057 without a device or pixels.

Original evidence is retained in
`artifacts/dxvk-native-d3d9-discard-20261007/candidate-affe7d4-01/` under the
workspace root. The42-member lifecycle archive has SHA256
`03bd63362010a975402a8dc7578564c3919a168b63b3c5dfed635a41cfbb6704`.
Independent review `root-system-runtime-discard-lifecycle-04-verified-01.json`
has SHA256
`b15d5cfc68bceae8cc06b2793ed51ca514ff6c9fbfa0622eac84c1e02c8c65ed`.
Registration, critical readiness fields, the signed driver, desktop processes
and owned task cleanup were verified. Cleanup's first flush returned88760868
and Mesa reported submit-4 before its first fence; that secondary failure
remains unresolved.

## Implemented contract

Local Microsoft documentation says the runtime may draw during software
transform and lighting before unlocking a vertex buffer, and marks both its
CreateResource and Lock requests with MightDrawFromLocked. The flag is not
valid on an index buffer. References:

- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_lock.md`
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/ns-d3dumddi-_d3dddi_lockflags.md`
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dukmdt/ns-d3dukmdt-_d3dddi_resourceflags.md`

The native UMD supports this flag for lockable SYSTEMMEM vertex buffers with
runtime-owned CPU storage. A draw from a locked buffer additionally requires
the matching flag in that particular Lock call. Ordinary locks, index buffers,
foreign handles, shared/primary resources and missing CPU storage retain their
validation. A static native hint uses dynamic private SYSTEMMEM backing so its
borrowed bytes can be captured on each draw.

Draw preparation copies each used flagged buffer once on the caller thread,
before status queries or other runtime callbacks. Worker execution copies that
snapshot into private staging storage and explicitly queues FlushBuffer before
Draw. This preserves both the runtime and private lock. The explicit flush
also handles mixed normal/flagged streams or an extra unused flagged stream,
where DXVK's per-draw upload optimization does not apply. No worker reads
borrowed runtime data.

## Vertex-copy bounds

The newly reachable private SYSTEMMEM path exposed an existing partial-tail
copy defect. With a60-byte buffer, first vertex1, count3, stride16 and declared
extent12, all requested vertex bytes fit exactly. The old planner subtracted
only the binding offset, causing an extra copy from logical offsets64..75.

The shared production planner now accounts for binding and vertex offsets,
bounds multiplication first, and caps complete/partial elements at the request
and actual source extent. It supports the valid final declaration without
requiring unused stride padding. Negative or overflowing source offsets produce
an empty copy. Combined VB/IB sizes are checked before UP allocation; an
overflow uses the ordinary buffer path, and index byte offsets/counts are
clipped with64-bit arithmetic. Three unused lambda captures found by strict Clang compilation
were removed without changing their commands.

## Verification and remaining work

The production copy planner passes4,050,444 scalar-byte, exact-allocation,
destination-guard, partial-tail and overflow checks under GCC and Clang
ASan/UBSan. Local x64/x86 cross compilation against official Windows headers
passes the UMD device/backend, controlled device fixture, production D3D9
device and copy fixture. These controls are local compilation and portable
execution evidence.

The consolidated Linux script passes its existing checks and the new planner,
private-child4029 and multisample269 controls. The existing automatic workflow
now builds/packages25 native ARM64 cases, including the merged DX10 format and
DX11 input/ownership controls. No additional workflow is enabled.

Strict native ARM64 execution of the changed device fixture, actual repeated
locked-buffer mixed/unused-stream rendering, and a fresh genuine system-runtime
lifecycle remain required. The original512-pixel lifecycle oracle and deadlines
remain the acceptance criteria. This change does not establish public-device,
presentation, DX8-DX11 hardware or desktop acceptance.
