# Findings

DxvkQuery::begin clears old Vulkan query handles and accumulated data. Reusing
one virtual query across generations cannot preserve a prefix result owed to an
older predicate. DxvkQuery and DxvkEvent each own an Rc<DxvkDevice>, so retained
fresh per-issue objects can preserve both result and backend device ownership.
createGpuQuery/createGpuEvent allocate virtual wrappers; real query/event pools
are assigned during CS backend execution.

Immediate CS closures must capture the exact API-issued ticket before later
reissue replaces the current public ticket. Deferred replay currently prefixes
all Ends before dispatch. Fresh owned FIFO tickets for those End occurrences
allow each corresponding CS Begin/End to use its own virtual query/event data.
Public pending/readiness remains global all-End-up-front for compatibility;
ticket-local End-recorded readiness does not imply GPU availability. Ordered
API-side advancement and actual conditional evaluation remain unimplemented.

The backend command list separately tracks raw pool query/event handles through
GPU retirement; retained virtual wrappers keep the device and accumulated result
owners alive. Fresh ticket ownership changes no query kinds, flags, stream
indices, public Boolean/statistics/timestamp decode or all-End prefix behavior.
Current public GetData reads the last issued ticket only. Historical result
evaluation still needs an execution-owned ordered occurrence-to-ticket map.

Actual production FIFO/completion helper controls, using shared_ptr ownership,
pass 325757 checks under both GCC and Clang ASan/UBSan. GPU payload/availability
is an explicit host model. Eight strict optimized official-SDK object compiles
and eight LLVM COFF reopens pass over 572 current selected inputs. All 197 base
tests/readers/scripts/workflow files remain byte-exact. No native GPU or COM
result test, predicate evaluator, guarded action or admission claim is made.
