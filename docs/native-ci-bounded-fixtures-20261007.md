# Bounded native CI fixture processes

`Invoke-BoundedFixture` previously waited without a timeout after killing an
overdue child, and again after a normal child exit. Those waits could stall a
CI backend job even after its nominal 30-second fixture deadline.

The function now uses the unchanged
`scripts/owned-raw-process-f4bf37f-02.cs`, SHA256
`d8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad`.
One process object retains the original OS handle and exit code. Both raw
output streams drain concurrently. Execution keeps the 30-second deadline;
termination has a five-second wait, and both pipe drains together have a
20-second wait. A complete failed attempt can therefore take up to 55 seconds
plus process creation and filesystem operations.

All 26 existing fixture invocations keep their executable, empty arguments,
working directory, expected exit zero, and stdout/stderr filenames. Raw log
files use `CreateNew`, so a repeated attempt must use fresh output paths.
Each attempt writes `<name>.process.json` before its exit/capture gates run.
Missing exit status, missing handle, timeout, a surviving child, incomplete
drains, capture errors and nonzero exits reject the fixture. The artifact
contains the exact original C# bytes as `owned-raw-process-original.cs.txt`
and `native-fixture-runner-source.json`. Existing failure artifact patterns
retain both without changing workflow triggers or jobs.

The configuration snapshot now contains 11 inputs, including the compiled
C# source, and records `fixture_runner` in `native-build-configuration.json`.
The architecture-specific private-loader change remains a separate preceding
commit. This follow-up changes neither the UMD implementation nor admission.

Original Windows ARM64-hosted/I386-output DX8 CPU evidence already exercised
the exact C# component for 28 children, including 14 expected exit-64 CLI
guards. The independently reopened 127-file archive passed all handle,
exit-code and drain checks. It also passed 306 policy checks, four strict
zero-warning compiles and three PE inspections, with source/compiler/SDK/
library and installed-driver/desktop continuity. This is CPU evidence; the
packet did not build a production core or call the actual system runtime.

Local Mono controls compile that same C# source with warnings treated as
errors and exercise exit zero, exit seven, simultaneous stdout/stderr larger
than pipe buffers, timeout/kill/reap, an absent executable, existing-log
preservation and an invalid deadline. Original commands, byte captures,
process rows and before/after input hashes are retained under workspace
`artifacts/dxvk-native-d3d8-system-runtime-20261007/ci-bounded-runner-local-01`.
This checks the unchanged component on Linux. Native PowerShell parsing and
execution of the changed CI wrapper remain separate validation steps for
the target owner or the next single consolidated CI run.

Other direct fixture commands and the ARM64 runtime script are outside this
change. They do not receive a new bounded-process claim.
