# Findings

Official Functional Spec 20.2 lists ClearRenderTargetView as predicatable.
The original public method emits one lambda owning DxvkAttachment view/shadow,
with no API-side destination mutation or original resource-sequence tail.

Immediate unbound clears keep their original path. All deferred clears become
individual action records, including default and explicit null bindings; their
replay uses recorded default/null state rather than the immediate API predicate.
The existing ordered list begins with null/FALSE and nested recording has exact
reset/restore predicate transitions. State/query chunks remain unconditional.

Counter action evaluator/ticket/replay/resource code and its two existing
fixtures remain unchanged. Only the generic invalid-action log loses its
counter-specific name. Native probe execution and broader classes stay pending.
