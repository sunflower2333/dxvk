param([string]$OutputDirectory = 'artifacts/umd')
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
meson setup build-umd --cross-file native-umd-cross.ini --buildtype release -Denable_umd=true -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false
if ($LASTEXITCODE) { throw 'Meson configure failed' }
ninja -C build-umd src/umd/dxvk-umd-identity-query-test.exe src/umd/dxvk-umd-adapter-test.exe src/umd/dxvk-umd-query-test.exe src/umd/dxvk-umd-allocation-test.exe src/umd/dxvk-umd-shader-test.exe src/umd/dxvk-umd-view-test.exe
if ($LASTEXITCODE) { throw 'Runtime adapter CPU test build failed' }
ninja -C build-umd src/umd/dxvk-umd-native-entry-test.exe src/umd/dxvk-umd-native-lifetime-test.exe src/umd/dxvk-umd-runtime-gpu-test.exe src/umd/dxvk-umd-predication-test.exe
if ($LASTEXITCODE) { throw 'Production native entry/lifetime fixture build failed' }
ninja -C build-umd src/umd/viogpudxvk.dll.p/umd_ddi.cpp.obj src/umd/dxvk-umd-ddi-probe.exe.p/.._.._tests_umd-ddi-probe.cpp.obj
if ($LASTEXITCODE) { throw 'Early UMD/DDI compile checks failed' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
function Invoke-BoundedFixture([string]$Executable, [string]$Name) {
    $out = Join-Path $OutputDirectory "$Name.txt"
    $err = Join-Path $OutputDirectory "$Name.stderr.txt"
    $process = Start-Process $Executable -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $null = $process.Handle
    if (!$process.WaitForExit(30000)) {
        $process.Kill(); $process.WaitForExit()
        throw "$Name exceeded its 30-second deadline"
    }
    Get-Content -LiteralPath $out
    if ($process.ExitCode) { throw "$Name failed: $(Get-Content -LiteralPath $err -Raw)" }
}
if ($arch -ne 'arm64') {
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-runtime-gpu-test.exe runtime-gpu-test
    & build-umd/src/umd/dxvk-umd-identity-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'runtime-query-test.txt')
    if ($LASTEXITCODE) { throw 'Runtime identity callback consumer test failed' }
    & build-umd/src/umd/dxvk-umd-adapter-test.exe | Tee-Object (Join-Path $OutputDirectory 'adapter-test.txt')
    if ($LASTEXITCODE) { throw 'Adapter lifecycle test failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-entry-test.exe native-entry-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-lifetime-test.exe native-lifetime-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-predication-test.exe predication-test
    & build-umd/src/umd/dxvk-umd-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'query-test.txt')
    if ($LASTEXITCODE) { throw 'Query completion test failed' }
    & build-umd/src/umd/dxvk-umd-allocation-test.exe | Tee-Object (Join-Path $OutputDirectory 'allocation-test.txt')
    if ($LASTEXITCODE) { throw 'Runtime allocation/presentation test failed' }
    & build-umd/src/umd/dxvk-umd-shader-test.exe | Tee-Object (Join-Path $OutputDirectory 'shader-test.txt')
    if ($LASTEXITCODE) { throw 'Native shader reconstruction test failed' }
    & build-umd/src/umd/dxvk-umd-view-test.exe | Tee-Object (Join-Path $OutputDirectory 'view-test.txt')
    if ($LASTEXITCODE) { throw 'Native texture view/range/MSAA test failed' }
}
ninja -C build-umd src/umd/dxvk-umd-backend-probe.exe src/umd/viogpudxvk.dll src/umd/dxvk-umd-ddi-probe.exe src/umd/dxvk-umd-system-runtime-test.exe src/umd/dxvk-umd-predication-probe.exe
if ($LASTEXITCODE) { throw 'DXVK UMD development build failed' }
foreach ($name in @('viogpudxvk.dll', 'dxvk-umd-backend-probe.exe', 'dxvk-umd-ddi-probe.exe', 'dxvk-umd-predication-probe.exe')) {
    $path = Join-Path 'build-umd/src/umd' $name
    $imports = & dumpbin /imports $path | Out-String
    if ($LASTEXITCODE -or $imports -match 'D3D11CreateDevice|D3D11CoreCreateDevice|D3D11CreateDeviceAndSwapChain') { throw "Forbidden D3D runtime import: $name" }
    $headers = & dumpbin /headers $path | Out-String
    $machine = if ($arch -eq 'arm64') { 'AA64' } elseif ($arch -eq 'x64') { '8664' } else { '14C' }
    if ($headers -notmatch "$machine machine") { throw "Incorrect architecture for $name" }
    Copy-Item $path $OutputDirectory
    $imports | Set-Content (Join-Path $OutputDirectory "$name.imports.txt")
}
$exports = & dumpbin /exports build-umd/src/umd/viogpudxvk.dll | Out-String
if ($exports -notmatch 'VioGpuDxvkCreateDdiTestDevice' -or $exports -notmatch '\bOpenAdapter10\b' -or $exports -notmatch '\bOpenAdapter10_2\b' -or $exports -match '\bOpenAdapter\b|D3D11CreateDevice') { throw 'Unexpected native UMD exports' }
$exports | Set-Content (Join-Path $OutputDirectory 'exports.txt')
foreach ($name in @('dxvk-umd-native-entry-test.exe', 'dxvk-umd-native-lifetime-test.exe', 'dxvk-umd-allocation-test.exe', 'dxvk-umd-runtime-gpu-test.exe', 'dxvk-umd-system-runtime-test.exe', 'dxvk-umd-predication-test.exe')) {
    # Test-only WARP binaries are separate from the production import gate.
    # Include ARM64 fixtures for execution by the target validation owner.
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect fixture architecture: $name" }
    Copy-Item $path $OutputDirectory
}
if ($arch -ne 'arm64') {
    Invoke-BoundedFixture (Join-Path $OutputDirectory 'dxvk-umd-system-runtime-test.exe') system-runtime-test
}
@"
DXVK_COMMIT=$(git rev-parse HEAD)
ARCH=$arch
STATUS=DDI development candidate; not registered or installable as the system UMD.
Development DDIs include restricted SM4 VS/GS/PS, state and Draw; stream output and general shader interfaces remain pending.
Occlusion predication uses a synchronous CPU/GPU correctness fallback with a two-second query deadline; no efficient GPU conditional rendering claim. Same-source WARP and embedded Turnip DDI probes cover both outcomes, inversion, query reuse, unbinding and resource operations. Real target execution remains required.
Native OpenAdapter10_2 negotiates exact identity/generation; incomplete production interfaces and feature levels remain unadvertised.
Production entry/lifetime fixtures use controlled callbacks and a WARP backend; they are not ordinary Microsoft runtime activation.
Native resource creation unwinds failed staged owners; destruction retires private storage before callbacks. Allocation identity/reset checks and cleanup fixtures are included.
Native backend receives copied runtime callbacks before vkCreateDevice; Turnip internal BO allocation/map/submit use one runtime-owned context through private Mesa v1. Actual GPU/system-runtime acceptance pending.
Ordinary native DDI/Present jobs pump RuntimeGpu, runtime allocation and core callbacks on their original DDI caller; CalcPrivate remains concurrent. Worker requests between DDIs wait for the next permitted caller. Native Flush joins command recording and queue submission before returning, without waiting for GPU completion. DestroyDevice drains backend workers and closes allocations/context before return. No post-DestroyDevice runtime lifetime is assumed.
The actual Microsoft runtime test requires candidate activation rejection on CI without VIOGPU and independently validates WARP Draw/readback/Present/immediate teardown. WARP control success is not candidate rendering or native admission. The dispatch integration still needs actual target/runtime proof.
Windowed-blit Present development path uses runtime allocations and synchronized pixel copies; target proof pending.
Registration, ordinary runtime activation, complete required table, sharing and primary/flip Present remain pending.
"@ | Set-Content (Join-Path $OutputDirectory 'STATUS.txt')
Get-ChildItem $OutputDirectory -File | Where-Object Extension -in '.dll','.exe' | Get-FileHash | Format-List
