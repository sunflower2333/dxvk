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
    'dxvk-umd-runtime-gpu-test.exe' = 'PASS \d+ runtime GPU checks'
    'dxvk-umd-native-entry-test.exe' = 'native production entry/lifetime PASS .*complete-contract-fixture=0 backend-calls=0'
    'dxvk-umd-native-lifetime-test.exe' = 'native production entry/lifetime PASS .*complete-contract-fixture=1'
    'dxvk-umd-allocation-test.exe' = 'runtime allocation/presentation PASS checks='
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
    if ($process.ExitCode -ne 0 -or $text -notmatch $cases[$name]) {
        throw "$name failed: exit=$($process.ExitCode) $(Get-Content -LiteralPath $err -Raw)"
    }
    Get-FileHash -Algorithm SHA256 -LiteralPath $exe | Format-List | Out-File -Append (Join-Path $root 'arm64-hashes.txt')
}
