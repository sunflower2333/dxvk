# Findings

A complete non-hint CPU fallback must evaluate on the API submission thread,
flush/synchronize the query End prefix and identify its exact query generation.
Current command-list replay marks all Ends pending first; a middle-of-list wait
can depend on a future unsubmitted reissue. Chunks mix bindings/query work and
conditional commands. Direct mapped-buffer Update writes need suppression
before destination CPU mutation, not only in a later Vulkan lambda.

The isolated prerequisite preserves current command emission and all-End-up-
front deferred readiness. It introduces the actual query sequence generation,
checks that generation before publishing event/query output and moves immediate
GetData under the same context lock as Begin/End. Existing Boolean decode and
GPU aggregation remain exact. SetPredication is still the original stub.

Actual GCC and Clang ASan/UBSan host runs each pass 204130 checks, including
100000 concurrent API issues/CS completions/readers, 4096 deferred Ends,
implicit End and stale-output canaries for both Boolean values. Those are
production sequence-helper controls, not draw/compute/transfer GPU execution.
The GCC TSAN binary compiled, but its runtime stopped before the test with
“unexpected memory mapping”; no successful TSAN/race-detector claim is made.

Core compilation requires the existing DXVK COM/native-header warning
exceptions in addition to the strict UMD toolchain. The matched local Vulkan
and SPIRV header repositories have the exact Gitlink revisions pinned by fae.
Failed compile attempts retain original diagnostics; only final passing source
pins/objects will be certified. No source was changed to suppress those legacy
header warnings.

Final verification uses eight actual strict optimized official-SDK x64/x86
objects for the query, immediate context, command-list and deferred-context
callers. All compile stdout/stderr is empty. Review process 3266830/0 reopens
all eight COFF files with actual LLVM processes and 570 current selected
dependency pins. Source/host/doc join 3268079/0 verifies both host binaries,
the unchanged SetPredication stub, unchanged deferred command emission,
unchanged Boolean decoding, exact admission source bytes and local Microsoft
contracts. These are current selected pins, not a historical toolchain claim.

Independent review by ordinary_runtime_gate_audit finds no blocker for this
limited prerequisite. Final generation validation is not an atomic unlocked
check-and-output operation; public correctness relies on the context lock or
required application serialization. Inherited event pending FALSE-output,
unknown flag acceptance and DONOTFLUSH behavior are unchanged. Legal native
reissue controls must first unbind the predicate before QueryEnd. Ordered
deferred replay and per-generation historical GPU results remain unimplemented.
