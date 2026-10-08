# Findings

Official Functional Spec 20.2 includes ClearDepthStencilView and both UAV clear
entry points. The base methods emit six independently owned backend commands;
none has an existing resource-sequence-number tail. Writable DSV aspects and
the integer/float UAV format conversion/recreation logic must remain exact.

Every deferred action must use recorded default/null predicate state. Invalid
view/aspect/format early returns remain outside the command. Image compatibility
changes remain inside the conditioned command. No whole CS chunk is suppressed.

The outer UMD already guards its own predication. These public core actions
are broader semantics work; they do not establish ordinary Mesa replacement.
