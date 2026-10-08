# SPDX-License-Identifier: MIT
# CPU-only tiny stdout/hold control; no registry/KMT/frontend/API/GPU use.
param([Parameter(Mandatory)][string]$ReleasePath)
$ErrorActionPreference='Stop'
[Console]::WriteLine('HELD binding-progress-control')
[Console]::Out.Flush()
$clock=[Diagnostics.Stopwatch]::StartNew()
while (!(Test-Path -LiteralPath $ReleasePath)) {
    if ($clock.ElapsedMilliseconds -gt 5000) { exit 9 }
    Start-Sleep -Milliseconds 20
}
exit 0
