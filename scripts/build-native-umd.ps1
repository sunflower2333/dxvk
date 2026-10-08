# LibraryName and VulkanLoader are for driver-package builds: one flat
# DriverStore directory holds a UMD per architecture, and each resolves its
# own private Vulkan loader beside itself. The defaults keep the development
# build unchanged.
param([string]$OutputDirectory = 'artifacts/umd',
      [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$LibraryName = 'viogpudxvk',
      [ValidatePattern('^(|[A-Za-z0-9_-]+\.dll)$')][string]$VulkanLoader = '')
$ErrorActionPreference = 'Stop'
$sdkVersion = ($env:WindowsSDKVersion -replace '\\+$', '')
if (-not $sdkVersion) { throw 'Run in the selected MSVC developer environment' }
$kit = Join-Path ${env:ProgramFiles(x86)} "Windows Kits/10/Include/$sdkVersion"
if (-not (Test-Path "$kit/um/d3d10umddi.h")) {
    $installer = Join-Path $env:RUNNER_TEMP 'dxvk-wdksetup.exe'
    Invoke-WebRequest 'https://go.microsoft.com/fwlink/?linkid=2272234' -OutFile $installer
    $process = Start-Process $installer -ArgumentList '/quiet', '/norestart', '/features', '+' -Wait -PassThru
    if ($process.ExitCode -notin @(0,3010)) { throw "WDK installation failed: $($process.ExitCode)" }
}
if (-not (Test-Path "$kit/um/d3d10umddi.h")) { throw "WDK is not paired with SDK $sdkVersion" }
python -m pip install meson ninja
if ($LASTEXITCODE) { throw 'Build dependency installation failed' }
if (-not (Get-Command glslangValidator.exe -ErrorAction SilentlyContinue)) {
    Invoke-WebRequest https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton-ci/main/glslangValidator.exe -OutFile glslangValidator.exe
    $env:PATH = "$PWD;$env:PATH"
}
$arch = $env:VSCMD_ARG_TGT_ARCH
$cpu = if ($arch -eq 'arm64') { 'aarch64' } elseif ($arch -eq 'x64') { 'x86_64' } elseif ($arch -eq 'x86') { 'x86' } else { throw "Unknown target $arch" }
$buildSourceCommit = (& git rev-parse HEAD | Out-String).Trim()
if ($LASTEXITCODE -or $buildSourceCommit -cnotmatch '^[0-9a-f]{40}$') { throw 'Missing build source identity' }
$isGitHubBuild = $env:GITHUB_ACTIONS -ceq 'true'
if ($isGitHubBuild -and ($env:GITHUB_SHA -cne $buildSourceCommit -or
    $VulkanLoader -cne "viogpu_gl_loader_$arch.dll")) {
    throw 'CI must build its exact source with the architecture-specific private loader'
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
# Snapshot the configuration inputs before Meson. A later checkout or source
# edit must not give an already-built DLL a new source/configuration identity.
$configurationInputs = @('.github/workflows/build-native-umd.yml', 'scripts/build-native-umd.ps1',
    'scripts/restore-native-ci-source.py',
    'meson.build', 'meson_options.txt', 'src/vulkan/meson.build', 'src/vulkan/vulkan_loader.cpp',
    'src/vulkan/vulkan_loader.h', 'src/umd/meson.build', 'src/umd/umd_vulkan_loader.cpp', 'src/umd/viogpudxvk.def',
    'scripts/owned-raw-process-f4bf37f-02.cs')
$configurationBefore = @($configurationInputs | ForEach-Object {
    $name = $_
    $expectedBlob = (& git rev-parse ($buildSourceCommit + ':' + $name) | Out-String).Trim()
    if ($LASTEXITCODE) { throw "Missing configuration source $name" }
    $actualBlob = (& git hash-object --no-filters -- $name | Out-String).Trim()
    if ($LASTEXITCODE -or ($isGitHubBuild -and $actualBlob -cne $expectedBlob)) {
        throw "CI configuration source differs from Git: $name"
    }
    [ordered]@{path=$name;bytes=(Get-Item -LiteralPath $name).Length;
        sha256=(Get-FileHash -LiteralPath $name -Algorithm SHA256).Hash.ToLowerInvariant();
        git_blob=$expectedBlob;actual_blob=$actualBlob;matches_git=($actualBlob -ceq $expectedBlob)}
})
$configurationBefore | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'native-build-source-before.json') -Encoding UTF8
@"
[binaries]
c = 'cl'
cpp = 'cl'
ar = 'lib'
windres = 'rc'
[properties]
needs_exe_wrapper = true
[host_machine]
system = 'windows'
cpu_family = '$cpu'
cpu = '$arch'
endian = 'little'
"@ | Set-Content native-umd-cross.ini
meson setup build-umd --cross-file native-umd-cross.ini --buildtype release -Denable_umd=true -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false "-Dumd_library_name=$LibraryName" "-Dumd_vulkan_loader=$VulkanLoader"
if ($LASTEXITCODE) { throw 'Meson configure failed' }
ninja -C build-umd src/umd/dxvk-umd-identity-query-test.exe src/umd/dxvk-umd-adapter-test.exe src/umd/dxvk-umd-d3d9-adapter-test.exe src/umd/dxvk-umd-query-test.exe src/umd/dxvk-umd-allocation-test.exe src/umd/dxvk-umd-shader-test.exe src/umd/dxvk-umd-view-test.exe src/umd/dxvk-umd-shared-policy-test.exe
if ($LASTEXITCODE) { throw 'Runtime adapter CPU test build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-public-adapter-test.exe
if ($LASTEXITCODE) { throw 'Native legacy runtime-entry test build failed' }
ninja -C build-umd src/umd/dxvk-umd-runtime-backend-test.exe
if ($LASTEXITCODE) { throw 'Runtime backend descriptor test build failed' }
ninja -C build-umd src/umd/dxvk-umd-vertex-input-test.exe
if ($LASTEXITCODE) { throw 'Vertex input equality test build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-device-test.exe
if ($LASTEXITCODE) { throw 'Typed D3D9 device lifecycle test build failed' }
ninja -C build-umd src/umd/dxvk-umd-native-entry-test.exe src/umd/dxvk-umd-native-lifetime-test.exe src/umd/dxvk-umd-runtime-gpu-test.exe src/umd/dxvk-umd-predication-test.exe src/umd/dxvk-umd-stream-output-test.exe src/umd/dxvk-umd-texture1d-test.exe
if ($LASTEXITCODE) { throw 'Production native entry/lifetime fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-rotation-test.exe
if ($LASTEXITCODE) { throw 'Production DXGI rotation fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-residency-transaction-test.exe src/umd/dxvk-umd-dxgi-residency-test.exe
if ($LASTEXITCODE) { throw 'Native DXGI residency and priority fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-allocation-terminal-test.exe
if ($LASTEXITCODE) { throw 'Terminal modern allocation fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-dxgi-shared-resolve-test.exe
if ($LASTEXITCODE) { throw 'Native DXGI shared-resource handoff fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-dxgi-blt-test.exe
if ($LASTEXITCODE) { throw 'Typed DXGI Blt fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-primary-policy-test.exe src/umd/dxvk-umd-dxgi-primary-test.exe src/umd/dxvk-umd-dxgi-extended-blt-test.exe src/umd/dxvk-umd-dxgi-extended-primary-test.exe
if ($LASTEXITCODE) { throw 'Typed DXGI primary/display fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-open-primary-policy-test.exe src/umd/dxvk-umd-dxgi-open-primary-test.exe
if ($LASTEXITCODE) { throw 'Typed DXGI opened primary/staging fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-volume-policy-test.exe src/umd/dxvk-umd-texture3d-test.exe
if ($LASTEXITCODE) { throw 'Native volume policy and Texture3D fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-srv-range-test.exe src/umd/dxvk-umd-tex2d-srv-remaining-test.exe
if ($LASTEXITCODE) { throw 'Texture2D SRV remaining-range fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-texturecube-test.exe src/umd/dxvk-umd-cube-array-policy-test.exe src/umd/dxvk-umd-cube-array-resource-test.exe src/umd/dxvk-umd-cube-srv-mips-test.exe src/umd/dxvk-umd-cube-array-mips-test.exe
if ($LASTEXITCODE) { throw 'Native cube resource, view and mip fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-cube-array-targets-test.exe
if ($LASTEXITCODE) { throw 'Native cube-array RTV/DSV fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d11-device-test.exe src/umd/dxvk-umd-compute-container-test.exe src/umd/dxvk-umd-sm5-container-test.exe src/umd/dxvk-umd-legacy-api-test.exe src/umd/dxvk-umd-d3d8-sm1-test.exe
if ($LASTEXITCODE) { throw 'Typed DX10/DX11 and DX8/SM5 compiler fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-uav-texture-policy-test.exe src/umd/dxvk-umd-texture-uav-test.exe
if ($LASTEXITCODE) { throw 'Texture UAV dimension policy and typed compute/clear fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-copy-format-test.exe src/umd/dxvk-umd-resource-copy-cast-test.exe
if ($LASTEXITCODE) { throw 'Resource copy format policy and typed bit-copy fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-bc-transfer-policy-test.exe src/umd/dxvk-umd-bc-update-test.exe
if ($LASTEXITCODE) { throw 'BC block-transfer policy and typed update fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-shader10-profile-test.exe src/umd/dxvk-umd-d3d10-system-shader-test.exe
if ($LASTEXITCODE) { throw 'D3D10 system shader profile and typed FXC fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d10-depth-test.exe
if ($LASTEXITCODE) { throw 'D3D10 depth and null-pixel-shader fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-distance-stream-policy-test.exe src/umd/dxvk-umd-d3d10-distance-stream-test.exe
if ($LASTEXITCODE) { throw 'Clip/cull distance Stream Output policy and typed fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-dxgi-optional-shared-primary-test.exe
if ($LASTEXITCODE) { throw 'Optional shared primary typed fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-msaa-copy-policy-test.exe src/umd/dxvk-umd-msaa-copy-test.exe
if ($LASTEXITCODE) { throw 'MSAA color region copy policy and typed fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-bc-copy-policy-test.exe src/umd/dxvk-umd-bc-copy-test.exe
if ($LASTEXITCODE) { throw 'BC regional copy policy and typed fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-depth-stencil-copy-test.exe
if ($LASTEXITCODE) { throw 'Packed depth/stencil regional copy fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d11-compute-probe.exe src/umd/dxvk-umd-compute-oracle-test.exe
if ($LASTEXITCODE) { throw 'Typed D3D11 compute probe/oracle failed to build' }
ninja -C build-umd src/umd/dxvk-umd-d3d11-cube-probe.exe src/umd/dxvk-umd-cube-probe-oracle-test.exe
if ($LASTEXITCODE) { throw 'Typed D3D11 cube probe/oracle failed to build' }
ninja -C build-umd src/umd/dxvk-umd-d3d11-so-probe.exe src/umd/dxvk-umd-d3d11-volume-probe.exe src/umd/dxvk-umd-so-oracle-test.exe src/umd/dxvk-umd-volume-probe-oracle-test.exe
if ($LASTEXITCODE) { throw 'Typed D3D11 SO and volume probes/oracles failed to build' }
ninja -C build-umd src/umd/dxvk-umd-private-children-test.exe src/umd/dxvk-umd-input-formats-test.exe src/umd/dxvk-umd-d3d10-formats-test.exe src/umd/dxvk-umd-multisample-policy-test.exe src/umd/dxvk-umd-d3d9-buffer-copy-test.exe
if ($LASTEXITCODE) { throw 'Native child/input/format and D3D9 copy fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d8-system-identity-test.exe
if ($LASTEXITCODE) { throw 'Shared system D3D8 image identity fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-runtime-callbacks-test.exe
if ($LASTEXITCODE) { throw 'Probe-only D3D9 callback fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-sm41-container-test.exe src/umd/dxvk-umd-d3d10-shader-test.exe
if ($LASTEXITCODE) { throw 'Native D3D10.1 shader compiler and rendering fixture build failed' }
ninja -C build-umd "src/umd/$LibraryName.dll.p/umd_ddi.cpp.obj" src/umd/dxvk-umd-ddi-probe.exe.p/.._.._tests_umd-ddi-probe.cpp.obj
if ($LASTEXITCODE) { throw 'Early UMD/DDI compile checks failed' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
# Reuse the exact runner already exercised by native target CPU builds. Keep
# the original bytes with failure diagnostics before compiling the C# type.
$runnerSource = Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
$runnerHash = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
$retainedRunnerSource = Join-Path $OutputDirectory 'owned-raw-process-original.cs.txt'
Copy-Item -LiteralPath $runnerSource -Destination $retainedRunnerSource
if ((Get-FileHash -LiteralPath $retainedRunnerSource -Algorithm SHA256).Hash.ToLowerInvariant() -cne $runnerHash) {
    throw 'Fixture runner differs from the native-tested original source'
}
$runnerReceipt = [ordered]@{source='scripts/owned-raw-process-f4bf37f-02.cs';sha256=$runnerHash;
    retained_member='owned-raw-process-original.cs.txt';type_compiled=$false;
    deadline_ms=30000;kill_wait_ms=5000;combined_pipe_drain_ms=20000;
    powershell_version=$PSVersionTable.PSVersion.ToString();clr_version=[Environment]::Version.ToString()}
$runnerReceipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'native-fixture-runner-source.json') -Encoding UTF8
Add-Type -TypeDefinition ([IO.File]::ReadAllText($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($retainedRunnerSource)))
$runnerReceipt.type_compiled = $true
$runnerReceipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'native-fixture-runner-source.json') -Encoding UTF8
function Invoke-BoundedFixture([string]$Executable, [string]$Name, [string]$WorkingDirectory='') {
    $out = Join-Path $OutputDirectory "$Name.txt"
    $err = Join-Path $OutputDirectory "$Name.stderr.txt"
    $executablePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Executable)
    $stdoutPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($out)
    $stderrPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($err)
    $childDirectory = $PWD.ProviderPath
    if ($WorkingDirectory) { $childDirectory = (Resolve-Path -LiteralPath $WorkingDirectory).Path }
    $process = [DxvkRawProcessF4_02]::Run($executablePath, '', $childDirectory, $stdoutPath, $stderrPath, 30000)
    [ordered]@{name=$Name;executable=$executablePath;arguments='';working_directory=$childDirectory;
        runner_sha256=$runnerHash;stdout_member="$Name.txt";stderr_member="$Name.stderr.txt";
        deadline_ms=30000;expected_exit=0;pid=$process.Pid;start_utc=$process.StartUtc;
        retained_process_handle=$process.ProcessHandle;exited=$process.Exited;
        exit_code_available=$process.ExitCodeAvailable;
        exit_code=$(if ($process.ExitCodeAvailable) { $process.ExitCode } else { $null });
        timed_out=$process.TimedOut;child_still_running=$process.ChildStillRunning;
        pipes_drained=$process.PipesDrained;stdout_bytes=$process.StdoutBytes;stderr_bytes=$process.StderrBytes;
        seconds=$process.Seconds;capture_failure=$process.Failure
    } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory "$Name.process.json") -Encoding UTF8
    if (Test-Path -LiteralPath $out -PathType Leaf) { Get-Content -LiteralPath $out }
    if ($process.TimedOut) { throw "$Name exceeded its 30-second deadline" }
    if ($process.Failure -or !$process.Exited -or !$process.ExitCodeAvailable -or
        !$process.PipesDrained -or $process.ChildStillRunning -or !$process.ProcessHandle) {
        throw "$Name lost its process exit or output capture: $($process.Failure)"
    }
    if ($process.ExitCode -ne 0) {
        throw "$Name failed or lost its exit status: $(Get-Content -LiteralPath $err -Raw)"
    }
}
if ($arch -ne 'arm64') {
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-runtime-backend-test.exe runtime-backend-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-vertex-input-test.exe vertex-input-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-rotation-test.exe rotation-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-runtime-gpu-test.exe runtime-gpu-test
    & build-umd/src/umd/dxvk-umd-identity-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'runtime-query-test.txt')
    if ($LASTEXITCODE) { throw 'Runtime identity callback consumer test failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-adapter-test.exe adapter-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-adapter-test.exe d3d9-adapter-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-public-adapter-test.exe d3d9-public-adapter-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-device-test.exe d3d9-device-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-entry-test.exe native-entry-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-lifetime-test.exe native-lifetime-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-residency-transaction-test.exe residency-transaction-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'residency-transaction-test.txt') -Raw) -notmatch '(?m)^DXGI residency transaction PASS checks=102085\r?$') {
        throw 'DXGI residency transaction marker mismatch'
    }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-residency-test.exe dxgi-residency-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'dxgi-residency-test.txt') -Raw) -notmatch '(?m)^DXGI residency/priority PASS checks=\d+ profiles=6 residency=108 priority=48 releases=36\r?$') {
        throw 'Typed DXGI residency/priority marker mismatch'
    }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-allocation-terminal-test.exe allocation-terminal-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'allocation-terminal-test.txt') -Raw) -notmatch '(?m)^DXGI terminal allocations PASS checks=\d+ owned=10 opened=1 transfers=4 attempts=10 released=9 failures=1 locks=3 unlocks=2 retired_lock_returns=1\r?$') {
        throw 'Terminal modern allocation marker mismatch'
    }
    $resolveOriginals = Join-Path $OutputDirectory 'dxgi-shared-resolve-originals'
    New-Item -ItemType Directory -Path $resolveOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-shared-resolve-test.exe dxgi-shared-resolve-test $resolveOriginals
    & python (Join-Path $PSScriptRoot 'verify-native-shared-resolve-originals.py') $resolveOriginals --stdout (Join-Path $OutputDirectory 'dxgi-shared-resolve-test.txt') --output (Join-Path $OutputDirectory 'dxgi-shared-resolve-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent shared-resource handoff originals failed' }
    $bltOriginals = Join-Path $OutputDirectory 'dxgi-blt-originals'
    New-Item -ItemType Directory -Path $bltOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-blt-test.exe dxgi-blt-test $bltOriginals
    & python (Join-Path $PSScriptRoot 'verify-native-blt-originals.py') $bltOriginals --stdout (Join-Path $OutputDirectory 'dxgi-blt-test.txt') --output (Join-Path $OutputDirectory 'dxgi-blt-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent typed DXGI Blt originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-primary-policy-test.exe primary-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'primary-policy-test.txt') -Raw) -notmatch '(?m)^DXGI primary policy PASS checks=6775\r?$') {
        throw 'Primary policy fixture did not pass'
    }
    $primaryOriginals = Join-Path $OutputDirectory 'dxgi-primary-originals'
    New-Item -ItemType Directory -Path $primaryOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-primary-test.exe dxgi-primary-test $primaryOriginals
    & python (Join-Path $PSScriptRoot 'verify-native-primary-originals.py') $primaryOriginals --stdout (Join-Path $OutputDirectory 'dxgi-primary-test.txt') --output (Join-Path $OutputDirectory 'dxgi-primary-originals-verified.json')
    if ($LASTEXITCODE) { throw 'DXGI primary original pixel verification failed' }
    $extendedBlt = Join-Path $OutputDirectory 'dxgi-extended-blt-originals'
    $extendedPrimary = Join-Path $OutputDirectory 'dxgi-extended-primary-originals'
    New-Item -ItemType Directory -Path $extendedBlt,$extendedPrimary -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-extended-blt-test.exe dxgi-extended-blt-test $extendedBlt
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-extended-primary-test.exe dxgi-extended-primary-test $extendedPrimary
    & python (Join-Path $PSScriptRoot 'verify-native-extended-format-originals.py') $extendedBlt $extendedPrimary --blt-stdout (Join-Path $OutputDirectory 'dxgi-extended-blt-test.txt') --primary-stdout (Join-Path $OutputDirectory 'dxgi-extended-primary-test.txt') --output (Join-Path $OutputDirectory 'dxgi-extended-format-originals-verified.json')
    if ($LASTEXITCODE) { throw 'DXGI extended-format original verification failed' }

    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-open-primary-policy-test.exe open-primary-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'open-primary-policy-test.txt') -Raw) -notmatch '(?m)^Opened primary policy PASS checks=6760\r?$') {
        throw 'Opened primary policy fixture did not pass'
    }
    $openPrimaryOriginals = Join-Path $OutputDirectory 'dxgi-open-primary-originals'
    New-Item -ItemType Directory -Path $openPrimaryOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-open-primary-test.exe dxgi-open-primary-test $openPrimaryOriginals
    & python (Join-Path $PSScriptRoot 'verify-native-open-primary-originals.py') $openPrimaryOriginals --stdout (Join-Path $OutputDirectory 'dxgi-open-primary-test.txt') --output (Join-Path $OutputDirectory 'dxgi-open-primary-originals-verified.json')
    if ($LASTEXITCODE) { throw 'DXGI opened primary original pixel/padding verification failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-predication-test.exe predication-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-stream-output-test.exe stream-output-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-texture1d-test.exe texture1d-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-volume-policy-test.exe volume-policy-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-srv-range-test.exe srv-range-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'srv-range-test.txt') -Raw) -notmatch '(?m)^native SRV remaining range policy verified checks=436737\r?$') {
        throw 'SRV remaining-range policy fixture did not pass'
    }
    $srvOriginals = Join-Path $OutputDirectory 'tex2d-srv-remaining-originals'
    New-Item -ItemType Directory -Path $srvOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-tex2d-srv-remaining-test.exe tex2d-srv-remaining-test $srvOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'tex2d-srv-remaining-test.txt') -Raw) -notmatch '(?m)^native D3D10/D3D10\.1/D3D11 Texture2D SRV remaining ranges verified checks=\d+ views=504 words=5184 callbacks=63 WARP controls\r?$') {
        throw 'Texture2D SRV remaining-range fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-tex2d-srv-remaining-originals.py') --directory $srvOriginals --output (Join-Path $OutputDirectory 'tex2d-srv-remaining-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent Texture2D SRV remaining-range originals failed' }
    $textureOriginals = Join-Path $OutputDirectory 'texture3d-originals'
    New-Item -ItemType Directory -Path $textureOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-texture3d-test.exe texture3d-test $textureOriginals
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-cube-array-policy-test.exe cube-array-policy-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-cube-probe-oracle-test.exe cube-probe-oracle-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-so-oracle-test.exe so-oracle-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-volume-probe-oracle-test.exe volume-probe-oracle-test
    foreach ($fixture in @('texturecube', 'cube-array-resource', 'cube-srv-mips', 'cube-array-mips', 'cube-array-targets')) {
        $cubeOriginals = Join-Path $OutputDirectory "$fixture-originals"
        New-Item -ItemType Directory -Path $cubeOriginals -ErrorAction Stop | Out-Null
        Invoke-BoundedFixture "build-umd/src/umd/dxvk-umd-$fixture-test.exe" "$fixture-test" $cubeOriginals
        if ($fixture -ceq 'texturecube') {
            & python (Join-Path $PSScriptRoot 'verify-native-cube-originals.py') $cubeOriginals --output (Join-Path $OutputDirectory 'texturecube-originals-verified.json')
            if ($LASTEXITCODE) { throw 'Independent original cube production oracle failed' }
            & python (Join-Path $PSScriptRoot 'verify-native-cube-public-mips-originals.py') $cubeOriginals --stdout (Join-Path $OutputDirectory 'texturecube-test.txt') --output (Join-Path $OutputDirectory 'texturecube-public-mips-originals-verified.json')
        } elseif ($fixture -ceq 'cube-array-resource') {
            & python (Join-Path $PSScriptRoot 'verify-native-cube-array-resource-originals.py') $cubeOriginals --stdout (Join-Path $OutputDirectory 'cube-array-resource-test.txt') --output (Join-Path $OutputDirectory 'cube-array-resource-originals-verified.json')
        } elseif ($fixture -ceq 'cube-srv-mips') {
            & python (Join-Path $PSScriptRoot '../tests/verify-cube-srv-mips-originals.py') $cubeOriginals --stdout (Join-Path $OutputDirectory 'cube-srv-mips-test.txt') | Set-Content (Join-Path $OutputDirectory 'cube-srv-mips-originals-verified.json') -Encoding UTF8
        } elseif ($fixture -ceq 'cube-array-targets') {
            & python (Join-Path $PSScriptRoot '../tests/verify-cube-array-target-originals.py') --originals $cubeOriginals --stdout (Join-Path $OutputDirectory 'cube-array-targets-test.txt') --output (Join-Path $OutputDirectory 'cube-array-targets-originals-verified.json')
        } else {
            & python (Join-Path $PSScriptRoot 'verify-native-cube-array-mips-originals.py') $cubeOriginals --output (Join-Path $OutputDirectory 'cube-array-mips-originals-verified.json')
        }
        if ($LASTEXITCODE) { throw "Independent original cube oracle failed: $fixture" }
    }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d11-device-test.exe d3d11-device-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-uav-texture-policy-test.exe uav-texture-policy-test
    $textureUavOriginals = Join-Path $OutputDirectory 'texture-uav-originals'
    New-Item -ItemType Directory -Path $textureUavOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-texture-uav-test.exe texture-uav-test $textureUavOriginals
    & python (Join-Path $PSScriptRoot '../tests/verify-texture-uav-originals.py') --directory $textureUavOriginals --output (Join-Path $OutputDirectory 'texture-uav-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent typed texture UAV descriptor/compute/clear originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-copy-format-test.exe copy-format-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'copy-format-test.txt') -Raw) -notmatch '(?m)^Copy format policy passed: 74036 checks, 36864 format pairs\r?$') {
        throw 'Resource copy format policy fixture did not pass'
    }
    $copyCastOriginals = Join-Path $OutputDirectory 'resource-copy-cast-originals'
    New-Item -ItemType Directory -Path $copyCastOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-resource-copy-cast-test.exe resource-copy-cast-test $copyCastOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'resource-copy-cast-test.txt') -Raw) -notmatch '(?m)^Resource copy cast passed: \d+ checks, 320 observations, 134624 bytes each native/public, 92 rejections\r?$') {
        throw 'Typed resource copy cast fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-resource-copy-cast-originals.py') --directory $copyCastOriginals --output (Join-Path $OutputDirectory 'resource-copy-cast-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent resource copy cast native/public originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-bc-transfer-policy-test.exe bc-transfer-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'bc-transfer-policy-test.txt') -Raw) -notmatch '(?m)^PASS BC block upload policy: checks=5780432\r?$') {
        throw 'BC block-transfer policy fixture did not pass'
    }
    $bcUpdateOriginals = Join-Path $OutputDirectory 'bc-update-originals'
    New-Item -ItemType Directory -Path $bcUpdateOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-bc-update-test.exe bc-update-test $bcUpdateOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'bc-update-test.txt') -Raw) -notmatch '(?m)^PASS native BC1-5 updates: uploads=552 noops=136 rejects=408 snapshots=840 bytes=130944 checks=\d+\r?$') {
        throw 'Typed BC1-5 update fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-bc-update-originals.py') --directory $bcUpdateOriginals --output (Join-Path $OutputDirectory 'bc-update-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent BC1-5 native/public update originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-shader10-profile-test.exe shader10-profile-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'shader10-profile-test.txt') -Raw) -notmatch '(?m)^D3D10 shader profile PASS checks=223 retained_tokens=1 hardware_admission=0\r?$') {
        throw 'D3D10 system shader profile fixture did not pass'
    }
    $systemShaderOriginals = Join-Path $OutputDirectory 'd3d10-system-shader-originals'
    New-Item -ItemType Directory -Path $systemShaderOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d10-system-shader-test.exe d3d10-system-shader-test $systemShaderOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'd3d10-system-shader-test.txt') -Raw) -notmatch '(?m)^D3D10 system shader PASS checks=\d+ scenes=4 pixels=1024 words=9216 original_frames=24 fxc_programs=12 hardware_admission=0\r?$') {
        throw 'D3D10 system shader FXC fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-d3d10-system-shader-originals.py') --directory $systemShaderOriginals --output (Join-Path $OutputDirectory 'd3d10-system-shader-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent D3D10 system shader DXBC/token/readback originals failed' }
    $depthOriginals = Join-Path $OutputDirectory 'd3d10-depth-originals'
    New-Item -ItemType Directory -Path $depthOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d10-depth-test.exe d3d10-depth-test $depthOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'd3d10-depth-test.txt') -Raw) -notmatch '(?m)^D3D10 depth PASS checks=\d+ scenes=12 pixels=3072 words=3072 original_frames=24 queries=24 fxc_programs=8 hardware_admission=0\r?$') {
        throw 'D3D10 depth and null-pixel-shader fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-d3d10-depth-originals.py') --directory $depthOriginals --output (Join-Path $OutputDirectory 'd3d10-depth-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent D3D10 depth DXBC/token/plane/query originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-distance-stream-policy-test.exe distance-stream-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'distance-stream-policy-test.txt') -Raw) -notmatch '(?m)^distance stream policy PASS checks=108 packed_semantics=1 full_union_ordinal=1 hardware_admission=0\r?$') {
        throw 'Clip/cull distance Stream Output policy fixture did not pass'
    }
    $distanceStreamOriginals = Join-Path $OutputDirectory 'd3d10-distance-stream-originals'
    New-Item -ItemType Directory -Path $distanceStreamOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d10-distance-stream-test.exe d3d10-distance-stream-test $distanceStreamOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'd3d10-distance-stream-test.txt') -Raw) -notmatch '(?m)^D3D10 distance SO PASS checks=\d+ scenes=16 words=1024 original_buffers=32 query_frames=32 fxc_programs=6 hardware_admission=0\r?$') {
        throw 'D3D10 clip/cull distance Stream Output fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-d3d10-distance-stream-originals.py') --directory $distanceStreamOriginals --output (Join-Path $OutputDirectory 'd3d10-distance-stream-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent D3D10 clip/cull distance DXBC/token/SO/query originals failed' }
    $optionalSharedPrimaryOriginals = Join-Path $OutputDirectory 'dxgi-optional-shared-primary-originals'
    New-Item -ItemType Directory -Path $optionalSharedPrimaryOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-dxgi-optional-shared-primary-test.exe dxgi-optional-shared-primary-test $optionalSharedPrimaryOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'dxgi-optional-shared-primary-test.txt') -Raw) -notmatch '(?m)^DXGI optional shared primary PASS checks=\d+ profiles=3 formats=2 images=24 pixels=840 negatives=12 raw_files=120 hardware_admission=0\r?$') {
        throw 'Optional shared primary fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-optional-shared-primary-originals.py') --directory $optionalSharedPrimaryOriginals --output (Join-Path $OutputDirectory 'dxgi-optional-shared-primary-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent optional shared primary typed/public/kernel originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-msaa-copy-policy-test.exe msaa-copy-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'msaa-copy-policy-test.txt') -Raw) -notmatch '(?m)^MSAA color copy policy PASS checks=236 single_quality_ignored=1 hardware_admission=0\r?$') {
        throw 'MSAA color region copy policy fixture did not pass'
    }
    $msaaCopyOriginals = Join-Path $OutputDirectory 'msaa-copy-originals'
    New-Item -ItemType Directory -Path $msaaCopyOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-msaa-copy-test.exe msaa-copy-test $msaaCopyOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'msaa-copy-test.txt') -Raw) -notmatch '(?m)^MSAA color copy PASS checks=\d+ profiles=2 scenes=36 copies=32 snapshots=204 bytes=78336 rejections=84 noops=24 raw_files=612 hardware_admission=0\r?$') {
        throw 'MSAA color region copy fixture did not pass'
    }
    & python (Join-Path $PSScriptRoot '../tests/verify-msaa-copy-originals.py') --directory $msaaCopyOriginals --output (Join-Path $OutputDirectory 'msaa-copy-originals-verified.json')
    if ($LASTEXITCODE) { throw 'Independent MSAA color region copy typed/public originals failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-bc-copy-policy-test.exe bc-copy-policy-test
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'bc-copy-policy-test.txt') -Raw) -notmatch '(?m)^BC regional copy policy PASS checks=164777 edge_blocks=1 hardware_admission=0\r?$') {
        throw 'BC regional copy policy fixture did not pass'
    }
    $bcCopyOriginals = Join-Path $OutputDirectory 'bc-copy-originals'
    New-Item -ItemType Directory -Path $bcCopyOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-bc-copy-test.exe bc-copy-test $bcCopyOriginals
    if ((Get-Content -LiteralPath (Join-Path $OutputDirectory 'bc-copy-test.txt') -Raw) -notmatch '(?m)^BC regional copy PASS checks=\d+ profiles=2 scenes=20 copies=160 rejections=400 noops=120 snapshots=180 subresources=3600 bytes=237312 raw_files=540 hardware_admission=0\r?$') {
        throw 'BC regional copy native fixture did not pass'
    }
    $bcCopyReview = Join-Path $OutputDirectory 'bc-copy-originals-verified.json'
    & python (Join-Path $PSScriptRoot '../tests/verify-bc-copy-originals.py') --directory $bcCopyOriginals --output $bcCopyReview
    if ($LASTEXITCODE) { throw 'Independent BC regional copy encoded block originals failed' }
    $bcCopyVerified = Get-Content -LiteralPath $bcCopyReview -Raw | ConvertFrom-Json
    if ($bcCopyVerified.verified -ne $true -or $bcCopyVerified.raw_files -ne 540 -or $bcCopyVerified.byte_observations -ne 474624 -or $bcCopyVerified.snapshots -ne 180 -or $bcCopyVerified.subresources -ne 3600 -or $bcCopyVerified.hardware_admission -ne $false -or $bcCopyVerified.registration -ne $false) {
        throw 'BC regional copy independent reader totals or admission flags changed'
    }
    $depthCopyOriginals = Join-Path $OutputDirectory 'depth-stencil-copy-originals'
    New-Item -ItemType Directory -Path $depthCopyOriginals -ErrorAction Stop | Out-Null
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-depth-stencil-copy-test.exe depth-stencil-copy-test $depthCopyOriginals
    $depthCopyStdout = Join-Path $OutputDirectory 'depth-stencil-copy-test.txt'
    if ((Get-Content -LiteralPath $depthCopyStdout -Raw) -notmatch '(?m)^Depth stencil regional copy PASS checks=[1-9][0-9]* profiles=3 families=2 copies=16 rejections=74 noops=12 snapshots=50 pixels=4100 bytes_each_native_public=24600 raw_files=150 hardware_admission=0\r?$') {
        throw 'Packed depth/stencil regional copy fixture did not pass'
    }
    $depthCopyReview = Join-Path $OutputDirectory 'depth-stencil-copy-originals-verified.json'
    & python (Join-Path $PSScriptRoot '../tests/verify-depth-stencil-copy-originals.py') --directory $depthCopyOriginals --stdout $depthCopyStdout --output $depthCopyReview
    if ($LASTEXITCODE) { throw 'Independent packed depth/stencil native/public storage originals failed' }
    $depthCopyVerified = Get-Content -LiteralPath $depthCopyReview -Raw | ConvertFrom-Json
    if ($depthCopyVerified.verified -ne $true -or $depthCopyVerified.profiles -ne 3 -or $depthCopyVerified.families -ne 2 -or $depthCopyVerified.copies -ne 16 -or $depthCopyVerified.rejections -ne 74 -or $depthCopyVerified.noops -ne 12 -or $depthCopyVerified.snapshots -ne 50 -or $depthCopyVerified.pixels -ne 4100 -or $depthCopyVerified.bytes_each_native_public -ne 24600 -or $depthCopyVerified.byte_observations -ne 49200 -or $depthCopyVerified.raw_files -ne 150 -or $depthCopyVerified.hardware_admission -ne $false -or $depthCopyVerified.registration -ne $false) {
        throw 'Packed depth/stencil independent reader totals or admission flags changed'
    }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-compute-container-test.exe compute-container-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-compute-oracle-test.exe compute-oracle-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-sm5-container-test.exe sm5-container-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-legacy-api-test.exe legacy-api-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d8-sm1-test.exe d3d8-sm1-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d8-system-identity-test.exe d3d8-system-identity-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-private-children-test.exe private-children-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-input-formats-test.exe input-formats-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d10-formats-test.exe d3d10-formats-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-multisample-policy-test.exe multisample-policy-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-buffer-copy-test.exe d3d9-buffer-copy-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-runtime-callbacks-test.exe d3d9-runtime-callbacks-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-sm41-container-test.exe sm41-container-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d10-shader-test.exe d3d10-shader-test
    & build-umd/src/umd/dxvk-umd-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'query-test.txt')
    if ($LASTEXITCODE) { throw 'Query completion test failed' }
    & build-umd/src/umd/dxvk-umd-allocation-test.exe | Tee-Object (Join-Path $OutputDirectory 'allocation-test.txt')
    if ($LASTEXITCODE) { throw 'Runtime allocation/presentation test failed' }
    & build-umd/src/umd/dxvk-umd-shader-test.exe | Tee-Object (Join-Path $OutputDirectory 'shader-test.txt')
    if ($LASTEXITCODE) { throw 'Native shader reconstruction test failed' }
    & build-umd/src/umd/dxvk-umd-view-test.exe | Tee-Object (Join-Path $OutputDirectory 'view-test.txt')
    if ($LASTEXITCODE) { throw 'Native texture view/range/MSAA test failed' }
    & build-umd/src/umd/dxvk-umd-shared-policy-test.exe | Tee-Object (Join-Path $OutputDirectory 'shared-policy-test.txt')
    if ($LASTEXITCODE) { throw 'Shared-surface refresh/publish policy test failed' }
}
ninja -C build-umd src/umd/dxvk-umd-backend-probe.exe "src/umd/$LibraryName.dll" src/umd/dxvk-umd-ddi-probe.exe src/umd/dxvk-umd-system-runtime-test.exe src/umd/dxvk-umd-predication-probe.exe src/umd/dxvk-umd-stream-output-probe.exe
if ($LASTEXITCODE) { throw 'DXVK UMD development build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-backend-test.exe
if ($LASTEXITCODE) { throw 'Embedded D3D9 rejection test build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-device-probe.exe
if ($LASTEXITCODE) { throw 'Typed D3D9 target lifecycle probe build failed' }
if ($arch -ne 'arm64') {
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-backend-test.exe d3d9-backend-test
}
foreach ($name in @("$LibraryName.dll", 'dxvk-umd-backend-probe.exe', 'dxvk-umd-ddi-probe.exe', 'dxvk-umd-predication-probe.exe', 'dxvk-umd-stream-output-probe.exe', 'dxvk-umd-d3d9-device-probe.exe', 'dxvk-umd-d3d11-compute-probe.exe', 'dxvk-umd-d3d11-cube-probe.exe', 'dxvk-umd-d3d11-so-probe.exe', 'dxvk-umd-d3d11-volume-probe.exe')) {
    $path = Join-Path 'build-umd/src/umd' $name
    $imports = & dumpbin /imports $path | Out-String
    if ($LASTEXITCODE -or $imports -match 'D3D11CreateDevice|D3D11CoreCreateDevice|D3D11CreateDeviceAndSwapChain|Direct3DCreate9|Direct3DCreate9Ex|Direct3DCreate9On12') { throw "Forbidden D3D runtime import: $name" }
    $headers = & dumpbin /headers $path | Out-String
    $machine = if ($arch -eq 'arm64') { 'AA64' } elseif ($arch -eq 'x64') { '8664' } else { '14C' }
    if ($headers -notmatch "$machine machine") { throw "Incorrect architecture for $name" }
    Copy-Item $path $OutputDirectory
    $imports | Set-Content (Join-Path $OutputDirectory "$name.imports.txt")
}
# The linker writes the PDB beside the DLL (/Z7 with /DEBUG:FULL for MSVC);
# ship it with the matching image so a crash in the UMD can be symbolized.
$symbols = Join-Path 'build-umd/src/umd' "$LibraryName.pdb"
if (-not (Test-Path -LiteralPath $symbols -PathType Leaf)) { throw "$LibraryName.dll has no PDB" }
Copy-Item $symbols $OutputDirectory
$exports = & dumpbin /exports "build-umd/src/umd/$LibraryName.dll" | Out-String
if ($exports -notmatch '\bVioGpuDxvkOpenAdapter10_2ForTest\b') { throw 'Missing typed modern development adapter export' }
if ($exports -notmatch 'VioGpuDxvkCreateDdiTestDevice' -or $exports -notmatch '\bVioGpuDxvkQueryVulkanLoader\b' -or $exports -notmatch '\bVioGpuDxvkOpenAdapter9ForTest\b' -or $exports -notmatch '\bOpenAdapter\b' -or $exports -notmatch '\bOpenAdapter10\b' -or $exports -notmatch '\bOpenAdapter10_2\b' -or $exports -match '\bD3D(?:8|9|10|11|12)Create(?:Device|9|8)\b') { throw 'Unexpected native UMD exports' }
if ($exports -notmatch '\bVioGpuDxvkProbeD3D9BackendForTest\b' -or $exports -match '\bDirect3DCreate(?:8|9)(?:Ex|On12)?\b') { throw 'Unexpected embedded D3D9 exports' }
$exports | Set-Content (Join-Path $OutputDirectory 'exports.txt')
foreach ($name in @('dxvk-umd-rotation-test.exe', 'dxvk-umd-native-entry-test.exe', 'dxvk-umd-native-lifetime-test.exe', 'dxvk-umd-allocation-test.exe', 'dxvk-umd-runtime-gpu-test.exe', 'dxvk-umd-system-runtime-test.exe', 'dxvk-umd-predication-test.exe', 'dxvk-umd-stream-output-test.exe', 'dxvk-umd-query-test.exe', 'dxvk-umd-texture1d-test.exe', 'dxvk-umd-volume-policy-test.exe', 'dxvk-umd-texture3d-test.exe', 'dxvk-umd-d3d9-adapter-test.exe', 'dxvk-umd-d3d9-public-adapter-test.exe', 'dxvk-umd-d3d11-device-test.exe', 'dxvk-umd-compute-container-test.exe', 'dxvk-umd-sm5-container-test.exe', 'dxvk-umd-legacy-api-test.exe', 'dxvk-umd-d3d8-sm1-test.exe', 'dxvk-umd-d3d8-system-identity-test.exe', 'dxvk-umd-private-children-test.exe', 'dxvk-umd-input-formats-test.exe', 'dxvk-umd-d3d10-formats-test.exe', 'dxvk-umd-multisample-policy-test.exe', 'dxvk-umd-d3d9-buffer-copy-test.exe', 'dxvk-umd-d3d9-runtime-callbacks-test.exe', 'dxvk-umd-sm41-container-test.exe', 'dxvk-umd-d3d10-shader-test.exe', 'dxvk-umd-residency-transaction-test.exe', 'dxvk-umd-dxgi-residency-test.exe', 'dxvk-umd-allocation-terminal-test.exe', 'dxvk-umd-dxgi-shared-resolve-test.exe', 'dxvk-umd-dxgi-blt-test.exe', 'dxvk-umd-primary-policy-test.exe', 'dxvk-umd-dxgi-primary-test.exe', 'dxvk-umd-open-primary-policy-test.exe', 'dxvk-umd-dxgi-open-primary-test.exe', 'dxvk-umd-uav-texture-policy-test.exe', 'dxvk-umd-texture-uav-test.exe', 'dxvk-umd-copy-format-test.exe', 'dxvk-umd-resource-copy-cast-test.exe', 'dxvk-umd-bc-transfer-policy-test.exe', 'dxvk-umd-bc-update-test.exe', 'dxvk-umd-shader10-profile-test.exe', 'dxvk-umd-d3d10-system-shader-test.exe', 'dxvk-umd-d3d10-depth-test.exe', 'dxvk-umd-distance-stream-policy-test.exe', 'dxvk-umd-d3d10-distance-stream-test.exe', 'dxvk-umd-dxgi-optional-shared-primary-test.exe', 'dxvk-umd-msaa-copy-policy-test.exe', 'dxvk-umd-msaa-copy-test.exe', 'dxvk-umd-bc-copy-policy-test.exe', 'dxvk-umd-bc-copy-test.exe', 'dxvk-umd-depth-stencil-copy-test.exe', 'dxvk-umd-dxgi-extended-blt-test.exe', 'dxvk-umd-dxgi-extended-primary-test.exe')) {
    # Test-only WARP binaries are separate from the production import gate.
    # Include ARM64 fixtures for execution by the target validation owner.
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect fixture architecture: $name" }
    Copy-Item $path $OutputDirectory
}
foreach ($name in @('dxvk-umd-runtime-backend-test.exe', 'dxvk-umd-d3d9-backend-test.exe', 'dxvk-umd-d3d9-device-test.exe', 'dxvk-umd-vertex-input-test.exe', 'dxvk-umd-compute-oracle-test.exe', 'dxvk-umd-cube-probe-oracle-test.exe', 'dxvk-umd-so-oracle-test.exe', 'dxvk-umd-volume-probe-oracle-test.exe')) {
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect fixture architecture: $name" }
    Copy-Item $path $OutputDirectory
}
foreach ($name in @('dxvk-umd-texturecube-test.exe', 'dxvk-umd-cube-array-policy-test.exe', 'dxvk-umd-cube-array-resource-test.exe', 'dxvk-umd-cube-srv-mips-test.exe', 'dxvk-umd-cube-array-mips-test.exe', 'dxvk-umd-cube-array-targets-test.exe', 'dxvk-umd-srv-range-test.exe', 'dxvk-umd-tex2d-srv-remaining-test.exe')) {
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect cube fixture architecture: $name" }
    Copy-Item $path $OutputDirectory
}
if ($arch -ne 'arm64') {
    Invoke-BoundedFixture (Join-Path $OutputDirectory 'dxvk-umd-system-runtime-test.exe') system-runtime-test
}
# Retain actual Meson-generated configuration, not just the requested flag.
$buildOptionsPath = 'build-umd/meson-info/intro-buildoptions.json'
$buildOptions = [IO.File]::ReadAllText((Join-Path $PWD $buildOptionsPath)) | ConvertFrom-Json
foreach ($option in @([ordered]@{name='umd_library_name';value=$LibraryName},
    [ordered]@{name='umd_vulkan_loader';value=$VulkanLoader},
    [ordered]@{name='enable_umd';value=$true})) {
    $actual = @($buildOptions | Where-Object name -CEQ $option.name)
    if ($actual.Count -ne 1 -or $actual[0].value -cne $option.value) { throw "Meson option differs: $($option.name)" }
}
$loaderHeader = 'build-umd/src/vulkan/vulkan_loader_config.h'
$headerText = [IO.File]::ReadAllText((Join-Path $PWD $loaderHeader))
$define = '(?m)^\s*#define\s+DXVK_PRIVATE_VULKAN_LOADER\s+"' + [regex]::Escape($VulkanLoader) + '"\s*$'
if ($headerText -cnotmatch $define) { throw 'Generated Vulkan loader header differs from requested configuration' }
$configurationAfter = @($configurationBefore | ForEach-Object {
    $actualHash = (Get-FileHash -LiteralPath $_.path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -cne $_.sha256 -or (Get-Item -LiteralPath $_.path).Length -ne $_.bytes) {
        throw "Configuration source changed during build: $($_.path)"
    }
    [ordered]@{path=$_.path;bytes=$_.bytes;sha256=$actualHash;matches_before=$true;matches_git=$_.matches_git}
})
$currentCommit = (& git rev-parse HEAD | Out-String).Trim()
if ($LASTEXITCODE -or $currentCommit -cne $buildSourceCommit) { throw 'Source commit changed during build' }
$configurationAfter | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputDirectory 'native-build-source-after.json') -Encoding UTF8
$retainedConfiguration = @([ordered]@{source=$loaderHeader;member='vulkan_loader_config.h'},
    [ordered]@{source=$buildOptionsPath;member='meson-build-options.json'},
    [ordered]@{source='build-umd/meson-info/intro-machines.json';member='meson-build-machines.json'},
    [ordered]@{source='build-umd/compile_commands.json';member='native-compile-commands.json'},
    [ordered]@{source='build-umd/build.ninja';member='native-build.ninja.txt'})
$configurationFiles = @($retainedConfiguration | ForEach-Object {
    Copy-Item -LiteralPath $_.source -Destination (Join-Path $OutputDirectory $_.member)
    [ordered]@{source=$_.source;member=$_.member;bytes=(Get-Item -LiteralPath $_.source).Length;
        sha256=(Get-FileHash -LiteralPath $_.source -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$builtDll = Join-Path 'build-umd/src/umd' "$LibraryName.dll"
$artifactDll = Join-Path $OutputDirectory "$LibraryName.dll"
$dllHash = (Get-FileHash -LiteralPath $builtDll -Algorithm SHA256).Hash.ToLowerInvariant()
if ($dllHash -cne (Get-FileHash -LiteralPath $artifactDll -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Artifact DLL differs from original linked DLL'
}
if ((Get-FileHash -LiteralPath $symbols -Algorithm SHA256).Hash -cne
    (Get-FileHash -LiteralPath (Join-Path $OutputDirectory "$LibraryName.pdb") -Algorithm SHA256).Hash) {
    throw 'Artifact PDB differs from original linked PDB'
}
$widePrivateName = $VulkanLoader -and [Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($builtDll)).Contains($VulkanLoader)
if ($VulkanLoader -and !$widePrivateName) { throw 'Original DLL does not contain its configured private loader name' }
[ordered]@{schema='native-umd-build-configuration-v1';source_commit=$buildSourceCommit;arch=$arch;
    library_name="$LibraryName.dll";vulkan_loader=$VulkanLoader;
    loader_policy=$(if ($VulkanLoader) { 'module-local-private-no-fallback' } else { 'public-search' });
    private_name_present_in_original_dll=[bool]$widePrivateName;configuration_files=$configurationFiles;
    fixture_runner=$runnerReceipt;
    configuration_source_before=$configurationBefore;configuration_source_after=$configurationAfter;
    all_configuration_sources_match_git=(@($configurationBefore | Where-Object { !$_.matches_git }).Count -eq 0);
    dll=[ordered]@{member="$LibraryName.dll";bytes=(Get-Item -LiteralPath $artifactDll).Length;sha256=$dllHash};
    pdb=[ordered]@{member="$LibraryName.pdb";bytes=(Get-Item -LiteralPath $symbols).Length;
        sha256=(Get-FileHash -LiteralPath $symbols -Algorithm SHA256).Hash.ToLowerInvariant()};
    github=[ordered]@{actions=$isGitHubBuild;repository=$env:GITHUB_REPOSITORY;sha=$env:GITHUB_SHA;
        run_id=$env:GITHUB_RUN_ID;run_attempt=$env:GITHUB_RUN_ATTEMPT;job=$env:GITHUB_JOB};
    target_hardware_validation='separate';installation=$false
} | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $OutputDirectory 'native-build-configuration.json') -Encoding UTF8
@"
DXVK_COMMIT=$buildSourceCommit
ARCH=$arch
LIBRARY=$LibraryName.dll
VULKAN_LOADER=$(if ($VulkanLoader) { "$VulkanLoader (private, beside the UMD)" } else { 'winevulkan.dll/vulkan-1.dll search' })
STATUS=DDI development candidate; not registered or installable as the system UMD.
Development DDIs include restricted SM4 VS/GS/PS, GS stream output, SO targets/stats/overflow, DrawAuto and predication. Null-GS passthrough and general shader interfaces remain pending. Same-source WARP and embedded Turnip SO probes verify bytes, append/reset, gaps, split buffers, overflow and predicated DrawAuto pixels; hardware execution remains required.
Occlusion predication uses a synchronous CPU/GPU correctness fallback with a two-second query deadline; no efficient GPU conditional rendering claim. Same-source WARP and embedded Turnip DDI probes cover both outcomes, inversion, query reuse, unbinding and resource operations. Real target execution remains required.
Native OpenAdapter10_2 negotiates exact identity/generation; incomplete production interfaces and feature levels remain unadvertised.
Typed D3D9 adapter development bridge saves the runtime handle/query owner and validates identity, caps buffer sizes, reentry, close and reset. Development CreateDevice constructs the embedded offscreen renderer with copied callbacks and a separate driver token. Typed device DDIs include Flush/DestroyDevice and restricted surface CreateResource/DestroyResource/SetRenderTarget/Clear/Blt/Lock/Unlock, owned vertex declarations, supported render states, viewport/zrange/scissor, stream-zero user memory, nonindexed fast-path draw, owned SM1-3 vertex/pixel shaders and float/int/bool constant uploads. Typed caps expose the implemented static/dynamic A8/X8 2D textures/render targets, D16/D24S8 depth buffers, six query types and a conservative SM2/single-RT profile. Unsupported cube/volume/MSAA/instancing, autogen textures, gamma, sharing and stretched plain-surface operations stay unadvertised. Caps arguments/output ownership are snapshotted before identity callbacks. The normal OpenAdapter and explicit VioGpuDxvkOpenAdapter9ForTest share the bounded Interface8/9 implementation and driver-only FOGINFVF caps. Ordinary Microsoft runtime draw/Present and installed/default acceptance require separate native original evidence.
The private D3D9 translation core is embedded without public Direct3DCreate9 exports/imports. It shares exact-LUID Turnip selection and copied callback ownership with D3D10, requires runtime ownership, and initializes offscreen state without display enumeration, DPI mutation or an implicit swapchain. VioGpuDxvkProbeD3D9BackendForTest is a synchronous construction/teardown helper requiring an active callback dispatcher; negative CI controls are not GPU construction or runtime activation proof. D3D9 surface DDIs support nonshared A8R8G8B8/X8R8G8B8 groups, nonsampled render-target0 binding, distinct computed/preclipped clears, straight blits, readback and CPU views with caller-owned padded system memory. Typed paths also implement static 2D mip chains and state, vertex/index buffers, indexed/multistream draw, depth/stencil, transforms/lights/clip planes and queries. Typed owned-allocation Present is implemented, but raw target presentation diagnostics reject before accepted screen pixels. Shared/opened resources, primary/flip presentation and ordinary runtime admission remain pending. The --render target probe independently requires exact clear/readback pixels, padding, subresource ordering and nonempty runtime render callbacks. The --draw probe adds quad/scissor/partial-color-write pixel checks with vertex-start offset and changing bound user-memory data. The --shader probe additionally requires SM1/2/3 pixel readback and float/int/bool constants used by both shader stages. Native shader entry points carry explicit code bounds through the existing translator; bytecode and constant arrays are copied on the DDI caller before callbacks. Shader tokens are distinct by device/stage, published after identity validation, and flushed/released on the renderer worker. Device/resource fixture substitutes only the backend; real typed adapter/device/RuntimeGpu/callback dispatch and cleanup execute. Nested/concurrent operations return WASSTILLDRAWING and preserve the device for a later retry.
Production entry/lifetime fixtures use controlled callbacks and a WARP backend; they are not ordinary Microsoft runtime activation.
Native resource creation unwinds failed staged owners; destruction retires private storage before callbacks. Allocation identity/reset checks and cleanup fixtures are included.
Exact typed D3D10.0/10.1/11.0 development tables retain live core callbacks and use the runtime DXGI revision. D3D11 UAV/compute/indirect/LOD fixtures and SM5 container controls are included; higher production interfaces remain gated.
Native backend receives copied runtime callbacks before vkCreateDevice; Turnip internal BO allocation/map/submit use one runtime-owned context through private Mesa v1. Actual GPU/system-runtime acceptance pending.
Ordinary native DDI/Present jobs pump RuntimeGpu, runtime allocation and core callbacks on their original DDI caller; CalcPrivate remains concurrent. Worker requests between DDIs wait for the next permitted caller. Native Flush joins command recording and queue submission before returning, without waiting for GPU completion. DestroyDevice drains backend workers and closes allocations/context before return. No post-DestroyDevice runtime lifetime is assumed.
The actual Microsoft runtime test requires candidate activation rejection on CI without VIOGPU and independently validates WARP Draw/readback/Present/immediate teardown. WARP control success is not candidate rendering or native admission. The dispatch integration still needs actual target/runtime proof.
Windowed-blit Present development path uses runtime allocations and synchronized pixel copies; target proof pending.
Registration, ordinary runtime activation, complete required table, sharing and primary/flip Present remain pending.
"@ | Set-Content (Join-Path $OutputDirectory 'STATUS.txt')
Get-ChildItem $OutputDirectory -File | Where-Object Extension -in '.dll','.exe' | Get-FileHash | Format-List
