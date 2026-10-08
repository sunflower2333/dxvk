# Exact-ticket predicate result reads

The private D3D11Query::ReadPredicateTicket method performs one nonblocking
poll of the selected retained virtual DxvkQuery. It does not consult the public
last-prefixed current ticket or global pending/generation. Public GetData and
its existing conversions, locking, flags and output behavior remain unchanged.
No submission, wait, ordered internal issue advancement or action suppression
is added; the public SetPredication execution stub remains unchanged.

Each accepted immediate/deferred End now marks its exact ticket EndIssued before
CS publication. An initial or Begin-only ticket is unissued and private reads
return DXGI_ERROR_INVALID_CALL. An issued End not yet recorded by CS returns
S_FALSE without polling. EndRecorded remains a separate release/acquire marker:
it establishes backend End recording, not GPU availability. Fresh wrappers are
never reset by a later issued query, so an older completed ticket has no pending
Begin on its own virtual backend object. Actual DxvkQuery::getData continues to
mutex-protect accumulation and report its existing GPU statuses.

The reader rejects absent/foreign-owner tickets, missing backend queries and
nonpredicate query kinds. A ticket's raw queryOwner is identity only, not an
owning COM reference; valid callers must retain the corresponding private Com
query owner. Existing owning replay bindings already do so. During a poll the
reader retains the shared ticket and its actual Rc<DxvkQuery>; those wrappers
own the device. A retained ticket alone does not authorize calling a freed API
query object. Native COM/foreign-owner lifetime execution is not verified here.
The private method's HRESULT/BOOL ABI behavior is source/object evidence only;
host execution covers the shared readiness/decode helper.

The predicate-hint check prevents every GPU data poll. The private reader reports
an invalid read for hints; a future evaluator may choose allowed unconditional
execution before calling it. For non-hint issued/recorded tickets, the reader
calls actual DxvkQuery::getData once. Pending returns S_FALSE; Invalid/Failed or
other nonavailable status returns DXGI_ERROR_INVALID_CALL. Only Available writes
BOOL: occlusion samplesPassed != 0, or SO primitivesNeeded > primitivesWritten.
All pending/error paths leave caller output untouched. A null output pointer
permits an availability-only poll. Existing nonprecise occlusion early-positive
availability is retained; the reader trusts actual backend availability status.

Actual backend status/data declarations were moved unchanged, apart from line
whitespace, from dxvk_gpu_query.h to a light dxvk_gpu_query_data.h included by
the original header. This allows production readiness/decode host controls to
use the same enum/union and 64-bit counters as the backend, with no duplicated
data definitions. Actual Vulkan query backend implementation remains unchanged.
The include follows existing platform headers to preserve the original data
declarations' packing/macro point. Legacy non-stream SO_OVERFLOW retains its
existing stream-zero mapping FIXME; per-stream native acceptance is unverified.
The initial Linux full-header probe failed at its Windows COM header dependency
(owned 3447223/1); its original diagnostics are preserved.

GCC and Clang ASan/UBSan host controls each pass 200187 checks. They exercise the
actual reader helper, backend data types and ticket/sequence/FIFO helpers with a
modeled poll callback: unissued/CS-pending/invalid/failed/hint zero-poll cases,
pending/error canaries for both Boolean values, null output, uint64 nonzero and
SO overflow boundary pairs, exact older result reads while 4095 later public
Ends remain pending, and 100000 concurrent issued/CS-completed/modeled-GPU-ready
tickets with 128 retained old results and zero final live owners. The GPU poll
and availability are modeled; no actual Vulkan query is executed by those tests.
These controls run through owned local recipes and are not registered in Meson
or CI by this change.

Ten optimized official-SDK x86/x64 objects and ten original LLVM COFF reopens
pass, including query/immediate/deferred/command-list callers and the actual
Vulkan query backend source, over 576 current selected compiler inputs. Existing
toolchain compatibility warning allowances and preceding original diagnostics
remain preserved. This is a source/object compile, not a linked/native GPU proof.
Existing 199 tests/readers/scripts/workflow files are byte-exact to the base.
Local Microsoft QueryBegin/QueryEnd/QueryGetData/SetPredication documents supply
the issued/signaled, implicit interval, output preservation and hint contracts.

The outer UMD already implements its own non-hint predicate evaluation and tags
predicatable native operations with a suppression wrapper. It does not call the
public DXVK SetPredication stub. This public-reader feature is therefore not the
sole ordinary-runtime creation or Mesa replacement gate. That outer path and
all ordinary/hardware admission gates remain unchanged.

Remaining public-feature work is prefix submission and exact-ticket evaluation,
ordered internal issue advancement, individual predicatable action/source capture
and CPU/GPU destination guards, state restoration and actual native draw/compute/
transfer/TRUE-FALSE/hint/reissue testing. This private result-reader prerequisite
establishes none of that complete execution behavior.
