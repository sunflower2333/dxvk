# Findings

Microsoft Direct3D 11.3 Functional Specification 20.2 explicitly includes
CopyStructureCount among the comprehensive operations honoring predication:
https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.2
Both implementing agent and ROOT reopened the official primary document.
Guaranteed predicates may not execute because of timing; hint predicates may.
Issue, state changes, Present, Map/Unmap and resource creation remain unaffected.

Local PFND3D10DDI_SETPREDICATION contract defines equal-result suppression,
null binding ignoring but retaining BOOL, and allowed unconditional hints.
Its local QUERYGETDATA contract prohibits polling predicate hints.

Current CopyStructureCount records one CS lambda owning destination/source
DxvkBufferSlices and tracks destination buffer sequence when required. There
is no API-thread destination write. This is the smallest predicatable action
chosen for this slice. All other action classes remain explicitly unsupported
by public core predication; outer UMD has its separate existing fallback.

Replay owns exact tickets, while public readiness still issues all Ends up
front. Exact historical tickets make prefix reads independent from the public
pending count. An evaluator must run on the API dispatch thread after the
relevant End's CS prefix is submitted; it must never wait in a CS lambda.

CopyStructureCount captures backend slices only after input/counter validation.
UAV CreateCounterBufferView allocates an independent four-byte DxvkBuffer and
view, so retaining the source slice remains valid after source API release.
Action entries separately own a destination resource only when sequence tracking
requires its API implementation; all calls retain its backend slice.

Production only evaluates the new action type. Existing resource/chunk tails,
query readiness and all untargeted commands remain unchanged. Existing issue
exceptions are not transactional; no action rollback claim. CPU yield fallback
may be slow and requires native device-loss/driver behavior evidence.
