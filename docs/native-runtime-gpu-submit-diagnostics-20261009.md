# First runtime GPU submission diagnostics

The ordinary SYSTEM D9 attempt using selected ARM64 core c8cda1f failed
CreateDevice9 with 80004005 after the matched private Mesa8443 ICD rejected
its first nonempty submission: three entries, six references, fence1 and no
transferred fence. The current source parent0eee0a2 also includes the newer
DDI11 amortized callback; that source was not exercised by the D9 attempt.

The early native context/device close messages come from physical-adapter
enumeration. Both accepted USER06 D10 and D10.1 originals contain the same
sequence and zero KMT handles at final destruction. Runtime-owned Vulkan
devices use the shared callbacks and intentionally carry no KMT handles.
The exact Mesa8443 shared render branch silently reduces context, status,
reference, packet and submit-callback failures to VK_ERROR_DEVICE_LOST.
Those originals do not establish an owner-lifetime defect or a GPU timeout.

Set TU_WDDM_DIAGNOSTICS to exactly1 before creating a runtime GPU owner to
enable VIOGPU_RUNTIME_GPU stderr records. Each owner emits at most its first
context-ready, submit-entered, submit-succeeded, failure and five close
boundary records. The failure
stage distinguishes dispatch, generic callback results, context callbacks/validation, status, submit
arguments/buffers/reference/alias/identity, RenderCb and replacement buffers.
Fields retain the normalized HRESULT at the recorded failure stage, whether
that stage invoked a callback,
its original HRESULT including positive non-S_OK, generation, context and
queue IDs, reference/stream counts, locked-reference count and rejected index.
Index4294967295 means no particular reference. No borrowed payload is dumped.

The close boundaries are close-entered, allocation-cleanup-finished,
destroy-context-entered, destroy-context-finished and close-finished. They
remain visible after a submission failure. Allocation cleanup reports the
first failure returned by the existing release loop; the context result also
retains the original DestroyContextCb HRESULT before normalization. The final
close result preserves the existing precedence of an earlier cleanup failure.
Every record includes live allocation handle count, locked allocation count
and active callback count. The disabled trace does not walk allocation owners.

Allocation-cleanup-finished means the existing loop returned, including when
a release failed; its HRESULT and remaining handles disclose that failure.
When active callbacks cause the existing close path to skip cleanup, the
entered/final records disclose active_calls and residual owners and no
allocation/context cleanup records are emitted. Close-finished records the
existing logical close, not successful kernel cleanup. Context and generation
are captured before callbacks or close erase them. A missing boundary can
localize a stopped call only after confirming this source and diagnostic flag.

The ordinary SYSTEM D9 hardware02 attempt using selected aa2a2da reached
RenderCb and received original 80004005 with six references, a 228-byte native
stream and five locked references. The public CreateDevice call did not
produce a return record or reach the probe hold. Its later native observer
reported complete, released context10 teardown and zero Render failure fields;
those facts do not establish the Render refusal's kernel status or a locked
allocation cause. The close records add finite localization for that remaining
uncertainty without changing callback behavior.

The submit-entered record is made before argument validation. The successful
record requires the unchanged callback, replacement-buffer, owner and identity
checks to pass. If ICD failure follows context-ready with no submit-entered,
the failure preceded RuntimeGpu::submit; callback-dispatch identifies a request
whose callback body was not entered, and callback-result identifies an invoked
body without a more specific failure record. Repeated cleanup failures cannot replace
the first failure. Absence of a record is meaningful only when the exact
diagnostic source and flag are confirmed, and is not hardware proof.

Microsoft's local CreateDevice and CreateContextCb contracts require explicit
context creation; RenderCb requires the original runtime device handle and
authoritative replacement buffers even on failure. Existing source follows
those contracts. LockCb documentation identifies locked-render rejection as
one possible failure, but no recorded HRESULT establishes it here. This change
preserves mapping, callbacks, ownership, validation, HRESULTs, masks and the
private UMD/ICD ABI. It adds no registration or production admission.

The small output control exercises disabled/enabled behavior, exact scalar
and HRESULT output, positive callback HRESULT preservation, per-owner event
suppression and first-failure retention. It is also included in the existing
runtime GPU fixture. Local standalone execution and strict x86/x64 official
SDK COFF compilation are source checks; native rendering remains pending.
