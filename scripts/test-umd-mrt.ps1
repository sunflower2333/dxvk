param(
    [ValidateSet('x64','arm64','x86')]
    [string]$Architecture = 'x64',
    [string]$OutputDirectory = 'artifacts/mrt',
    [switch]$CompileOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Fail immediately on compiler, linker or fixture failure rather than using stale outputs.
function Check-Exit {
    param([string]$Name)
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$target = $env:VSCMD_ARG_TGT_ARCH
if ($target -ne $Architecture) { throw "Expected $Architecture compiler, got $target" }
$wdk = Get-ChildItem -Path ($env:INCLUDE -split ';' | Where-Object { $_ -and (Test-Path $_) }) `
    -Filter d3d10umddi.h -File -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $wdk) { throw 'Selected SDK/WDK does not contain d3d10umddi.h' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Refusing existing output directory' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$out = (Resolve-Path -LiteralPath $OutputDirectory).Path
$shader = @(
    'src/umd/umd_shader.cpp',
    'subprojects/dxbc-spirv/dxbc/dxbc_container.cpp',
    'subprojects/dxbc-spirv/dxbc/dxbc_parser.cpp',
    'subprojects/dxbc-spirv/dxbc/dxbc_signature.cpp',
    'subprojects/dxbc-spirv/dxbc/dxbc_types.cpp',
    'subprojects/dxbc-spirv/ir/ir.cpp',
    'subprojects/dxbc-spirv/util/util_swizzle.cpp',
    'subprojects/dxbc-spirv/util/util_log.cpp',
    'subprojects/dxbc-spirv/util/util_md5.cpp'
) | ForEach-Object { Join-Path $root $_ }
$ddi = @('src/umd/umd_ddi.cpp','src/umd/umd_runtime_query.cpp',
         'src/umd/umd_allocation.cpp','src/umd/umd_runtime_gpu.cpp') | ForEach-Object { Join-Path $root $_ }
Push-Location $out
try {
    $flags = @('/nologo','/std:c++17','/EHsc','/O1','/MD','/bigobj',
               "/I$root/subprojects/dxbc-spirv",'/DNOMINMAX')
    & cl @flags /c @shader @ddi
    Check-Exit 'production DDI and shader objects'
    $shaderObjects = $shader | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) + '.obj' }
    $ddiObjects = $ddi | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) + '.obj' }
    & cl @flags "$root/tests/umd-mrt.cpp" @shaderObjects @ddiObjects d3d11.lib d3dcompiler.lib /Fe:native-mrt.exe
    Check-Exit 'native MRT fixture'
    & cl @flags "$root/tests/umd-mrt-signature.cpp" @shaderObjects /Fe:mrt-signature.exe
    Check-Exit 'actual DXBC signature fixture'
    & cl @flags "$root/tests/umd-output-policy.cpp" /Fe:output-policy.exe
    Check-Exit 'output policy fixture'
    & cl @flags "$root/tests/umd-output-views.cpp" d3d11.lib /Fe:output-views.exe
    Check-Exit 'actual output view metadata fixture'
    & cl @flags "$root/tests/umd-texture1d.cpp" @shaderObjects @ddiObjects d3d11.lib d3dcompiler.lib /Fe:texture1d.exe
    Check-Exit 'actual Texture1D resource/view fixture'
    & cl @flags "$root/tests/umd-texture-transfer.cpp" @shaderObjects @ddiObjects d3d11.lib /Fe:texture-transfer.exe
    Check-Exit 'actual native texture transfer fixture'
    & cl @flags "$root/tests/umd-transfer-policy.cpp" /Fe:transfer-policy.exe
    Check-Exit 'upload arithmetic fixture'
    foreach ($name in @('native-mrt.exe','mrt-signature.exe','output-policy.exe','output-views.exe','texture1d.exe','texture-transfer.exe','transfer-policy.exe')) {
        $headers = (& dumpbin /headers $name | Out-String)
        Check-Exit "PE inspection $name"
        $machine = @{arm64='AA64';x64='8664';x86='14C'}[$Architecture]
        if ($headers -notmatch "$machine machine") { throw "Wrong architecture: $name" }
        if (-not $CompileOnly) {
            $process = Start-Process (Join-Path $out $name) -PassThru `
                -RedirectStandardOutput "$name.stdout.txt" -RedirectStandardError "$name.stderr.txt"
            $null = $process.Handle
            if (-not $process.WaitForExit(60000)) {
                $process.Kill()
                throw "$name exceeded its fixture deadline"
            }
            Get-Content "$name.stdout.txt"
            Get-Content "$name.stderr.txt"
            if ($process.ExitCode -ne 0) { throw "$name failed: $($process.ExitCode)" }
        }
    }
    [ordered]@{
        schema=1; source=$env:GITHUB_SHA; architecture=$Architecture
        executed=(!$CompileOnly); backend='WARP test-only injection into actual native DDI'
        viogpu_gpu_test=$false; native_runtime_admission=$false; installation=$false
    } | ConvertTo-Json | Set-Content -Encoding utf8 receipt.json
}
finally { Pop-Location }
