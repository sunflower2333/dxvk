param(
  [Parameter(Mandatory)][string]$Packet,
  [Parameter(Mandatory)][string]$Manifest,
  [Parameter(Mandatory)][string]$SourceCommit,
  [Parameter(Mandatory)][string]$Root,
  [Parameter(Mandatory)][string]$CompilerRoot,
  [Parameter(Mandatory)][string]$CompilerProvenance,
  [string]$RunnerSource = (Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'),
  [ValidateSet('auto','arm64','x64')][string]$CompilerHost = 'auto',
  [string]$KitRoot = 'D:\Program Files\Windows Kits\10',
  [string]$KitVersion = '10.0.28000.0',
  [string[]]$Candidates = @(
    'C:\Users\Public\DxvkD3D9Candidate-061ee8f-37549764498\viogpudxvk.dll',
    'C:\Users\Public\DxvkD3D9CapsCandidate-478eca2\viogpudxvk.dll',
    'C:\Users\Public\DxvkD3D9DeviceFlagsCandidate-c8fbd55-37569563644\viogpudxvk.dll'
  )
)
# Native CPU policy and frontend null/invalid guards only. No valid system
# runtime mode, KMT selector, device, rendering, registry, service or VM action.
$ErrorActionPreference = 'Stop'
if (Test-Path $Root) { throw 'Fresh evidence root required' }
$Root = [IO.Path]::GetFullPath($Root)
if (!$Root.StartsWith('C:\Users\Public\DxvkD3D8Runtime-', [StringComparison]::OrdinalIgnoreCase)) {
  throw 'Expected a fresh owned public DX8 runtime build root'
}
foreach ($name in @('VIOGPU_DXVK_RUNTIME_DIAGNOSTIC','VIOGPU_DXVK_D3D8_CORE_PATH',
    'VIOGPU_DXVK_D3D8_CORE_SHA256','VIOGPU_DXVK_D3D8_CORE_COMMIT')) {
  if (Test-Path ('Env:' + $name)) { throw 'Diagnostic permission/core pins must initially be absent' }
}
New-Item -ItemType Directory $Root | Out-Null
Copy-Item $PSCommandPath (Join-Path $Root 'executed-build-helper.ps1')
Copy-Item $Manifest (Join-Path $Root 'source-manifest.json')
Copy-Item $Packet (Join-Path $Root 'original-source-packet.tar.gz')
$receipt = [ordered]@{schema='native-system-d3d8-readonly-x86-build-v1';scope='native CPU policy and frontend guard only';
  target_arch='x86';installation=$false;gpu_runs=0;system_runtime_calls=0;selector_calls=0;commands=@();compile_inputs=@();outputs=@()}
function Save-Receipt { $receipt | ConvertTo-Json -Depth 24 | Set-Content (Join-Path $Root 'result.json') -Encoding UTF8 }
function Hash([string]$Path) { (Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Machine([string]$Path) {
  $data = [IO.File]::ReadAllBytes($Path)
  if ($data.Length -lt 20) { return 0 }
  if ($data[0] -eq 0x4d -and $data[1] -eq 0x5a) {
    if ($data.Length -lt 64) { throw 'Truncated PE header' }
    $offset = [BitConverter]::ToUInt32($data, 0x3c)
    if ($offset -gt $data.Length - 6 -or [BitConverter]::ToUInt32($data, $offset) -ne 0x4550) { throw 'Invalid PE header' }
    return [int][BitConverter]::ToUInt16($data, $offset + 4)
  }
  if ([BitConverter]::ToUInt16($data, 0) -eq 0 -and [BitConverter]::ToUInt16($data, 2) -eq 0xffff) {
    if ($data.Length -lt 56) { throw 'Truncated bigobj header' }
    return [int][BitConverter]::ToUInt16($data, 6)
  }
  return [int][BitConverter]::ToUInt16($data, 0)
}
function File-Row([string]$Path) {
  if (!(Test-Path $Path -PathType Leaf)) { return [ordered]@{path=$Path;present=$false} }
  $file = Get-Item $Path
  $row = [ordered]@{path=$file.FullName;present=$true;bytes=$file.Length;sha256=(Hash $Path);version=$file.VersionInfo.FileVersion}
  if ($file.Extension -in @('.exe','.dll','.sys','.obj')) { $row.machine = Machine $Path }
  return $row
}
function State {
  $id = 'PCI\VEN_1AF4&DEV_1050&SUBSYS_10501AF4&REV_01\3&11583659&1&08'
  $enum = Get-ItemProperty ('HKLM:\SYSTEM\CurrentControlSet\Enum\' + $id)
  if ($enum.Driver -cne '{4d36e968-e325-11ce-bfc1-08002be10318}\0002' -or $enum.Service -cne 'VioGpuWddm') { throw 'Display binding changed' }
  $class = Get-ItemProperty ('HKLM:\SYSTEM\CurrentControlSet\Control\Class\' + $enum.Driver)
  if ($class.InfPath -cne 'oem17.inf' -or $class.DriverVersion -cne '100.6.101.58624') { throw 'Installed package changed' }
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
  return [ordered]@{sys=(File-Row $sys);service=$status;enum_binding=$enum.Driver;active_values=$values;desktop=$desktop;
    system_d3d9=(File-Row 'C:\Windows\System32\d3d9.dll');system_d3d8_native=(File-Row 'C:\Windows\System32\d3d8.dll');
    system_d3d8_x86=(File-Row 'C:\Windows\SysWOW64\d3d8.dll');system_d3d9_x86=(File-Row 'C:\Windows\SysWOW64\d3d9.dll');
    candidates=@($Candidates | ForEach-Object { File-Row $_ });metadata='direct known Enum/Class, service API, files and Get-Process; no CIM'}
}
function Run([string]$Name, [string]$Exe, [string]$Arguments, [int]$Expected, [int]$Seconds) {
  $out=Join-Path $Root ($Name+'.stdout.txt');$err=Join-Path $Root ($Name+'.stderr.txt')
  $entry=[ordered]@{name=$Name;exe=$Exe;arguments=$Arguments;expected=$Expected;deadline_seconds=$Seconds;stdout=$out;stderr=$err;raw_pipe_bytes=$true}
  $receipt.commands += $entry;Save-Receipt
  # Exact native-tested runner owns the original OS handle and bounds Kill
  # reaping to5s, plus one combined20s deadline for both raw output streams.
  $run=[DxvkRawProcessF4_02]::Run($Exe,$Arguments,$Root,$out,$err,$Seconds*1000)
  $entry.pid=$run.Pid;$entry.start_utc=$run.StartUtc;$entry.retained_process_handle=$run.ProcessHandle
  $entry.exited=$run.Exited;$entry.exit_code_available=$run.ExitCodeAvailable
  $entry.exit=$null;if ($run.ExitCodeAvailable) { $entry.exit=[int]$run.ExitCode }
  $entry.timeout=$run.TimedOut;$entry.child_still_running=$run.ChildStillRunning
  $entry.pipes_drained=$run.PipesDrained;$entry.stdout_bytes=$run.StdoutBytes;$entry.stderr_bytes=$run.StderrBytes
  $entry.seconds=$run.Seconds;$entry.capture_failure=$run.Failure
  Save-Receipt
  if ($run.Failure) { throw ($Name+' owned capture failed: '+$run.Failure) }
  if ($run.TimedOut) { throw ($Name+' owned child timed out') }
  if (!$run.Exited -or !$run.ExitCodeAvailable -or !$run.PipesDrained -or $run.ChildStillRunning -or $run.ProcessHandle -eq 0) {
    throw ($Name+' missing actual owned process/exit/pipe evidence')
  }
  if ($entry.exit -ne $Expected) { throw ($Name+' exit='+$entry.exit) }
  if ($Name -like '*compile*') {
    $entry.compiler_warnings=@([regex]::Matches(([IO.File]::ReadAllText($out)+[IO.File]::ReadAllText($err)), '(?im)\bwarning [CD]\d+') | ForEach-Object { $_.Value })
    $entry.first_party_strict=$true
    Save-Receipt
    if ($entry.compiler_warnings.Count) { throw ($Name+' first-party compiler warning') }
  }
}

function Source-Rows([object]$Sources) {
  return @($Sources | ForEach-Object {
    if ([IO.Path]::IsPathRooted($_.path) -or $_.path -match '(^|[/\\])\.\.([/\\]|$)') { throw 'Unsafe input member path' }
    $file = Join-Path (Join-Path $Root 'source') $_.path
    if ((Hash $file) -cne $_.sha256) { throw ('Frozen input mismatch ' + $_.path) }
    $row = File-Row $file; $row.input_path=$_.path; $row
  })
}
function Select-Compiler {
  $hosts = if ($CompilerHost -eq 'auto') { @('arm64','x64') } else { @($CompilerHost) }
  $receipt.compiler_candidates=@()
  foreach ($hostName in $hosts) {
    $candidate=[ordered]@{host=$hostName;bin=(Join-Path $CompilerRoot ('bin\Host' + $hostName + '\x86'));readable=$false;files=@()}
    $receipt.compiler_candidates += $candidate
    try {
      foreach ($name in @('cl.exe','link.exe','lib.exe','dumpbin.exe','c1.dll','c1xx.dll','c2.dll','mspdbcore.dll','1033\clui.dll')) {
        $file=Join-Path $candidate.bin $name; [void][IO.File]::ReadAllBytes($file)
        $candidate.files += File-Row $file
      }
      $expected = if ($hostName -eq 'arm64') { 0xaa64 } else { 0x8664 }
      if ((Machine (Join-Path $candidate.bin 'cl.exe')) -ne $expected) { throw 'Compiler host architecture mismatch' }
      $candidate.readable=$true; Save-Receipt
      return $candidate
    } catch { $candidate.error=$_.Exception.Message; Save-Receipt }
  }
  throw 'No readable official compiler host in the explicit compiler tree; original attempts retained'
}

function Build-Group([object]$Group, [string]$SourceBase) {
  $dir = Join-Path $Root $Group.name
  New-Item -ItemType Directory $dir | Out-Null
  $objects = @()
  foreach ($unit in $Group.units) {
    $name = $unit -replace '[^A-Za-z0-9_.-]', '_'
    $object = Join-Path $dir ($name + '.obj')
    $rsp = Join-Path $dir ($name + '.compile.rsp')
    $sourceFile = Join-Path $SourceBase $unit
    @('/nologo','/c','/MT','/EHsc','/O1','/std:c++17','/Z7','/bigobj','/W4','/WX','/Zc:preprocessor',
      '/external:anglebrackets','/external:W0','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/DNTDDI_VERSION=0x0A00000C',
      ('/external:I"' + $legacy + '"'),('/Fo"' + $object + '"'),('"' + $sourceFile + '"')) |
        Set-Content $rsp -Encoding ASCII
    $receipt.compile_inputs += [ordered]@{group=$Group.name;source=(File-Row $sourceFile);response=(File-Row $rsp)}
    Save-Receipt
    Run ($Group.name + '-' + $name + '-compile') $cl ('@"' + $rsp + '"') 0 180
    if ((Machine $object) -ne 0x14c) { throw 'Expected original x86 COFF' }
    $objects += $object
  }
  $output = Join-Path $dir $Group.output
  $rsp = Join-Path $dir 'link.rsp'
  (@('/nologo','/MT',('/Fe"' + $output + '"')) + @($objects | ForEach-Object { '"' + $_ + '"' })) |
    Set-Content $rsp -Encoding ASCII
  $pdb = Join-Path $dir ($Group.name + '.pdb')
  $arguments = '@"' + $rsp + '"'
  if ($Group.kind -ceq 'dll') { $arguments += ' /LD' }
  $arguments += ' /link /MACHINE:X86 /DEBUG:FULL /INCREMENTAL:NO /PDB:"' + $pdb + '" kernel32.lib user32.lib'
  if ($Group.kind -ceq 'dll') {
    $arguments += ' bcrypt.lib'
    $def = Join-Path $SourceBase $Group.def
    $receipt.def = File-Row $def
    $arguments += ' /DLL /DEF:"' + $def + '" /IMPLIB:"' + (Join-Path $dir 'viogpu-d3d8-runtime-front.lib') + '"'
  } else { $arguments += ' /SUBSYSTEM:CONSOLE' }
  # The /link boundary is on the actual command line, outside the cl rsp.
  Run ($Group.name + '-link') $cl $arguments 0 120
  if ((Machine $output) -ne 0x14c) { throw 'Expected original x86 PE' }
  Run ($Group.name + '-headers') $dumpbin ('/headers /imports /exports "' + $output + '"') 0 30
  $imports = [IO.File]::ReadAllText((Join-Path $Root ($Group.name + '-headers.stdout.txt')))
  if ($imports -match '(?im)^\s*(msvcp\d+|vcruntime\d+|ucrtbase|msvcrt|api-ms-win-crt[^\s]*|d3d8|d3d9|dxgi|d3d10warp)\.dll\s*$') {
    throw 'Unexpected dynamic CRT or public graphics API import'
  }
  if ($Group.kind -ceq 'dll') {
    $exportNames = @([regex]::Matches($imports, '(?m)^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+([A-Za-z_][A-Za-z_0-9@]*)') |
      ForEach-Object { $_.Groups[1].Value })
    if ($exportNames.Count -ne 1 -or $exportNames[0] -cne 'OpenAdapter') { throw 'Expected only original OpenAdapter export' }
    $receipt.front_exports = $exportNames
  }
  $receipt.outputs += @($objects | ForEach-Object { File-Row $_ }) + @(File-Row $output)
  $receipt.auxiliaries += @(Get-ChildItem $dir -File | Where-Object { $_.Extension -in @('.lib','.exp','.pdb') } |
    ForEach-Object { File-Row $_.FullName })
  Save-Receipt
  return $output
}
try {
  $cpuManifest = [IO.File]::ReadAllText($Manifest) | ConvertFrom-Json
  if ($cpuManifest.schema -cne 'native-system-d3d8-readonly-x86-v1' -or $cpuManifest.source_commit -cne $SourceCommit -or
      $cpuManifest.target_arch -cne 'x86' -or $cpuManifest.target_execution -cne 'deferred' -or
      $cpuManifest.core_reference_commit -cne 'b75d6d583aa587da2f78b4e7183d3da14e5b373f' -or
      (Hash $Packet) -cne $cpuManifest.archive_sha256) { throw 'Packet/source/core identity mismatch' }
  if ((Hash $PSCommandPath) -cne $cpuManifest.build_helper_sha256) { throw 'Prepared helper identity mismatch' }
  if ((Hash $RunnerSource) -cne $cpuManifest.raw_process_helper_sha256 -or
      $cpuManifest.raw_process_helper_sha256 -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') {
    throw 'Original native-tested raw-process helper identity mismatch'
  }
  Copy-Item $RunnerSource (Join-Path $Root 'executed-raw-process-source.cs')
  $receipt.raw_process_source = File-Row $RunnerSource
  $receipt.raw_process_limits = [ordered]@{reap_after_kill_ms=5000;combined_pipe_drain_ms=20000;retains_os_process_handle=$true}
  Add-Type -Path $RunnerSource
  $receipt.source_commit = $SourceCommit
  $receipt.core_reference_commit = $cpuManifest.core_reference_commit
  $receipt.production_core_built = $false
  $receipt.production_core_required_for_cpu_guards = $false
  $receipt.packet = File-Row $Packet
  $receipt.manifest = File-Row $Manifest
  $receipt.auxiliaries = @()
  $before = State; $receipt.before = $before; Save-Receipt
  if (!$before.system_d3d8_x86.present -or $before.system_d3d8_x86.sha256 -cne
      '65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8') {
    throw 'Original Microsoft x86 D3D8 image differs; retain evidence and refresh inventory'
  }
  $source = Join-Path $Root 'source'; New-Item -ItemType Directory $source | Out-Null
  $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
  Run 'archive-list' $tar ('-tf "' + $Packet + '"') 0 30
  $listed = @([IO.File]::ReadAllLines((Join-Path $Root 'archive-list.stdout.txt')))
  $expected = @($cpuManifest.inputs | ForEach-Object { $_.path })
  if ($listed.Count -ne $expected.Count -or @(Compare-Object ($listed | Sort-Object) ($expected | Sort-Object)).Count) {
    throw 'Archive member list mismatch'
  }
  foreach ($name in $listed) {
    if ([IO.Path]::IsPathRooted($name) -or $name -match '(^|[/\\])\.\.([/\\]|$)') { throw 'Unsafe archive member' }
  }
  Run 'extract' $tar ('-xf "' + $Packet + '" -C "' + $source + '"') 0 30
  $receipt.source_before = Source-Rows $cpuManifest.inputs
  $selected = Select-Compiler; $bin = $selected.bin
  $cl = Join-Path $bin 'cl.exe'; $dumpbin = Join-Path $bin 'dumpbin.exe'
  $receipt.selected_compiler = $selected
  $receipt.compiler_host_execution = if ($selected.host -ceq 'arm64') {
    'official native Hostarm64/x86; x86 target output'
  } else { 'official Hostx64/x86 on ARM64 Windows under emulation; x86 target output' }
  Copy-Item $CompilerProvenance (Join-Path $Root 'original-compiler-provenance.json')
  $receipt.compiler_provenance = File-Row $CompilerProvenance
  $toolManifest = [IO.File]::ReadAllText($CompilerProvenance) | ConvertFrom-Json
  if (!$toolManifest.ready -or $toolManifest.compiler_root -cne $CompilerRoot -or
      $toolManifest.file_count -ne @($toolManifest.files).Count) { throw 'Owned official compiler provenance mismatch' }
  $receipt.compiler_full_before = @($toolManifest.files | ForEach-Object {
    if ((Hash $_.path) -cne $_.sha256) { throw ('Compiler provenance hash mismatch: ' + $_.path) }; File-Row $_.path
  })
  $sdkInclude = Join-Path $KitRoot ('Include\' + $KitVersion)
  $env:INCLUDE = (Join-Path $CompilerRoot 'include') + ';' + (Join-Path $sdkInclude 'shared') + ';' +
    (Join-Path $sdkInclude 'um') + ';' + (Join-Path $sdkInclude 'ucrt')
  $env:LIB = (Join-Path $CompilerRoot 'lib\x86') + ';' + (Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86')) + ';' +
    (Join-Path $KitRoot ('Lib\' + $KitVersion + '\ucrt\x86'))
  $env:PATH = $bin + ';' + $env:PATH
  $receipt.compiler_environment = [ordered]@{include=$env:INCLUDE;lib=$env:LIB;path_prefix=$bin;kit_root=$KitRoot;kit_version=$KitVersion}
  $headers = @('shared\d3d9.h','shared\d3d9caps.h','shared\d3d9types.h','shared\d3dukmdt.h',
    'shared\d3dkmthk.h','um\d3dumddi.h','um\Windows.h','shared\bcrypt.h')
  New-Item -ItemType Directory (Join-Path $Root 'original-sdk-headers') | Out-Null
  $receipt.sdk = @($headers | ForEach-Object {
    $p = Join-Path $sdkInclude $_
    Copy-Item $p (Join-Path $Root ('original-sdk-headers\' + [IO.Path]::GetFileName($p))); File-Row $p
  })
  $libraries = @((Join-Path $CompilerRoot 'lib\x86\libcmt.lib'),(Join-Path $CompilerRoot 'lib\x86\libcpmt.lib'),
    (Join-Path $CompilerRoot 'lib\x86\libvcruntime.lib'),(Join-Path $KitRoot ('Lib\' + $KitVersion + '\ucrt\x86\libucrt.lib')),
    (Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86\kernel32.lib')),(Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86\user32.lib')),
    (Join-Path $KitRoot ('Lib\' + $KitVersion + '\um\x86\bcrypt.lib')))
  New-Item -ItemType Directory (Join-Path $Root 'original-link-libraries') | Out-Null
  $receipt.libraries = @($libraries | ForEach-Object {
    Copy-Item $_ (Join-Path $Root ('original-link-libraries\' + [IO.Path]::GetFileName($_))); File-Row $_
  })
  $legacy = Join-Path $source 'dependencies\legacy-d3d8'
  $plan = @($cpuManifest.groups)
  if ($plan.Count -ne 3 -or @(Compare-Object @($plan.name | Sort-Object) @('front','policy','probe')).Count) {
    throw 'Unexpected readonly group plan'
  }
  $fixedUnits = @{
    front=@('tests/umd-d3d8-runtime-front.cpp')
    probe=@('tests/umd-d3d8-runtime-probe.cpp','tests/umd-d3d8-runtime-guard.cpp')
    policy=@('tests/umd-d3d8-runtime-policy.cpp')
  }
  $fixedOutputs = @{front='viogpu-d3d8-runtime-front.dll';probe='d3d8-runtime-probe.exe';policy='d3d8-runtime-policy.exe'}
  foreach ($group in $plan) {
    $units = @($group.units)
    if ($units.Count -ne @($fixedUnits[$group.name]).Count -or
        @(Compare-Object ($units | Sort-Object) ($fixedUnits[$group.name] | Sort-Object)).Count -or
        $group.output -cne $fixedOutputs[$group.name]) { throw 'Unexpected readonly compilation inputs' }
    if (($group.name -ceq 'front' -and ($group.kind -cne 'dll' -or $group.def -cne 'tests/umd-d3d8-runtime-front.def')) -or
        ($group.name -cne 'front' -and $group.kind -cne 'exe')) { throw 'Unexpected readonly compilation kind' }
  }
  $built = @{}
  foreach ($group in $plan) { $built[$group.name] = Build-Group $group $source }
  Run 'policy-positive' $built.policy '' 0 30
  $policyOut = [IO.File]::ReadAllText((Join-Path $Root 'policy-positive.stdout.txt'))
  if ($policyOut -notmatch '(?m)^D3D8 runtime selector policy PASS checks=306;') { throw 'Native policy positive missing' }
  Run 'frontend-null-invalid-guard' $built.probe ('--front-guard "' + $built.front + '"') 0 30
  $guardOut = [IO.File]::ReadAllText((Join-Path $Root 'frontend-null-invalid-guard.stdout.txt'))
  if ($guardOut -notmatch '(?m)^D3D8_FRONT_GUARD PASS .*invalid_interfaces=6 non_system_caller=1 no_core_open=1 system_runtime_calls=0') {
    throw 'Native frontend guards missing'
  }
  $cli = @('', '--unknown', '--enumerate extra', '--offscreen extra', '--front-guard', '--front-guard one two',
    '--front-guard one two three', '--front-enumerate', '--front-enumerate missing', '--front-enumerate one',
    '--front-enumerate one two extra', '--front-enumerate one two three four',
    '--front-enumerate one two three four five', '--front-enumerate one two three four five extra')
  $index = 0
  foreach ($arguments in $cli) { Run ('invalid-cli-' + (++$index)) $built.probe $arguments 64 15 }
  $receipt.malformed_cli_guards = $cli.Count
  $receipt.policy_checks = 306
  $receipt.source_after = Source-Rows $cpuManifest.inputs
  $receipt.compiler_full_after = @($toolManifest.files | ForEach-Object { File-Row $_.path })
  $receipt.sdk_after = @($receipt.sdk | ForEach-Object { File-Row $_.path })
  $receipt.libraries_after = @($receipt.libraries | ForEach-Object { File-Row $_.path })
  $receipt.raw_process_source_after = File-Row $RunnerSource
  if ($receipt.raw_process_source.sha256 -cne $receipt.raw_process_source_after.sha256) { throw 'Owned raw-process source changed' }
  if (($receipt.compiler_full_before | ConvertTo-Json -Depth 24 -Compress) -cne
      ($receipt.compiler_full_after | ConvertTo-Json -Depth 24 -Compress) -or
      ($receipt.sdk | ConvertTo-Json -Depth 24 -Compress) -cne ($receipt.sdk_after | ConvertTo-Json -Depth 24 -Compress) -or
      ($receipt.libraries | ConvertTo-Json -Depth 24 -Compress) -cne ($receipt.libraries_after | ConvertTo-Json -Depth 24 -Compress)) {
    throw 'Official compiler/header/library changed'
  }
  $after = State; $receipt.after = $after
  if (($before | ConvertTo-Json -Depth 24 -Compress) -cne ($after | ConvertTo-Json -Depth 24 -Compress)) {
    throw 'Original target state changed'
  }
  $receipt.object_count = @($receipt.outputs | Where-Object { $_.path -like '*.obj' }).Count
  $receipt.pe_count = @($receipt.outputs | Where-Object { $_.path -like '*.exe' -or $_.path -like '*.dll' }).Count
  if ($receipt.object_count -ne 4 -or $receipt.pe_count -ne 3) { throw 'Original output count mismatch' }
  $receipt.status = 'PASS'; Save-Receipt
} catch {
  $receipt.status = 'FAIL'; $receipt.error = $_.Exception.Message
  try { $receipt.failure_state = State } catch { $receipt.failure_state_error = $_.Exception.Message }
  Save-Receipt; throw
}
