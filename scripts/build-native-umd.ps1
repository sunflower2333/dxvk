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
ninja -C build-umd src/umd/dxvk-umd-identity-query-test.exe src/umd/dxvk-umd-adapter-test.exe src/umd/dxvk-umd-query-test.exe
if ($LASTEXITCODE) { throw 'Runtime adapter CPU test build failed' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
if ($arch -ne 'arm64') {
    & build-umd/src/umd/dxvk-umd-identity-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'runtime-query-test.txt')
    if ($LASTEXITCODE) { throw 'Runtime identity callback consumer test failed' }
    & build-umd/src/umd/dxvk-umd-adapter-test.exe | Tee-Object (Join-Path $OutputDirectory 'adapter-test.txt')
    if ($LASTEXITCODE) { throw 'Adapter lifecycle test failed' }
    & build-umd/src/umd/dxvk-umd-query-test.exe | Tee-Object (Join-Path $OutputDirectory 'query-test.txt')
    if ($LASTEXITCODE) { throw 'Query completion test failed' }
}
ninja -C build-umd src/umd/dxvk-umd-backend-probe.exe src/umd/viogpudxvk.dll src/umd/dxvk-umd-ddi-probe.exe src/umd/dxvk-umd-shader-test.exe
if ($LASTEXITCODE) { throw 'DXVK UMD development build failed' }
if ($arch -ne 'arm64') {
    & build-umd/src/umd/dxvk-umd-shader-test.exe | Tee-Object (Join-Path $OutputDirectory 'shader-test.txt')
    if ($LASTEXITCODE) { throw 'Native shader reconstruction test failed' }
}
foreach ($name in @('viogpudxvk.dll', 'dxvk-umd-backend-probe.exe', 'dxvk-umd-ddi-probe.exe')) {
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
if ($exports -notmatch 'VioGpuDxvkCreateDdiTestDevice' -or $exports -notmatch 'VioGpuDxvkOpenAdapterForTest' -or $exports -match '\bOpenAdapter(?:10(?:_2)?)?\b|D3D11CreateDevice') { throw 'Unexpected UMD development exports' }
$exports | Set-Content (Join-Path $OutputDirectory 'exports.txt')
@"
DXVK_COMMIT=$(git rev-parse HEAD)
ARCH=$arch
STATUS=DDI development candidate; not registered or installable as the system UMD.
Development DDIs include restricted SM4 VS/PS, state and Draw; general shader interfaces remain pending.
Development OpenAdapter harness validates runtime identity and retains it across adapter/device lifetime.
Runtime-to-DDI activation, complete required table, sharing and Present remain pending.
"@ | Set-Content (Join-Path $OutputDirectory 'STATUS.txt')
Get-ChildItem $OutputDirectory -File | Where-Object Extension -in '.dll','.exe' | Get-FileHash | Format-List
