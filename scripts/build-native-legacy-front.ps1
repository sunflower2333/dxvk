# Build a real ARM64X legacy entry in the selected ARM64 MSVC environment.
# No module/probe execution or registration is performed by this producer.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,
      [Parameter(Mandatory=$true)][string[]]$Arm64EcLibraryDirectories,
      [string]$SourceManifestPath='',
      [ValidatePattern('^(|[0-9a-f]{64})$')][string]$SourceManifestSha256='')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:VSCMD_ARG_TGT_ARCH -cne 'arm64') { throw 'Select the actual ARM64 MSVC developer environment' }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Frontend output must be fresh' }
New-Item -ItemType Directory -Path $output -ErrorAction Stop | Out-Null
$repository = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $repository 'src/umd/umd_legacy_front.cpp'
$definition = Join-Path $repository 'src/umd/umd_legacy_front.def'
$runner = Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
$runnerHash = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'
$sourceNames = @('src/umd/umd_legacy_front.cpp','src/umd/umd_legacy_front.def',
    'scripts/build-native-legacy-front.ps1','scripts/owned-raw-process-f4bf37f-02.cs')
function Get-FrontRawGitBlob([string]$Path) {
    [byte[]]$content = [IO.File]::ReadAllBytes($Path)
    [byte[]]$header = [Text.Encoding]::ASCII.GetBytes('blob ' + $content.Length + [char]0)
    [byte[]]$blob = New-Object byte[] ($header.Length + $content.Length)
    [Array]::Copy($header,0,$blob,0,$header.Length)
    [Array]::Copy($content,0,$blob,$header.Length,$content.Length)
    $sha = [Security.Cryptography.SHA1]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($blob))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
$sourceMode = 'actual-git-checkout'
$sourceOriginalManifest = $null
if ($SourceManifestPath -or $SourceManifestSha256) {
    if (!$SourceManifestPath -or !$SourceManifestSha256 -or
        (Get-FileHash -LiteralPath $SourceManifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $SourceManifestSha256) {
        throw 'Explicit original source manifest bytes are required'
    }
    $sourceOriginalManifest = [IO.File]::ReadAllText($SourceManifestPath) | ConvertFrom-Json
    if ($sourceOriginalManifest.schema -cne 'native-legacy-front-source-input-v1' -or
        $sourceOriginalManifest.source_commit -cnotmatch '^[0-9a-f]{40}$') { throw 'Invalid raw-Git-original source manifest' }
    [object[]]$sourceRows = $sourceOriginalManifest.files
    if ($sourceRows.Count -ne $sourceNames.Count) { throw 'Exactly four raw Git source originals are required' }
    $sourceCommit = [string]$sourceOriginalManifest.source_commit
    $sourceMode = 'explicit-raw-git-originals-no-checkout-assumed'
} else {
    $sourceCommit = (& git -C $repository rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE -or $sourceCommit -cnotmatch '^[0-9a-f]{40}$') { throw 'Actual Git checkout or explicit original source manifest is required' }
}
$before = @($sourceNames | ForEach-Object {
    $name = $_; $path = Join-Path $repository $name
    $actual = Get-FrontRawGitBlob $path
    if ($sourceMode -ceq 'actual-git-checkout') {
        $blob = (& git -C $repository rev-parse ($sourceCommit + ':' + $name) | Out-String).Trim()
        if ($LASTEXITCODE) { throw "Missing committed source $name" }
    } else {
        $rows = @($sourceRows | Where-Object { $_.path -ceq $name })
        if ($rows.Count -ne 1 -or $rows[0].sha256 -cne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -or
            $rows[0].bytes -ne (Get-Item -LiteralPath $path).Length) { throw "Explicit original source differs: $name" }
        $blob = [string]$rows[0].git_blob
    }
    if ($blob -cne $actual) { throw "Frontend source differs from raw Git: $name" }
    [ordered]@{path=$_;bytes=(Get-Item -LiteralPath $path).Length;
        sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant();git_blob=$blob}
})
$before | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'source-before.json') -Encoding UTF8
Copy-Item -LiteralPath $runner -Destination (Join-Path $output 'owned-raw-process-original.cs.txt')
if ((Get-FileHash -LiteralPath $runner -Algorithm SHA256).Hash.ToLowerInvariant() -cne $runnerHash) { throw 'Original retained-handle runner changed' }
if (-not ('DxvkRawProcessF4_02' -as [type])) {
    Add-Type -TypeDefinition ([IO.File]::ReadAllText($runner))
}
$stages = New-Object 'System.Collections.Generic.List[object]'
function Invoke-FrontTool([string]$Name, [string]$Command, [string[]]$Arguments) {
    $executable = (Get-Command $Command -CommandType Application -ErrorAction Stop).Path
    $response = Join-Path $output ($Name + '.rsp')
    $Arguments | Set-Content -LiteralPath $response -Encoding ascii
    $stdout = Join-Path $output ($Name + '.stdout.raw')
    $stderr = Join-Path $output ($Name + '.stderr.raw')
    $argumentsText = '@"' + $response + '"'
    $owned = [DxvkRawProcessF4_02]::Run($executable, $argumentsText, $repository, $stdout, $stderr, 60000)
    $receipt = [ordered]@{name=$Name;executable=$executable;
        executable_sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant();
        arguments=$argumentsText;response_file=$response;response_arguments=$Arguments;response_count=$Arguments.Count;
        runner_sha256=$runnerHash;deadline_ms=60000;pid=$owned.Pid;start_utc=$owned.StartUtc;
        retained_process_handle=$owned.ProcessHandle;exited=$owned.Exited;exit_code_available=$owned.ExitCodeAvailable;
        exit_code=$(if ($owned.ExitCodeAvailable) { $owned.ExitCode } else { $null });
        timed_out=$owned.TimedOut;child_still_running=$owned.ChildStillRunning;pipes_drained=$owned.PipesDrained;
        stdout_bytes=$owned.StdoutBytes;stderr_bytes=$owned.StderrBytes;seconds=$owned.Seconds;capture_failure=$owned.Failure}
    $receipt | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $output ($Name + '.process.json')) -Encoding UTF8
    $stages.Add($receipt)
    if ($owned.Failure -or !$owned.ProcessHandle -or !$owned.Exited -or !$owned.ExitCodeAvailable -or
        !$owned.PipesDrained -or $owned.ChildStillRunning -or $owned.TimedOut -or $owned.ExitCode -ne 0) {
        throw "Owned frontend stage failed: $Name (original stdout/stderr retained)"
    }
}

