param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Directory).Path
if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'Arm64') {
    throw 'These executables require a native ARM64 Windows runner'
}
$status = Get-Content -LiteralPath (Join-Path $root 'STATUS.txt') -Raw
if ($status -notmatch "(?m)^DXVK_COMMIT=$($env:GITHUB_SHA)\r?$" -or $status -notmatch '(?m)^ARCH=arm64\r?$') {
    throw 'Artifact source or architecture does not match this CI run'
}
$cases = [ordered]@{
    'dxvk-umd-vertex-input-test.exe' = 'vertex input equality PASS checks=\d+; colliding layouts retained, no GPU runtime'
    'dxvk-umd-runtime-backend-test.exe' = 'runtime backend ownership PASS checks=\d+; CPU descriptor and lifetime contracts'
    'dxvk-umd-d3d9-backend-test.exe' = 'D3D9 backend rejection PASS checks=\d+; no GPU construction or runtime admission'
    'dxvk-umd-d3d9-adapter-test.exe' = 'native D3D9 adapter PASS checks=\d+; mock runtime, no rendering or admission'
    'dxvk-umd-d3d9-device-test.exe' = 'native D3D9 device PASS checks=\d+; controlled backend, no GPU rendering or runtime admission'
    'dxvk-umd-rotation-test.exe' = 'PASS native DXGI rotation: .*WARP only, admission closed'
    'dxvk-umd-texture1d-test.exe' = 'PASS Texture1D'
    'dxvk-umd-d3d11-device-test.exe' = '(?m)^typed D3D10\.1/D3D11 fixture PASS checks=\d+ callbacks=\d+ SM5 graphics/queries/packed IA/streams/tessellation/classes WARP controls; native Turnip/runtime acceptance remains gated\r?$'
    'dxvk-umd-compute-container-test.exe' = 'compute container PASS checks=\d+ exact tokens/hash and malformed SM5 controls'
    'dxvk-umd-sm5-container-test.exe' = 'SM5 signatures/interfaces PASS checks=\d+ exact tokens/hash, GS streams, patch factors, typed/depth outputs, native table IDs'
    'dxvk-umd-legacy-api-test.exe' = 'legacy8/9 API bounds PASS checks=\d+; renderer framing follows, admission unchanged'
    'dxvk-umd-d3d8-sm1-test.exe' = 'actual DXVK SM1.1/1.4 compiler bridge PASS checks=\d+; no GPU execution'
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
    'dxvk-umd-predication-test.exe' = 'native predication PASS checks=.*draw-cases=24'
    'dxvk-umd-stream-output-test.exe' = 'native stream output PASS checks=.*drawauto-cases=2'
    'dxvk-umd-query-test.exe' = 'query completion PASS checks='
    'dxvk-umd-system-runtime-test.exe' = 'system runtime control PASS: WARP Draw/readback/Present/immediate teardown'
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
    $process = Start-Process -FilePath $exe -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $null = $process.Handle
    if (!$process.WaitForExit(30000)) {
        $process.Kill(); $process.WaitForExit()
        throw "$name exceeded its 30-second functional deadline"
    }
    $text = Get-Content -LiteralPath $out -Raw
    Write-Host $text
    if ($null -eq $process.ExitCode -or $process.ExitCode -ne 0 -or $text -notmatch $cases[$name]) {
        throw "$name failed: exit=$($process.ExitCode) $(Get-Content -LiteralPath $err -Raw)"
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath $exe | Format-List | Out-File -Append (Join-Path $root 'arm64-hashes.txt')
}
