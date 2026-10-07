# Native D3D11 queries and input assembly

This source slice follows the typed shader/device port and leaves production
interface admission closed. It uses the existing private exact-LUID DXVK core,
its real query/input-layout objects, and the existing caller-thread callback
pump. It adds no device owner or public runtime replacement.

## Query behavior

`queryInfo11` translates the exact WDK native query names into the public
backend names. Native stream statistics are contiguous; public statistics
and predicates alternate. They are not interchangeable numeric enums.
`D3D11DDI_QUERY_PIPELINESTATS` returns the full 88-byte eleven-counter payload,
including HS, DS and CS invocation counts. The legacy D3D10 query name retains
its 64-byte payload through an 88-byte backend scratch buffer. SDK size/offset
assertions check both layouts.

All four SO statistics and overflow predicates use real backend queries. The
legacy overflow predicate on a feature-level11 native device observes all
four streams, as the D3D11 contract requires. The pinned DXVK backend currently
implements that public legacy predicate using stream zero. The native adapter
therefore owns four exact per-stream predicates and aggregates their completed
BOOL results. An incomplete or failed constituent leaves the caller's output
unchanged. This also supplies the existing correctness-first CPU predication
fallback; it does not claim efficient GPU conditional rendering.

Pending/error output staging, status-only reads, predicate hints, begin/end
rules and bound-predicate rejection remain explicit. QueryEnd without Begin
is a legal empty interval. Optional performance counters remain unsupported.

## Input assembly

Input formats now include typed R32, R16 and R8 scalar/vector families,
normalized/half-float conversion, packed R10G10B10A2 and R11G11B10_FLOAT.
Creation requires the actual backend's `IA_VERTEX_BUFFER` format-support bit
and a successful non-NULL input layout. Typeless, depth, sRGB, BGRA, compressed
and video formats are rejected by this subset. Each element feeds the correct
float32, uint32 or sint32 native shader input register.

Offsets use `min(element byte size, 4)` alignment. APPEND offsets resolve from
the previous element in the same slot, even with interleaved slot declarations.
Element extent must fit 2048 bytes. Per-slot classification and instance rate
must agree; per-vertex rate must be zero. Existing real vertex/index bindings
and draw argument forwarding remain in place.

Logical10.0 limits inputs, registers and vertex slots to16; logical10.1/11.0
allows32, matching the SDK constants. Layout-only metadata retains SM4.0,
SM4.1 or SM5.0 accordingly, including the last valid register. These minimal
layout tokens are never used as an application shader.

Query and layout private storage is now an opaque registration key. Independent
shared owners survive reentrant callbacks. A creation ticket prevents rollback
from erasing a new child at a reused address; a retained runtime-service epoch
prevents device-address reuse from granting ownership. Failed creation leaves
runtime bytes untouched and releases staged backend references before reporting
an error. Destroy removes the registration and queues COM retirement; device
cleanup drains remaining records. Unknown, foreign and retired handles are
rejected without reading the runtime's private bytes.

## Verification and exact native inputs

The portable registry fixture passes 4029 checks, including concurrent pins,
retirement/reuse, suspended creation rollback and reentrant epoch cleanup. The
portable input-format fixture passes 41 scalar/alignment/overflow checks using
the official standalone DXGI enum header. Both passed with AddressSanitizer
and UBSan. Strict local clang Windows x64/x86 checks against official SDK/WDK
26100 headers passed for the DDI and all five affected fixtures. The portable
SM5 fixture now passes351 checks under ASan/UBSan, including exact version,
hash and all scalar/register metadata for16/32-entry input layouts.

`tests/umd-query.cpp` adds exact four-stream enum translation, full pipeline
counter offsets and bounded pending/error output checks. It has type-checked
with the official Windows SDK; its new execution checks are pending a native
build.

The extended source-linked WARP fixture now uses native query DDIs for compute,
tessellation and four GS streams. It checks asymmetric 3/6/9/12 primitive
counts, four no-overflow results, stream-two-only overflow, the aggregate
predicate, gap/stride canaries and predicate lifetime rules. Packed UNORM,
half, narrow signed/unsigned and R10 inputs are streamed out after indexed
instanced draws using slots0/1/31, an instance divisor of2, a zero-stride/rate
input, nonzero start/base offsets, and both index widths. Complete results are
compared with the original app shader blobs and exact public API draw parameters.
Additional controls protect retired private pages with PAGE_NOACCESS, reject
foreign/duplicate handles, mutate live callback slots, and destroy query/layout
objects from the error callback. Exact typed10.1 and logical10.0/11.0 layout
controls reject excess counts/registers/slots and retain private-byte canaries.
These new WARP controls have not run on Windows
yet. They cannot establish genuine Turnip, presentation or normal runtime
acceptance.

The extended native executable has the unchanged source/link manifest in
[native-d3d11-ddi-20261007.md](native-d3d11-ddi-20261007.md#native-fixture-inputs):
both `umd_shader.cpp` and `umd_shader11.cpp`, the actual DDI/allocation/runtime
objects, the pinned dxbc container/parser/signature/types/interface/IR/hash/log/
swizzle objects, and `d3d11.lib d3dcompiler.lib`. New header-only dependencies
are `src/umd/umd_private_children.h` and `src/umd/umd_input_format.h`, alongside
the existing typed11/shader helpers and peer-owned interface header.

New standalone native targets require no additional project objects or graphics
libraries:

```text
dxvk-umd-query-test:            tests/umd-query.cpp
dxvk-umd-private-children-test: tests/umd-private-children.cpp
dxvk-umd-input-formats-test:    tests/umd-input-formats.cpp
```

All use C++17 and the selected architecture's normal MSVC/UCRT/STL; the query
target also needs the official WDK/SDK include roots. The input-format target
uses the SDK's `dxgiformat.h`. No target/CI/push operation is part of this slice.

Signature-only NULL-code GS/SO and cross-format conversion remain incomplete.
Command-list capability remains unadvertised. Full required behavior and actual
hardware/runtime acceptance must be established before enabling production11.

Primary contracts:
[native query enum](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ne-d3d10umddi-d3d10ddi_query),
[native QueryGetData](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_querygetdata),
[public query semantics](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query),
[input element description](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_input_element_desc),
[indexed instanced parameters](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-drawindexedinstanced).
