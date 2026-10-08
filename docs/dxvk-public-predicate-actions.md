# Public CopyStructureCount predicate actions

This slice implements a CPU submission fallback for public core
CopyStructureCount on immediate and deferred contexts. It does not implement
public predication for the remaining draws, dispatches, clears, resource copies,
updates, mip generation or resolves. The existing SetPredication log continues
to identify that broader incomplete support. No discovery, ordinary-runtime,
threading or hardware admission gate changes. Outer UMD already has a separate
predicate fallback and does not call core SetPredication; this slice is not the
sole Mesa replacement gate.

## Contract and action order

Microsoft's [Direct3D 11.3 Functional Specification 20.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.2)
explicitly lists CopyStructureCount among the operations that honor predication.
Guaranteed predicates cannot allow execution because of timing. Hints can
execute unconditionally. Query Issue, state changes, Present, Map/Unmap and
creation are exempt. The local Microsoft PFND3D10DDI_SETPREDICATION contract
defines equal Boolean data as suppression, null bindings as unconditional while
retaining the passed BOOL, and allowed hint behavior. QUERYGETDATA prohibits
polling predicate hints. The local docs and a newly downloaded full official
functional spec are pinned by the validation packet.

A bound CopyStructureCount becomes an individually owned deferred action,
separate from reusable CS chunks. The action owns the original source counter
and destination DxvkBufferSlices. Source counters have independent four-byte
DxvkBuffers; retaining a slice does not depend on a freed counter suballocation.
A destination requiring sequence tracking additionally has a private resource
owner in the action entry. Nested lists copy actions and relocate action IDs
independently from chunk and End occurrence IDs. Predicate/query metadata keeps
its existing private COM owners. An unbound immediate/deferred call continues
through the original CS emission and destination tracking path.

Replay still accepts all Ends before dispatch, preserving public pending,
generation and GetData behavior. Each action gets the replay's exact earlier
End ticket or initial snapshot. Individual actions may be suppressed; state,
query and all other CS chunks are always dispatched. No whole chunk is skipped.
Execution copies each accepted action into a new CS command, so repeated or
nested replay does not mutate recorded command state or share a decision flag.
Executed actions retain their cost, flush hints and destination sequence
tracking. Suppressed/failed actions do not publish new resource use or cost.

## API-thread evaluation

Null and hint bindings execute without polling or synchronization. A guaranteed
predicate is first read using ReadPredicateTicket with its exact retained
owner. If pending, existing FlushRuntimeSubmission dispatches and submits the
preceding CS prefix, synchronizes its CS recording and actual queue submission,
then the API thread polls that same ticket until available or device failure.
The first read may already be available and then needs no submission. A ready
result suppresses the action exactly when its logical Boolean equals the stored
BOOL's logical truth. The API BOOL itself remains stored unchanged.

There is no timeout into unconditional execution. Invalid tickets and device
or backend failures abort this individual action. An invalid ticket is logged;
it reflects invalid predicate usage, not a supported timing bypass. No wait or
API context lock acquisition is installed inside a CS command. The caller
retains the corresponding private COM query owner throughout evaluation and
uses existing context protection or required application API serialization.
CS/backend submission paths do not acquire this API context lock.

The fallback blocks the CPU and uses yield while awaiting GPU availability;
it is a correctness path rather than a GPU performance implementation. Existing
public all-End issue exceptions remain nontransactional. Action/list allocation
exceptions are not new rollback guarantees. Existing query stream0 semantics
for non-stream SO overflow are unchanged. No native error/BOOL ABI or GPU
availability claim follows from local helper testing.

## Local proof and pending native path

Actual GCC and Clang ASan/UBSan runs each report 40192 checks. They execute the
production evaluator, ticket-read/state and order/replay helpers with modeled
GPU availability and copied words: the four logical result/value combinations,
noncanonical BOOL values, null/hint zero polls, invalid/error/device failure,
10000 pending reads with no timing escape, all-End public pending versus exact
historical results, 4000 individual actions, 2000 suppressed actions, 12000
unconditional chunks, 1000 repeated nested replays, 32 old retained tickets,
and a separate completion thread. Query/ticket owners return to zero. These are
not production COM, command queue or Vulkan GPU execution tests.

Ten actual optimized official SDK/MSVC-target x86/x64 COFF objects compile the
common-context explicit instantiations, immediate context, deferred context,
command list and query caller. Two more objects compile the standalone native
probe. Actual LLVM reopens join all twelve object architectures and compiler
input closures. Existing query/GetData/backend/UMD bodies and all preexisting
fixtures/readers/scripts/workflows are checked against the immutable base.

tests/d3d11-predicate-actions-native.cpp is an unregistered standalone public
core probe. It loads an explicit matching DXVK d3d11.dll, with matching DXGI
module available, and requires a hardware device. It is designed for 22 cases
and 88 exact raw words: immediate visible/empty predicates with both BOOLs,
hints, null BOOLs, unconditionally initialized UAV counters, same-query two-End
deferred historical results, three repeated executions, nested lists, restore
TRUE/FALSE and query/source API release after recording. Its 22 raw four-word
files include the destination's untouched word canaries. The empty predicate /
FALSE cases must fail against the original always-copy core stub. No probe was
linked or executed here, and it has no existing CI/Meson/native harness hook.

All other predicatable action classes and the full native/target GPU acceptance
matrix remain pending. CPU fallback cost and native device-loss behavior also
require real execution evidence before any broader admission claim.
