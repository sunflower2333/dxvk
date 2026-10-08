param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Directory).Path
if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'Arm64' -or
    [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -ne 'Arm64') {
    throw 'These executables require a native ARM64 Windows runner'
}
$status = Get-Content -LiteralPath (Join-Path $root 'STATUS.txt') -Raw
if ($status -notmatch "(?m)^DXVK_COMMIT=$($env:GITHUB_SHA)\r?$" -or $status -notmatch '(?m)^ARCH=arm64\r?$') {
    throw 'Artifact source or architecture does not match this CI run'
}
# Match the native target and x86/x64 harness: own the original process handle
# and drain both raw pipes concurrently before inspecting exit or markers.
$runnerSource = Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
$runnerHash = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
$retainedRunnerSource = Join-Path $root 'arm64-owned-raw-process-original.cs.txt'
if (Test-Path -LiteralPath $retainedRunnerSource) { throw 'ARM64 runner originals already exist' }
Copy-Item -LiteralPath $runnerSource -Destination $retainedRunnerSource
if ((Get-FileHash -LiteralPath $retainedRunnerSource -Algorithm SHA256).Hash.ToLowerInvariant() -cne $runnerHash) {
    throw 'ARM64 fixture runner differs from the native-tested original source'
}
$runnerReceipt = [ordered]@{source='scripts/owned-raw-process-f4bf37f-02.cs';sha256=$runnerHash;
    retained_member='arm64-owned-raw-process-original.cs.txt';type_compiled=$false;
    deadline_ms=30000;kill_wait_ms=5000;combined_pipe_drain_ms=20000;
    powershell_version=$PSVersionTable.PSVersion.ToString();clr_version=[Environment]::Version.ToString()}
$runnerReceipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $root 'arm64-fixture-runner-source.json') -Encoding UTF8
Add-Type -TypeDefinition ([IO.File]::ReadAllText($retainedRunnerSource))
$runnerReceipt.type_compiled = $true
$runnerReceipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $root 'arm64-fixture-runner-source.json') -Encoding UTF8
$cases = [ordered]@{
    'dxvk-umd-so-oracle-test.exe' = 'SO capture oracle verified checks=21083 bit_flips=20480 streams=4 raw_bits=1 callback_controls=55'
    'dxvk-umd-volume-probe-oracle-test.exe' = 'volume probe oracle verified checks=100632 voxels=3046 bit_flips=97472 sampled=551'
    'dxvk-umd-cube-probe-oracle-test.exe' = 'PASS cube probe arithmetic checks=159292 bit_flips=154752 texels=4830 hardware=0'
    'dxvk-umd-vertex-input-test.exe' = 'vertex input equality PASS checks=\d+; colliding layouts retained, no GPU runtime'
    'dxvk-umd-runtime-backend-test.exe' = 'runtime backend ownership PASS checks=\d+; CPU descriptor and lifetime contracts'
    'dxvk-umd-d3d9-backend-test.exe' = 'D3D9 backend rejection PASS checks=\d+; no GPU construction or runtime admission'
    'dxvk-umd-d3d9-adapter-test.exe' = 'native D3D9 adapter PASS checks=\d+; mock runtime, no rendering or admission'
    'dxvk-umd-d3d9-public-adapter-test.exe' = 'native legacy OpenAdapter PASS checks=\d+; Interface8/9 mock runtime, no rendering'
    'dxvk-umd-d3d9-device-test.exe' = 'native D3D9 device PASS checks=\d+; controlled backend, no GPU rendering or runtime admission'
    'dxvk-umd-rotation-test.exe' = 'PASS native DXGI rotation: .*WARP only, admission closed'
    'dxvk-umd-texture1d-test.exe' = 'PASS Texture1D'
    'dxvk-umd-volume-policy-test.exe' = 'PASS volume policy: 385547 checks; independent padded volume and xyz bounds'
    'dxvk-umd-texture3d-test.exe' = 'PASS Texture3D\r?\nprofiles=3\r?\ncases=27\r?\nchecks=\d+\r?\nvoxels=9138\r?\nsampled=945'
    'dxvk-umd-texturecube-test.exe' = 'PASS TextureCube\r?\ncases=8\r?\nchecks=\d+\r?\ntexels=10380\r?\nreadbacks=7'
    'dxvk-umd-cube-array-policy-test.exe' = 'PASS cube-array policy: 97387 checks; independent complete-face and mip bounds'
    'dxvk-umd-cube-array-resource-test.exe' = 'PASS cube-array resource\r?\nprofiles=2\r?\ncases=7\r?\nchecks=\d+\r?\ntexels=3540\r?\nfailures=56'
    'dxvk-umd-cube-srv-mips-test.exe' = '(?m)^native D3D10\.1/D3D11 cube SRV mip ranges verified checks=\d+ views=102 words=2244 callbacks=36 WARP controls\r?$'
    'dxvk-umd-cube-array-mips-test.exe' = 'PASS CubeArrayMips\r?\ncases=5\r?\nchecks=\d+\r?\ntexels=30690\r?\nreadbacks=5'
    'dxvk-umd-cube-array-targets-test.exe' = '(?m)^typed cube-array targets verified checks=\d+ views=24 snapshots=30 words=35700 callbacks=46 public_reference=1 hardware_admission=0\r?$'
    'dxvk-umd-d3d11-device-test.exe' = '(?m)^typed D3D10\.1/D3D11 fixture PASS checks=\d+ callbacks=\d+ SM5 graphics/queries/packed IA/streams/tessellation/classes WARP controls; native Turnip/runtime acceptance remains gated\r?$'
    'dxvk-umd-uav-texture-policy-test.exe' = '(?m)^D3D11 texture UAV shape policy verified checks=49631\r?$'
    'dxvk-umd-texture-uav-test.exe' = '(?m)^D3D11 texture UAVs verified checks=\d+ views=72 words=10752 callbacks=88 original_files=384 WARP controls hardware_admission=0\r?$'
    'dxvk-umd-compute-container-test.exe' = 'compute container PASS checks=\d+ exact tokens/hash and malformed SM5 controls'
    'dxvk-umd-sm5-container-test.exe' = 'SM5 signatures/interfaces PASS checks=\d+ exact tokens/hash, GS streams, patch factors, typed/depth outputs, native table IDs'
    'dxvk-umd-legacy-api-test.exe' = 'legacy8/9 API bounds PASS checks=\d+; renderer framing follows, admission unchanged'
    'dxvk-umd-d3d8-sm1-test.exe' = 'actual DXVK SM1.1/1.4 compiler bridge PASS checks=\d+; no GPU execution'
    'dxvk-umd-d3d8-system-identity-test.exe' = '(?m)^D3D8 system image identity fixture PASS checks=91 runtime_calls=0 KMT_calls=0 core_loads=0\r?$'
    'dxvk-umd-private-children-test.exe' = 'private children PASS checks=\d+; concurrent pins/reuse/rollback/epoch cleanup, no GPU'
    'dxvk-umd-input-formats-test.exe' = 'input formats PASS checks=\d+; scalar/packed offsets/overflow, no GPU'
    'dxvk-umd-d3d10-formats-test.exe' = 'native D3D10/10\.1 format queries verified checks=\d+ callbacks=12 backend_calls=2 hardware_admission=0'
    'dxvk-umd-multisample-policy-test.exe' = 'native multisample output policy verified checks=\d+'
    'dxvk-umd-d3d9-buffer-copy-test.exe' = 'D3D9 vertex copy PASS checks=\d+; bounded source offsets/partial tails/overflow, no GPU'
    'dxvk-umd-d3d9-runtime-callbacks-test.exe' = 'probe D3D9 typed runtime callbacks verified checks=\d+ calls=25 vista_callbacks=22 vista_functions=99 hardware_admission=0'
    'dxvk-umd-sm41-container-test.exe' = 'SM4.0/4.1 containers verified checks=763 typed_models=2 new_opcodes=4 hardware_admission=0'
    'dxvk-umd-d3d10-shader-test.exe' = 'native D3D10/10.1 shaders verified checks=\d+ callbacks=3 draws=18 pixels=4608 hardware_admission=0'
    'dxvk-umd-runtime-gpu-test.exe' = 'PASS \d+ runtime GPU checks'
    'dxvk-umd-native-entry-test.exe' = 'native production entry/lifetime PASS .*complete-contract-fixture=0 backend-calls=0'
    'dxvk-umd-native-lifetime-test.exe' = 'native production entry/lifetime PASS .*complete-contract-fixture=1'
    'dxvk-umd-allocation-test.exe' = 'runtime allocation/presentation PASS checks='
    'dxvk-umd-residency-transaction-test.exe' = '(?m)^DXGI residency transaction PASS checks=102085\r?$'
    'dxvk-umd-dxgi-residency-test.exe' = '(?m)^DXGI residency/priority PASS checks=\d+ profiles=6 residency=108 priority=48 releases=36\r?$'
    'dxvk-umd-allocation-terminal-test.exe' = '(?m)^DXGI terminal allocations PASS checks=\d+ owned=10 opened=1 transfers=4 attempts=10 released=9 failures=1 locks=3 unlocks=2 retired_lock_returns=1\r?$'
    'dxvk-umd-dxgi-shared-resolve-test.exe' = '(?m)^DXGI shared resolve PASS checks=\d+ profiles=3 snapshots=15 pixels=525 hardware_admission=0\r?$'
    'dxvk-umd-dxgi-blt-test.exe' = '(?m)^DXGI Blt PASS checks=\d+ profiles=3 snapshots=30 pixels=1536 hardware_admission=0\r?$'
    'dxvk-umd-primary-policy-test.exe' = '(?m)^DXGI primary policy PASS checks=5637\r?$'
    'dxvk-umd-dxgi-primary-test.exe' = '(?m)^DXGI primary/display PASS checks=\d+ profiles=6 snapshots=24 pixels=768 hardware_admission=0\r?$'
    'dxvk-umd-srv-range-test.exe' = '(?m)^native SRV remaining range policy verified checks=436737\r?$'
    'dxvk-umd-tex2d-srv-remaining-test.exe' = '(?m)^native D3D10/D3D10\.1/D3D11 Texture2D SRV remaining ranges verified checks=\d+ views=504 words=5184 callbacks=63 WARP controls\r?$'
    'dxvk-umd-open-primary-policy-test.exe' = '(?m)^Opened primary policy PASS checks=6760\r?$'
    'dxvk-umd-dxgi-open-primary-test.exe' = '(?m)^DXGI opened primary PASS checks=\d+ profiles=3 formats=3 images=36 pixels=1260 failures=78 callbacks=78 runtime_terminal_releases=4 runtime_terminal_maps=2 hardware_admission=0\r?$'
    'dxvk-umd-predication-test.exe' = 'native predication PASS checks=.*draw-cases=24'
    'dxvk-umd-stream-output-test.exe' = 'native stream output PASS checks=.*drawauto-cases=2'
    'dxvk-umd-query-test.exe' = 'query completion PASS checks='
    'dxvk-umd-system-runtime-test.exe' = 'system runtime control PASS: WARP Draw/readback/Present/immediate teardown'
}
$originalDirectories = @{
    'dxvk-umd-texture-uav-test.exe' = 'arm64-texture-uav-originals'
    'dxvk-umd-tex2d-srv-remaining-test.exe' = 'arm64-tex2d-srv-remaining-originals'
    'dxvk-umd-dxgi-blt-test.exe' = 'arm64-dxgi-blt-originals'
    'dxvk-umd-dxgi-primary-test.exe' = 'arm64-dxgi-primary-originals'
    'dxvk-umd-dxgi-open-primary-test.exe' = 'arm64-dxgi-open-primary-originals'
    'dxvk-umd-dxgi-shared-resolve-test.exe' = 'arm64-dxgi-shared-resolve-originals'
    'dxvk-umd-texture3d-test.exe' = 'arm64-texture3d-originals'
    'dxvk-umd-texturecube-test.exe' = 'arm64-texturecube-originals'
    'dxvk-umd-cube-array-resource-test.exe' = 'arm64-cube-array-resource-originals'
    'dxvk-umd-cube-srv-mips-test.exe' = 'arm64-cube-srv-mips-originals'
    'dxvk-umd-cube-array-mips-test.exe' = 'arm64-cube-array-mips-originals'
    'dxvk-umd-cube-array-targets-test.exe' = 'arm64-cube-array-targets-originals'
}
foreach ($name in $cases.Keys) {
    $exe = Join-Path $root $name
    $bytes = [System.IO.File]::ReadAllBytes($exe)
    $offset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($offset -lt 0 -or $offset -gt $bytes.Length - 24 -or
        [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0xaa64) {
        throw "Wrong PE architecture: $name"
    }
    $out = Join-Path $root "arm64-$name.stdout.txt"
    $err = Join-Path $root "arm64-$name.stderr.txt"
    $childDirectory = $PWD.ProviderPath
    if ($originalDirectories.ContainsKey($name)) {
        $childDirectory = Join-Path $root $originalDirectories[$name]
        New-Item -ItemType Directory -Path $childDirectory -ErrorAction Stop | Out-Null
    }
    $process = [DxvkRawProcessF4_02]::Run($exe, '', $childDirectory, $out, $err, 30000)
    [ordered]@{name=$name;executable=$exe;arguments='';working_directory=$childDirectory;
        runner_sha256=$runnerHash;stdout_member="arm64-$name.stdout.txt";stderr_member="arm64-$name.stderr.txt";
        deadline_ms=30000;expected_exit=0;pid=$process.Pid;start_utc=$process.StartUtc;
        retained_process_handle=$process.ProcessHandle;exited=$process.Exited;
        exit_code_available=$process.ExitCodeAvailable;
        exit_code=$(if ($process.ExitCodeAvailable) { $process.ExitCode } else { $null });
        timed_out=$process.TimedOut;child_still_running=$process.ChildStillRunning;
        pipes_drained=$process.PipesDrained;stdout_bytes=$process.StdoutBytes;stderr_bytes=$process.StderrBytes;
        seconds=$process.Seconds;capture_failure=$process.Failure
    } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $root "arm64-$name.process.json") -Encoding UTF8
    if ($process.TimedOut) {
        throw "$name exceeded its 30-second functional deadline"
    }
    if ($process.Failure -or !$process.Exited -or !$process.ExitCodeAvailable -or
        !$process.PipesDrained -or $process.ChildStillRunning -or !$process.ProcessHandle) {
        throw "$name lost its process exit or output capture: $($process.Failure)"
    }
    $text = Get-Content -LiteralPath $out -Raw
    Write-Host $text
    if ($process.ExitCode -ne 0 -or $text -notmatch $cases[$name]) {
        throw "$name failed: exit=$($process.ExitCode) $(Get-Content -LiteralPath $err -Raw)"
    }
    if ($name -ceq 'dxvk-umd-texture-uav-test.exe') {
        & python (Join-Path $PSScriptRoot '../tests/verify-texture-uav-originals.py') --directory $childDirectory --output (Join-Path $root 'arm64-texture-uav-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 texture UAV descriptor/compute/clear originals failed' }
    } elseif ($name -ceq 'dxvk-umd-dxgi-shared-resolve-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-shared-resolve-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-dxgi-shared-resolve-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 shared-resource handoff originals failed' }
    } elseif ($name -ceq 'dxvk-umd-dxgi-blt-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-blt-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-dxgi-blt-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 typed DXGI Blt originals failed' }
    } elseif ($name -ceq 'dxvk-umd-dxgi-primary-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-primary-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-dxgi-primary-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 typed DXGI primary originals failed' }
    } elseif ($name -ceq 'dxvk-umd-tex2d-srv-remaining-test.exe') {
        & python (Join-Path $PSScriptRoot '../tests/verify-tex2d-srv-remaining-originals.py') --directory $childDirectory --output (Join-Path $root 'arm64-tex2d-srv-remaining-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 Texture2D SRV remaining-range originals failed' }
    } elseif ($name -ceq 'dxvk-umd-dxgi-open-primary-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-open-primary-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-dxgi-open-primary-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 opened primary pixel/padding originals failed' }
    } elseif ($name -ceq 'dxvk-umd-texturecube-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-cube-originals.py') $childDirectory --output (Join-Path $root 'arm64-texturecube-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 cube original oracle failed' }
        & python (Join-Path $PSScriptRoot 'verify-native-cube-public-mips-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-texturecube-public-mips-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 public cube observations failed' }
    } elseif ($name -ceq 'dxvk-umd-cube-array-resource-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-cube-array-resource-originals.py') $childDirectory --stdout $out --output (Join-Path $root 'arm64-cube-array-resource-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 cube-array resource original oracle failed' }
    } elseif ($name -ceq 'dxvk-umd-cube-srv-mips-test.exe') {
        & python (Join-Path $PSScriptRoot '../tests/verify-cube-srv-mips-originals.py') $childDirectory --stdout $out | Set-Content (Join-Path $root 'arm64-cube-srv-mips-originals-verified.json') -Encoding UTF8
        if ($LASTEXITCODE) { throw 'Independent ARM64 cube SRV original oracle failed' }
    } elseif ($name -ceq 'dxvk-umd-cube-array-mips-test.exe') {
        & python (Join-Path $PSScriptRoot 'verify-native-cube-array-mips-originals.py') $childDirectory --output (Join-Path $root 'arm64-cube-array-mips-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 cube-array mip original oracle failed' }
    } elseif ($name -ceq 'dxvk-umd-cube-array-targets-test.exe') {
        & python (Join-Path $PSScriptRoot '../tests/verify-cube-array-target-originals.py') --originals $childDirectory --stdout $out --output (Join-Path $root 'arm64-cube-array-targets-originals-verified.json')
        if ($LASTEXITCODE) { throw 'Independent ARM64 cube-array target original oracle failed' }
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath $exe | Format-List | Out-File -Append (Join-Path $root 'arm64-hashes.txt')
}
