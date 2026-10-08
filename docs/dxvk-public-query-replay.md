# Owning query replay bindings

Each D3D11CommandList execution now constructs a fresh owning operation view.
It snapshots every referenced query's current ticket before the existing
all-End prefix changes those current owners. Each accepted ordered QueryEnd
then receives the exact fresh ticket returned by DoDeferredEnd. The existing
query vector and relocated End occurrence IDs are checked to have the same
order. The immutable recorded list and its chunks remain reusable.

The execution view walks records in order. A Predicate binds the latest earlier
End ticket for its query, or the pre-prefix ticket if no earlier End exists.
The binding is an owning value; a later query End does not replace that saved
binding. Null predicates keep their exact BOOL and hint metadata. Begin records
identify their corresponding next accepted End ticket, matching the existing
backend FIFO. FinalizeQueries still provides an End for each recorded Begin.
Nested append retains its existing relocated chunk and End identities, and
each repeated execution allocates a fresh view and fresh End tickets.

Actual chunk dispatch traverses this execution view. Every chunk, cost, flush
hint and resource sequence-number operation still executes in its original
order. A chunk contains mixed unconditional and predicatable work; it is never
suppressed by this change. SetPredication execution remains a stub. No predicate
wait, Boolean evaluation, conditional destination mutation or admission is added.

The view owns private Com query references and shared ticket owners for the API
replay/dispatch walk. Captured immediate CS tickets, the deferred FIFO and raw
backend command-list query/event pins separately preserve CS/GPU lifetime.
An extracted owning predicate binding can retain its query/ticket after the
view retires. Future asynchronous individual actions must capture such owners
explicitly; this prerequisite adds no per-chunk CS no-op lifetime commands.
Lookup work is proportional to operations times distinct referenced queries.
If ticket allocation/issue fails, a partial accepted-End prefix may remain as
with the preceding prefix loop; this change does not add transactional rollback.

Metadata selection identifies an earlier recorded End, not CS completion or GPU
availability. Existing global pending/current-generation/phase GetData behavior
and conversions remain unchanged, and current public data still refers to the
last prefixed issue. Query issue/read calls require the context lock or valid
application serialization as before. The mapping does not advance public issue
state at each ordered operation, validate legacy invalid bound-query reissue,
or turn an unissued initial ticket into a ready result. A future evaluator must
check exact ticket readiness and hint policy before consuming backend data.

The current host controls exercise actual production replay/order/FIFO/sequence
helpers with shared owners; API query objects and issue callbacks are modeled.
They do not execute native COM, Vulkan queries or predicated GPU actions. GCC
and Clang ASan/UBSan each pass 109673 checks: pre-prefix snapshots despite final
current-owner replacement, exact initial/earlier/reissued predicate tickets,
null BOOL and hints, Begin/End pairing, five nested chunks/four relocated Ends,
4096 appended End occurrences, 1000 concurrently CS-completed seventeen-End
replays with 32 retained views, and extracted binding/recording owner release.
Final live query and ticket counts are zero.

Eight optimized official-SDK x86/x64 query, command-list and immediate/deferred
context object compiles and eight original COFF reopens pass over 573 current
selected compiler inputs. Existing SDK/toolchain compatibility warning
allowances remain, with inherited originals preserved in the preceding packet.
This is a scoped source compile, not a linked DLL or native runtime proof. All
198 existing tests/readers/scripts/workflow files are byte-exact to the base.
The local Microsoft QueryBegin/QueryEnd/QueryGetData/SetPredication contracts
remain the source for implicit intervals, issued readiness, hint restrictions
and legal unbound reissue.

Remaining work is individual predicatable action representation and source
capture, ordered internal API issue advancement, API-thread prefix submission
and exact-ticket GPU evaluation, guarded CPU/GPU destination mutations, state
restore and the complete native draw/compute/transfer/TRUE-FALSE/hint/reissue
matrix. Ordinary runtime and hardware gates remain unchanged.
