# Owned query result tickets

This prerequisite allocates fresh virtual GPU query/event data per issued
query, and captures that exact owner in immediate CS Begin/End. It introduces
no predicate wait, evaluator, individual action suppression or admission.

DxvkQuery::begin clears the old virtual query's Vulkan handles and accumulated
data. A new issued query now gets a separate D3D11QueryDataTicket containing
its own Rc<DxvkQuery>/Rc<DxvkEvent> arrays. Capturing an older ticket retains
that result storage; a later issue cannot begin/reset those same virtual
objects. Both wrapper types own Rc<DxvkDevice>, and existing backend command
lists track raw query/event handles through submission retirement. Ownership
retires when the final captured ticket owner and backend command pins release;
this change does not retain every past result forever.

The constructor still validates and creates the same query kinds/flags/stream
indices. Accepted immediate Begin creates a fresh ticket; duplicate/unscoped
Begin remains ignored. End without Begin creates a fresh implicit interval;
event/timestamp End creates a fresh ticket. Both scoped Begin and its matching
End capture the same ticket in the existing private Com query-owned CS lambda.
The query's current public owner can change on reissue without changing any
already captured CS operation's data.

Deferred replay still prefixes every recorded End before dispatch. Each of
those occurrences now receives a fresh owned FIFO ticket. Accepted CS Begin
peeks the corresponding ticket; CS End takes that same ticket. Repeated and
nested replay allocates fresh objects/IDs, rather than reusing objects saved
at recording time. The existing immutable ordered operation records and
all-End-up-front prefix remain unchanged. Recorded End occurrence IDs and
per-query result ticket IDs are separate identities. A future ordered API
replayer must associate them explicitly before it can bind a mid-list predicate
to the right result; current global API state still points to the last issue.

Ticket completion means CS has recorded its backend End. It is not GPU
availability. The release/acquire completion flag guards ticket-local data
access; actual DxvkQuery::getData/DxvkEvent::test still returns the GPU status.
Public GetData retains its exact phase, global pending counter and final issue-
generation checks, then reads the current ticket. Boolean/statistics/timestamp
conversion and inherited pending/event output remain unchanged. Public query
reads and issue transitions still require context protection or valid app
serialization. No new hint polling is introduced; any future non-hint evaluator
must check the existing hint policy before reading a ticket.

Host controls exercise the actual production completion/FIFO helper with the
same shared_ptr ticket ownership mechanism. GPU availability and payload are
explicit host models, not Vulkan execution. GCC and Clang ASan/UBSan each pass
325757 checks, including 100000 concurrently issued/CS-completed tickets with
independent modeled GPU completion/readers, 4096 prefixed Ends, 128 repeated
seven-End replays, 128 retained old results, false/true reissue and zero final
live ticket owners. Recording End without modeled GPU availability preserves
an output canary. Native COM/backend result data and actual Vulkan execution
remain unverified; old native fixtures/readers/workflows and hardware gates
are unchanged.

The query, immediate/deferred context and command-list sources pass eight
optimized official-SDK x86/x64 object compiles and eight original COFF reopens.
The current compiler dependency closure has 572 selected inputs; this is a
scoped source compile, not a linked DLL or native execution result. Existing
toolchain compatibility warning allowances remain, including the inherited
unused-lambda-capture allowance documented by the preceding ordered-operation
packet. Its original failed compiler diagnostics remain preserved. All 197
existing tests, readers, scripts and workflow files are byte-exact to the base.

The local Microsoft QueryBegin/QueryEnd/QueryGetData/SetPredication contracts
are in reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/.
They define issued/signaled readiness, implicit empty intervals, predicate-hint
restrictions and legal unbound query reissue. This slice preserves valid API
usage and does not newly validate legacy invalid bound-query calls.

Remaining work is an execution-owned map from ordered End occurrences to fresh
tickets, ordered API query advancement, individual predicatable action entries,
API-thread prefix submission/evaluation, deferred source capture and guarded
CPU destination mutations, state restore and the full actual draw/compute/
transfer predicate matrix. A retained ticket makes historical data ownership
possible; it does not by itself implement historical predicate evaluation.
