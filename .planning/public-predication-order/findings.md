# Findings

Set/GetPredication already stores the exact predicate pointer and BOOL, including
null-predicate BOOL. It does not record deferred predicate transitions. Restore-
CommandListState rebinds graphics/compute state but has no predicate record.
ResetCommandListState currently has no ordered predicate boundary.

The new operation stream will pin exact predicate/query references, record
accepted Begin/End occurrences and preserve the current chunk IDs separately
from operation positions. Nested append relocates chunk IDs and End occurrence
ordinals while preserving every operation and existing resource metadata.
All-End-up-front public readiness stays unchanged. End occurrence ordinals are
recorded-list identities, not retained historical Vulkan result generations.
No predicatable destination mutation or waits are added by this slice.

The old deferred first pending resource chunk was inferred from m_chunkId0 as
ID1, but AddChunk records ID0. The current helper derives pending/current IDs
from the actual recorded chunk count, and nested resource/operation relocation
uses the same function. Existing resource ownership and CS sequence assignment
remain in the original dispatch path.

GCC3299797/0 and Clang3299742/0 each pass155773 ASan/UBSan controls. The tested
production template uses shared_ptr host owners; actual production instantiates
private Com references. These are not native COM/GPU execution claims. Review
3320926/0 proves8 strict optimized official-SDK objects and8 actual LLVM reopens
with571 current selected inputs. Source/host/doc join3327850/0 proves196 existing
test/reader/workflow files exact455 and protected GetData/query generation,
GetPredication and the all-End-up-front prefix unchanged.

Local Microsoft DDI docs were read first. Official public Execute/Finish/Get-
Predication pages were read and fetched3312697/0 for restore parameters absent
from local DDI pages; exact HTML originals/URLs remain pinned in artifacts.
The first common-context compile fails on inherited CopyImage unused captures.
Originals remain preserved. Current compile adds-Wno-unused-lambda-capture;
join3331786/0 proves the whole CopyImage body exact455 and all8 original
diagnostics per architecture. No source warning cleanup was performed.
