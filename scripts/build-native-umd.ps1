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
    'meson.build', 'meson_options.txt', 'src/vulkan/meson.build', 'src/vulkan/vulkan_loader.cpp',
    'src/vulkan/vulkan_loader.h', 'src/umd/meson.build', 'src/umd/umd_vulkan_loader.cpp', 'src/umd/viogpudxvk.def')
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
ninja -C build-umd src/umd/dxvk-umd-d3d11-device-test.exe src/umd/dxvk-umd-compute-container-test.exe src/umd/dxvk-umd-sm5-container-test.exe src/umd/dxvk-umd-legacy-api-test.exe src/umd/dxvk-umd-d3d8-sm1-test.exe
if ($LASTEXITCODE) { throw 'Typed DX10/DX11 and DX8/SM5 compiler fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-private-children-test.exe src/umd/dxvk-umd-input-formats-test.exe src/umd/dxvk-umd-d3d10-formats-test.exe src/umd/dxvk-umd-multisample-policy-test.exe src/umd/dxvk-umd-d3d9-buffer-copy-test.exe
if ($LASTEXITCODE) { throw 'Native child/input/format and D3D9 copy fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-d3d9-runtime-callbacks-test.exe
if ($LASTEXITCODE) { throw 'Probe-only D3D9 callback fixture build failed' }
ninja -C build-umd src/umd/dxvk-umd-sm41-container-test.exe src/umd/dxvk-umd-d3d10-shader-test.exe
if ($LASTEXITCODE) { throw 'Native D3D10.1 shader compiler and rendering fixture build failed' }
ninja -C build-umd "src/umd/$LibraryName.dll.p/umd_ddi.cpp.obj" src/umd/dxvk-umd-ddi-probe.exe.p/.._.._tests_umd-ddi-probe.cpp.obj
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
    $process.WaitForExit()
    Get-Content -LiteralPath $out
    if ($null -eq $process.ExitCode -or $process.ExitCode -ne 0) {
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
    & build-umd/src/umd/dxvk-umd-adapter-test.exe | Tee-Object (Join-Path $OutputDirectory 'adapter-test.txt')
    if ($LASTEXITCODE) { throw 'Adapter lifecycle test failed' }
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-adapter-test.exe d3d9-adapter-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d9-device-test.exe d3d9-device-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-entry-test.exe native-entry-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-native-lifetime-test.exe native-lifetime-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-predication-test.exe predication-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-stream-output-test.exe stream-output-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-texture1d-test.exe texture1d-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d11-device-test.exe d3d11-device-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-compute-container-test.exe compute-container-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-sm5-container-test.exe sm5-container-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-legacy-api-test.exe legacy-api-test
    Invoke-BoundedFixture build-umd/src/umd/dxvk-umd-d3d8-sm1-test.exe d3d8-sm1-test
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
foreach ($name in @("$LibraryName.dll", 'dxvk-umd-backend-probe.exe', 'dxvk-umd-ddi-probe.exe', 'dxvk-umd-predication-probe.exe', 'dxvk-umd-stream-output-probe.exe', 'dxvk-umd-d3d9-device-probe.exe')) {
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
if ($exports -notmatch 'VioGpuDxvkCreateDdiTestDevice' -or $exports -notmatch '\bVioGpuDxvkQueryVulkanLoader\b' -or $exports -notmatch '\bVioGpuDxvkOpenAdapter9ForTest\b' -or $exports -notmatch '\bOpenAdapter10\b' -or $exports -notmatch '\bOpenAdapter10_2\b' -or $exports -match '\bOpenAdapter\b|D3D11CreateDevice') { throw 'Unexpected native UMD exports' }
if ($exports -notmatch '\bVioGpuDxvkProbeD3D9BackendForTest\b' -or $exports -match '\bDirect3DCreate9(?:Ex|On12)?\b') { throw 'Unexpected embedded D3D9 exports' }
$exports | Set-Content (Join-Path $OutputDirectory 'exports.txt')
foreach ($name in @('dxvk-umd-rotation-test.exe', 'dxvk-umd-native-entry-test.exe', 'dxvk-umd-native-lifetime-test.exe', 'dxvk-umd-allocation-test.exe', 'dxvk-umd-runtime-gpu-test.exe', 'dxvk-umd-system-runtime-test.exe', 'dxvk-umd-predication-test.exe', 'dxvk-umd-stream-output-test.exe', 'dxvk-umd-query-test.exe', 'dxvk-umd-texture1d-test.exe', 'dxvk-umd-d3d9-adapter-test.exe', 'dxvk-umd-d3d11-device-test.exe', 'dxvk-umd-compute-container-test.exe', 'dxvk-umd-sm5-container-test.exe', 'dxvk-umd-legacy-api-test.exe', 'dxvk-umd-d3d8-sm1-test.exe', 'dxvk-umd-private-children-test.exe', 'dxvk-umd-input-formats-test.exe', 'dxvk-umd-d3d10-formats-test.exe', 'dxvk-umd-multisample-policy-test.exe', 'dxvk-umd-d3d9-buffer-copy-test.exe', 'dxvk-umd-d3d9-runtime-callbacks-test.exe', 'dxvk-umd-sm41-container-test.exe', 'dxvk-umd-d3d10-shader-test.exe')) {
    # Test-only WARP binaries are separate from the production import gate.
    # Include ARM64 fixtures for execution by the target validation owner.
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect fixture architecture: $name" }
    Copy-Item $path $OutputDirectory
}
foreach ($name in @('dxvk-umd-runtime-backend-test.exe', 'dxvk-umd-d3d9-backend-test.exe', 'dxvk-umd-d3d9-device-test.exe', 'dxvk-umd-vertex-input-test.exe')) {
    $path = Join-Path 'build-umd/src/umd' $name
    $headers = & dumpbin /headers $path | Out-String
    if ($LASTEXITCODE -or $headers -notmatch "$machine machine") { throw "Incorrect fixture architecture: $name" }
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
Typed D3D9 adapter development bridge saves the runtime handle/query owner and validates identity, caps buffer sizes, reentry, close and reset. Development CreateDevice constructs the embedded offscreen renderer with copied callbacks and a separate driver token. Typed device DDIs include Flush/DestroyDevice and restricted surface CreateResource/DestroyResource/SetRenderTarget/Clear/Blt/Lock/Unlock, owned vertex declarations, supported render states, viewport/zrange/scissor, stream-zero user memory, nonindexed fast-path draw, owned SM1-3 vertex/pixel shaders and float/int/bool constant uploads. Typed caps expose the implemented static/dynamic A8/X8 2D textures/render targets, D16/D24S8 depth buffers, six query types and a conservative SM2/single-RT profile. Unsupported cube/volume/MSAA/instancing, autogen textures, gamma, sharing and stretched plain-surface operations stay unadvertised. Caps arguments/output ownership are snapshotted before identity callbacks. VioGpuDxvkOpenAdapter9ForTest is a harness helper; production OpenAdapter is absent.
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
