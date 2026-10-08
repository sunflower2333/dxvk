# Findings

The actual quiet05 worker source sets VK_LOADER_DEBUG to error,warn,driver before its API11 TU_WDDM_DIAGNOSTICS=0 override. Saved stderr contains INFO/DEBUG DRIVER messages and warnings. Error-only loader output is necessary to make the existing zero-stderr success contract internally consistent while retaining loader failures.

Quiet05 selected the buffered 4697-byte d8cf runner. Both CopyToAsync pipe readers start before WaitForExit, but FileStream flush is in final cleanup. The exact 4096-byte visible stderr prefix ending midline does not prove a pipe deadlock. Probe-start occurred about 72 seconds after binding; controller recovery began about eight seconds after probe-start. The controller's 80-second hold deadline and runner's 115-second probe deadline have different starting points. No completed 115-second probe timeout or public factory return is proved.

The progress runner is an independent visibility proposal. Existing API11 owner and ordinary-reader pins require d8cf, so changing only the packet Runner would fail. A coordinated future policy/packet update plus fresh native CPU verification is required; it cannot establish a rendering fix.

No local pwsh, powershell or dotnet command is available. Native controls and AST validation remain explicitly pending ROOT. Existing source-routing controls add nine checks (70 to79), retain eight diagnostic cases, and preserve other case counts.
