# Findings

Frozen8d0 Present probe uses the same nine operands/held schema/negative9 as
corrected786. It has a separate exact Present2 positive marker and independent
window/swapchain reader. A controller phase is therefore marker selection for
an already approved probe binary, not a new runtime probe argument.

New pure phase guard runs before the controller's native helper, and before
the worker's selected input/entry gates. Watchdog still restores independently.
All registry ownership and restoration code is retained from020f.

Current published0ccc CI failed x86/x64 shader scene; ARM runtime skipped.
No approved current core or hardware tuple is inferred from these controls.