$completed = $false
$failure = $null
$nativeLibraryEnvironment = $env:LIB
$ecLibraries = [IO.Path]::GetFullPath((Join-Path $env:VCToolsInstallDir 'lib\arm64ec'))
$x64Libraries = [IO.Path]::GetFullPath((Join-Path $env:VCToolsInstallDir 'lib\x64'))
$Arm64EcLibraryDirectories = @($Arm64EcLibraryDirectories | ForEach-Object { [IO.Path]::GetFullPath($_) })
if ($Arm64EcLibraryDirectories.Count -lt 3 -or
    $Arm64EcLibraryDirectories -notcontains $ecLibraries -or
    $Arm64EcLibraryDirectories -notcontains $x64Libraries) {
    throw 'Reviewed ARM64EC support-object, x64 CRT and SDK library directories are required'
}
foreach ($directory in $Arm64EcLibraryDirectories) {
    if (!$directory -or $directory -match '(?i)[\\/]arm64[\\/]?$' -or
        $directory.Contains(';') -or $directory.Contains("`r") -or
        $directory.Contains("`n") -or !(Test-Path -LiteralPath $directory -PathType Container)) {
        throw 'ARM64EC library paths must be actual scalar directories'
    }
}
if (!(Test-Path -LiteralPath (Join-Path $ecLibraries 'chkstk_arm64ec.obj') -PathType Leaf) -or
    !(Test-Path -LiteralPath (Join-Path $x64Libraries 'libcmt.lib') -PathType Leaf)) {
    throw 'Actual ARM64EC support object and x64-compatible static CRT are required'
}
try {
    # /LINKREPROFULLPATHRSP records explicit inputs, not the /DEFAULTLIB
    # directives in the compiled object. Preserve native static CRT inputs
    # before the ARM64EC link changes LIB to its x64-compatible search path.
    if ([string]::IsNullOrWhiteSpace($nativeLibraryEnvironment)) {
        throw 'The selected native ARM64 library environment is required'
    }
    [string[]]$nativeLibraryDirectories = @($nativeLibraryEnvironment.Split(';') | Where-Object {
        ![string]::IsNullOrWhiteSpace($_)
    } | ForEach-Object { [IO.Path]::GetFullPath($_) })
    [string[]]$nativeStaticCrtInputs = @()
    $nativeStaticCrtPins = @()
    foreach ($name in @('libcmt.lib','libvcruntime.lib','libucrt.lib','oldnames.lib')) {
        $selected = $null
        for ($index = 0; $index -lt $nativeLibraryDirectories.Count; $index++) {
            $directory = $nativeLibraryDirectories[$index]
            $candidate = Join-Path $directory $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                if ($directory -notmatch '(?i)[\\/]arm64[\\/]?$') {
                    throw "The selected native CRT input is not ARM64: $candidate"
                }
                $selected = [IO.Path]::GetFullPath($candidate)
                $nativeStaticCrtPins += [ordered]@{name=$name;path=$selected;
                    library_directory_index=$index;
                    bytes=(Get-Item -LiteralPath $selected).Length;
                    sha256=(Get-FileHash -LiteralPath $selected -Algorithm SHA256).Hash.ToLowerInvariant()}
                break
            }
        }
        if (!$selected) { throw "Missing selected native static CRT library: $name" }
        $nativeStaticCrtInputs += ('"' + $selected + '"')
    }
    [ordered]@{schema='native-legacy-explicit-arm64-static-crt-inputs-v1';
        original_native_LIB=$nativeLibraryEnvironment;library_directories=$nativeLibraryDirectories;
        inputs=$nativeStaticCrtPins
    } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'arm64-explicit-static-crt-inputs.json') -Encoding UTF8
    $nativeObject = Join-Path $output 'front-arm64.obj'
    $nativeDll = Join-Path $output 'viogpu_dxvk_legacy_native.dll'
    $nativePdb = Join-Path $output 'viogpu_dxvk_legacy_native.pdb'
    $nativeRsp = Join-Path $output 'arm64-original-link-inputs.rsp'
    $commonCompile = @('/nologo','/std:c++17','/Zc:preprocessor','/EHsc','/MT','/O1','/W4','/WX',
        '/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/c')
    Invoke-FrontTool 'compile-arm64' 'cl.exe' ($commonCompile + @(('"' + $source + '"'),('/Fo"' + $nativeObject + '"')))
    Invoke-FrontTool 'link-arm64' 'link.exe' (@('/nologo','/DLL','/MACHINE:ARM64','/WX','/DEBUG:FULL',
        ('"' + $nativeObject + '"'),('/DEF:"' + $definition + '"'),('/OUT:"' + $nativeDll + '"'),
        ('/PDB:"' + $nativePdb + '"'),('/LINKREPROFULLPATHRSP:"' + $nativeRsp + '"'),'kernel32.lib') + $nativeStaticCrtInputs)
    # Preserve the actual full ARM64 static-CRT/import/object inputs; do not
    # guess them from a list of requested names or reuse an x64 CRT directory.
    [string[]]$nativeInputs = @(Get-Content -LiteralPath $nativeRsp | Where-Object {
        $_ -match '(?i)^"?[A-Z]:[\\/].*\.(obj|lib)"?$'
    })
    if (!$nativeInputs.Count) { throw 'No actual ARM64 link inputs captured' }
    $nativeInputPins = @($nativeInputs | ForEach-Object {
        $path = $_.Trim('"')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing original link input $path" }
        [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;
            sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    $nativeInputPins | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'arm64-original-link-inputs.json') -Encoding UTF8
    foreach ($explicit in $nativeStaticCrtPins) {
        $captured = @($nativeInputPins | Where-Object {
            [StringComparer]::OrdinalIgnoreCase.Equals($_.path, $explicit.path)
        })
        if ($captured.Count -ne 1 -or $captured[0].bytes -ne $explicit.bytes -or
            $captured[0].sha256 -cne $explicit.sha256) {
            throw "Native full-path response omitted or changed a static CRT input: $($explicit.path)"
        }
    }
    $mergeRsp = Join-Path $output 'arm64-merge-inputs.rsp'
    $nativeInputs | Set-Content -LiteralPath $mergeRsp -Encoding ascii
    # ARM64EC accepts x64-compatible libraries; native ARM64 libraries belong
    # only to the preserved ARM64 merge response, never the EC search path.
    $env:LIB = $Arm64EcLibraryDirectories -join ';'
    $ecObject = Join-Path $output 'front-arm64ec.obj'
    Invoke-FrontTool 'compile-arm64ec' 'cl.exe' ($commonCompile + @('/arm64EC',('"' + $source + '"'),('/Fo"' + $ecObject + '"')))
    # Keep the merge file as original evidence. LINK cannot expand an @file
    # embedded in Invoke-FrontTool's response file, so splice the same inputs.
    Invoke-FrontTool 'link-arm64x' 'link.exe' (@('/nologo','/DLL','/MACHINE:ARM64X','/WX','/DEBUG:FULL',
        ('"' + $ecObject + '"')) + $nativeInputs + @(
        ('/DEFARM64NATIVE:"' + $definition + '"'),('/DEF:"' + $definition + '"'),
        ('/OUT:"' + (Join-Path $output 'viogpu_dxvk_legacy.dll') + '"'),
        ('/PDB:"' + (Join-Path $output 'viogpu_dxvk_legacy.pdb') + '"'),
        ('/LINKREPROFULLPATHRSP:"' + (Join-Path $output 'arm64x-original-link-inputs.rsp') + '"'),'kernel32.lib'))
    $hybrid = Join-Path $output 'viogpu_dxvk_legacy.dll'
    Invoke-FrontTool 'hybrid-headers' 'dumpbin.exe' @('/headers','/loadconfig',('"' + $hybrid + '"'))
    Invoke-FrontTool 'hybrid-exports' 'dumpbin.exe' @('/exports',('"' + $hybrid + '"'))
    Invoke-FrontTool 'hybrid-imports' 'dumpbin.exe' @('/imports',('"' + $hybrid + '"'))
    $headers = [IO.File]::ReadAllText((Join-Path $output 'hybrid-headers.stdout.raw'))
    $exports = [IO.File]::ReadAllText((Join-Path $output 'hybrid-exports.stdout.raw'))
    $imports = [IO.File]::ReadAllText((Join-Path $output 'hybrid-imports.stdout.raw'))
    if ($headers -notmatch '(?im)^\s*AA64 machine' -or $headers -notmatch '\.a64xrm' -or $headers -notmatch '\.hexpthk') {
        throw 'Actual ARM64X hybrid sections are missing'
    }
    if ($exports -notmatch '(?m)\sOpenAdapter\s*$' -or $exports -match 'OpenAdapter10|Direct3DCreate|D3D11Create') {
        throw 'Unexpected legacy-only frontend exports'
    }
    if ($imports -match '(?i)\b(?:d3d9|d3d11|dxgi|viogpudxvk|msvcp\d+|vcruntime\d+)\.dll\b|Direct3DCreate|D3D11CreateDevice') {
        throw 'Unexpected frontend runtime/core/dynamic-CRT import'
    }
    $completed = $true
} catch {
    $failure = $_.Exception.ToString()
    throw
} finally {
    $env:LIB = $nativeLibraryEnvironment
    $after = @($before | ForEach-Object {
        $path = Join-Path $repository $_.path
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        [ordered]@{path=$_.path;bytes=(Get-Item -LiteralPath $path).Length;sha256=$hash;
            matches_before=($hash -ceq $_.sha256 -and (Get-Item -LiteralPath $path).Length -eq $_.bytes)}
    })
    $after | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'source-after.json') -Encoding UTF8
    $sameCommit = if ($sourceMode -ceq 'actual-git-checkout') {
        (& git -C $repository rev-parse HEAD | Out-String).Trim() -ceq $sourceCommit
    } else {
        (Get-FileHash -LiteralPath $SourceManifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $SourceManifestSha256
    }
    $stable = $sameCommit -and @($after | Where-Object { !$_.matches_before }).Count -eq 0
    [ordered]@{schema='native-legacy-arm64x-build-v1';source_commit=$sourceCommit;source_mode=$sourceMode;
        source_manifest_sha256=$(if ($sourceMode -ceq 'actual-git-checkout') { $null } else { $SourceManifestSha256 });completed=$completed;
        passed=($completed -and $stable);failure=$failure;source_stable=$stable;stages=$stages.ToArray();
        arm64ec_library_directories=$Arm64EcLibraryDirectories;
        core_layout=[ordered]@{native='arm64/viogpudxvk.dll';emulated_x64='x64/viogpudxvk.dll';wow='x86/viogpudxvk.dll'};
        module_or_probe_executed=$false;registry_modified=$false;installation=$false
    } | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $output 'native-legacy-front-build.json') -Encoding UTF8
    if (-not $stable) { throw 'Frontend source changed during the build' }
}
