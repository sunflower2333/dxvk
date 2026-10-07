param(
  [Parameter(Mandatory)][string]$Packet,
  [Parameter(Mandatory)][string]$Manifest,
  [Parameter(Mandatory)][string]$Root,
  [Parameter(Mandatory)][string]$CompilerRoot,
  [string]$KitRoot = 'D:\Program Files\Windows Kits\10',
  [string]$KitVersion = '10.0.28000.0',
  [string[]]$Candidates = @(
    'C:\Users\Public\DxvkD3D9Candidate-061ee8f-37549764498\viogpudxvk.dll',
    'C:\Users\Public\DxvkD3D9CapsCandidate-478eca2\viogpudxvk.dll',
    'C:\Users\Public\DxvkD3D9DeviceFlagsCandidate-c8fbd55-37569563644\viogpudxvk.dll'
  )
)
# Prepared CPU-only workflow. No valid runtime mode, registry write, service
# change, GPU/device creation, image/VM action or implicit architecture fallback.
$ErrorActionPreference = 'Stop'
if (Test-Path $Root) { throw 'Fresh evidence root required; preserving old attempts' }
if (!$Root.StartsWith('C:\Users\Public\DxvkD3D8Cpu-', [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Expected a fresh task-owned public CPU root'
}
New-Item -ItemType Directory $Root | Out-Null
Copy-Item $PSCommandPath (Join-Path $Root 'executed-build-helper.ps1')
Copy-Item $Manifest (Join-Path $Root 'source-manifest.json')
$receipt = [ordered]@{scope='native x86 CPU build/fixtures/invalid CLI only';target_arch='x86';installation=$false;gpu_runs=0;commands=@()}
function Save-Receipt { $receipt | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $Root 'result.json') -Encoding UTF8 }
function Hash([string]$Path) { (Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function File-Row([string]$Path) {
  if (!(Test-Path $Path -PathType Leaf)) { return [ordered]@{path=$Path;present=$false} }
  $file = Get-Item $Path
  [ordered]@{path=$Path;present=$true;bytes=$file.Length;sha256=(Hash $Path);version=$file.VersionInfo.FileVersion}
}
function State {
  $id = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_10501AF4&REV_01\3&11583659&1&08'
  $enum = Get-ItemProperty ('HKLM:\SYSTEM\CurrentControlSet\Enum\' + $id)
  if ($enum.Driver -cne '{4d36e968-e325-11ce-bfc1-08002be10318}\0002' -or $enum.Service -cne 'VioGpuWddm') { throw 'Display binding changed' }
  $class = Get-ItemProperty ('HKLM:\SYSTEM\CurrentControlSet\Control\Class\' + $enum.Driver)
  if ($class.InfPath -cne 'oem17.inf' -or $class.DriverVersion -cne '100.6.101.58624') { throw 'Installed display package changed' }
  $sys = 'C:\WINDOWS\System32\DriverStore\FileRepository\viogpuwddm.inf_arm64_46d4547d492b1e80\viogpuwddm.sys'
  if ((Hash $sys) -cne 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a') { throw 'Original signed SYS changed' }
  Add-Type -AssemblyName System.ServiceProcess
  $service = New-Object System.ServiceProcess.ServiceController('VioGpuWddm')
  try { $status = $service.Status.ToString() } finally { $service.Dispose() }
  if ($status -cne 'Running') { throw 'Display service is not running' }
  $values = [ordered]@{}
  foreach ($item in ($class.PSObject.Properties | Sort-Object Name)) {
    if ($item.Name -notlike 'PS*') { $values[$item.Name] = $item.Value }
  }
  $desktop = @(Get-Process dwm,explorer | Sort-Object Name,Id | ForEach-Object {
    [ordered]@{name=$_.Name;pid=$_.Id;start=$_.StartTime.ToString('o')}
  })
  if ($desktop.Count -ne 2) { throw 'Expected one DWM and one Explorer' }
  [ordered]@{sys=(File-Row $sys);service=$status;enum_binding=$enum.Driver;active_values=$values;desktop=$desktop;
    system_d3d9=(File-Row 'C:\Windows\System32\d3d9.dll');system_d3d8_x86=(File-Row 'C:\Windows\SysWOW64\d3d8.dll');
    candidates=@($Candidates | ForEach-Object { File-Row $_ });metadata='direct known Enum/Class, service API, files and Get-Process; no CIM'}
}
function Run([string]$Name, [string]$Exe, [string]$Arguments, [int]$Expected, [int]$Seconds) {
  $out = Join-Path $Root ($Name + '.stdout.txt'); $err = Join-Path $Root ($Name + '.stderr.txt')
  $entry = [ordered]@{name=$Name;exe=$Exe;arguments=$Arguments;expected=$Expected;deadline_seconds=$Seconds;stdout=$out;stderr=$err}
  $receipt.commands += $entry; Save-Receipt
  $process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -NoNewWindow -RedirectStandardOutput $out -RedirectStandardError $err
  if (!$process.WaitForExit($Seconds * 1000)) {
    # Only this owned child may be terminated. No target-wide process actions.
    $process.Kill(); $process.WaitForExit(); $entry.timeout=$true; Save-Receipt; throw ($Name + ' timed out')
  }
  $process.Refresh(); $entry.exit=$process.ExitCode; Save-Receipt
  if ($process.ExitCode -ne $Expected) { throw ($Name + ' exit=' + $process.ExitCode) }
  if ($Name -like '*compile*' -and (([IO.File]::ReadAllText($out) + [IO.File]::ReadAllText($err)) -match '(?im)\bwarning C\d+')) {
    throw ($Name + ' compiler warning')
  }
}
function Source-Rows([object]$Sources) {
  @($Sources | ForEach-Object {
    $file = Join-Path (Join-Path $Root 'source') $_.path
    if ((Hash $file) -cne $_.sha256) { throw ('Frozen input mismatch ' + $_.path) }
    File-Row $file
  })
}
try {
  $input = Get-Content $Manifest -Raw | ConvertFrom-Json
  if ((Hash $Packet) -cne $input.archive_sha256 -or $input.target_execution -cne 'deferred') { throw 'Packet/manifest mismatch' }
  $receipt.source_commit=$input.source_commit; $receipt.packet=(File-Row $Packet)
  $before=State; $receipt.before=$before; Save-Receipt
  New-Item -ItemType Directory (Join-Path $Root 'source') | Out-Null
  Run 'extract' (Join-Path $env:SystemRoot 'System32\tar.exe') ('-xf "' + $Packet + '" -C "' + (Join-Path $Root 'source') + '"') 0 30
  $receipt.source_before=Source-Rows $input.inputs
  $bin = Join-Path $CompilerRoot 'bin\Hostarm64\x86'
  $cl = Join-Path $bin 'cl.exe'; $dumpbin = Join-Path $bin 'dumpbin.exe'
  # Read every original compiler component before using the mounted EWDK.
  # Missing/truncated clui.dll fails with evidence, never substitutes a tool.
  $receipt.compiler=@('cl.exe','c1.dll','c1xx.dll','c2.dll','mspdbcore.dll','1033\clui.dll' | ForEach-Object {
    $file=Join-Path $bin $_; [void][IO.File]::ReadAllBytes($file); File-Row $file
  })
  $sdkInclude=Join-Path $KitRoot ('Include\' + $KitVersion)
  $env:INCLUDE=(Join-Path $CompilerRoot 'include') + ';' + (Join-Path $sdkInclude 'shared') + ';' + (Join-Path $sdkInclude 'um') + ';' + (Join-Path $sdkInclude 'ucrt')
  $env:LIB=(Join-Path $CompilerRoot 'lib\x86') + ';' + (Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86')) + ';' + (Join-Path $KitRoot ('Lib\' + $KitVersion + '\ucrt\x86'))
  $env:PATH=$bin + ';' + $env:PATH
  $receipt.kit_root=$KitRoot; $receipt.kit_version=$KitVersion
  $receipt.sdk=@('shared\d3d9.h','shared\d3d9caps.h','shared\d3d9types.h','shared\d3dkmthk.h','um\d3dumddi.h','um\Windows.h' | ForEach-Object { File-Row (Join-Path $sdkInclude $_) })
  $receipt.libraries=@((Join-Path $CompilerRoot 'lib\x86\libcmt.lib'),(Join-Path $CompilerRoot 'lib\x86\libcpmt.lib'),
    (Join-Path $CompilerRoot 'lib\x86\libvcruntime.lib'),(Join-Path $KitRoot ('Lib\' + $KitVersion + '\ucrt\x86\libucrt.lib')),
    (Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86\kernel32.lib')),(Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86\user32.lib')) | ForEach-Object { File-Row $_ })
  $source=Join-Path $Root 'source'; $legacy=Join-Path $source 'dependencies\legacy-d3d8'
  $units=@('tests\umd-d3d8-runtime-probe.cpp','tests\umd-runtime-imports.cpp','tests\umd-d3d8-compat.cpp','tests\umd-d3d8-sdk.cpp','src\umd\umd_d3d8_compat.cpp','tests\umd-d3d8-runtime-guard.cpp')
  $objects=@{}
  foreach ($unit in $units) {
    $name=[IO.Path]::GetFileNameWithoutExtension($unit); $object=Join-Path $Root ($name + '.obj'); $rsp=Join-Path $Root ($name + '.compile.rsp')
    @('/nologo','/c','/W4','/WX','/MT','/EHsc','/std:c++17','/Z7','/external:W0',('/external:I"' + $legacy + '"'),('/Fo"' + $object + '"'),('"' + (Join-Path $source $unit) + '"')) | Set-Content $rsp -Encoding ASCII
    Run ($name + '-compile') $cl ('@"' + $rsp + '"') 0 120
    $objects[$unit]=$object
  }
  $groups=@(
    @{name='caps';units=@('tests\umd-d3d8-compat.cpp','tests\umd-d3d8-sdk.cpp','src\umd\umd_d3d8_compat.cpp')},
    @{name='imports';units=@('tests\umd-runtime-imports.cpp')},
    @{name='runtime';units=@('tests\umd-d3d8-runtime-probe.cpp','tests\umd-d3d8-runtime-guard.cpp')}
  )
  foreach ($group in $groups) {
    $exe=Join-Path $Root ($group.name + '.exe'); $rsp=Join-Path $Root ($group.name + '.link.rsp')
    @('/nologo',('/Fe"' + $exe + '"')) + @($group.units | ForEach-Object { '"' + $objects[$_] + '"' }) | Set-Content $rsp -Encoding ASCII
    # /link must be on the actual cl command line, outside the compiler rsp.
    Run ($group.name + '-link') $cl ('@"' + $rsp + '" /link /MACHINE:X86 /SUBSYSTEM:CONSOLE kernel32.lib user32.lib') 0 120
    Run ($group.name + '-headers') $dumpbin ('/headers /imports "' + $exe + '"') 0 30
    if ($group.name -ne 'runtime') { Run ($group.name + '-fixture') $exe ' ' 0 30 }
  }
  $runtime=Join-Path $Root 'runtime.exe'
  $bad=@('','--unknown','--enumerate extra','--offscreen extra','--front-enumerate','--front-enumerate missing','--front-enumerate one two extra')
  for ($i=0; $i -lt $bad.Count; $i++) { Run ('invalid-cli-' + $i) $runtime (' ' + $bad[$i]) 64 10 }
  $receipt.outputs=@($objects.Values | Sort-Object | ForEach-Object { File-Row $_ }) + @(Get-ChildItem $Root -Filter '*.exe' | Sort-Object Name | ForEach-Object { File-Row $_.FullName })
  $receipt.source_after=Source-Rows $input.inputs
  $after=State; $receipt.after=$after
  if (($before | ConvertTo-Json -Depth 12 -Compress) -cne ($after | ConvertTo-Json -Depth 12 -Compress)) { throw 'Original target state changed' }
  $receipt.status='PASS'; Save-Receipt
} catch {
  $receipt.status='FAIL'; $receipt.error=$_.Exception.Message
  try { $receipt.failure_state=State } catch { $receipt.failure_state_error=$_.Exception.Message }
  Save-Receipt; throw
}
