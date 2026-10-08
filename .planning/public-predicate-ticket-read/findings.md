# Findings

The public current ticket/global pending cannot select an earlier ordered End
while later prefixed Ends are pending. Retained per-issue wrappers now permit
single-poll reads of the exact selected ticket after its CS End completes.
Actual DxvkQuery::getData already mutex-protects accumulated data and reports
Invalid/Pending/Available/Failed; non-precise occlusion may become available
early when it is already known nonzero.

EndRecorded=false currently conflates an unissued/Begin-only ticket with an
accepted End waiting for CS. A ticket-local EndIssued marker must distinguish
invalid unissued reads from valid issued-pending reads. Hint policy must stop
all GPU polling before readiness/data access. Public GetData stays unchanged.

Actual GPU query data/status declarations currently share a header requiring
Windows COM headers. Linux host reachability probe 3447223/1 failed at missing
windows.h; original preserved. Move those declarations unchanged to a light
production data header so host controls use actual backend types, not copied
model enums or union definitions. The Vulkan query backend implementation and
public output conversion policy remain untouched.

The private method checks exact ticket owner identity and predicate kind, holds
the shared ticket/actual Rc query during its single poll, and writes BOOL only
on Available. The raw owner pointer is identity only; caller Com owns the API
query. EndIssued marks accepted Ends before CS publication and distinguishes
initial/Begin-only invalid reads from issued-but-unrecorded S_FALSE. Hint paths
perform zero GPU polls. No waits/evaluator/destination action is added.

Current actual production-helper tests use the moved actual backend status/data
types, with modeled GPU polling/availability, and pass 200187 checks under both
GCC and Clang ASan/UBSan. Ten SDK x86/x64 caller/backend objects and ten actual
LLVM reopens pass with 576 current selected compiler inputs. Public GetData and
outer UMD predication behavior remain exact; no native/GPU/admission claim.
