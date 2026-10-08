param(
  [Parameter(Mandatory)][ValidateSet('names','enumerate','offscreen','present')][string]$Phase,
  [Parameter(Mandatory)][string]$Manifest,
  [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ManifestHash,
  [Parameter(Mandatory)][string]$Output,
  [Parameter(Mandatory)][string]$Helpers,
  [Parameter(Mandatory)][string]$Authorization,
  [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$AuthorizationHash,
  [string]$PriorAdmission,
  [ValidatePattern('^[0-9a-f]{64}$')][string]$PriorAdmissionHash
)
# Each invocation is one separately authorized USER phase. Never advance
# phases from an in-process success flag: the host reopens originals first.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Require([bool]$Condition,[string]$Message) { if (!$Condition) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Json([string]$Path) { [IO.File]::ReadAllText($Path) | ConvertFrom-Json }
function File([string]$Path) {
  $item=Get-Item -LiteralPath $Path
  [ordered]@{path=$item.FullName;bytes=$item.Length;sha256=(Hash $Path);version=$item.VersionInfo.FileVersion}
}
function Machine([string]$Path) {
  $bytes=[IO.File]::ReadAllBytes($Path)
  Require ($bytes.Length -ge 64 -and [BitConverter]::ToUInt16($bytes,0) -eq 0x5a4d) 'Original PE DOS header required'
  $offset=[BitConverter]::ToUInt32($bytes,0x3c)
  Require ($offset -le $bytes.Length-6 -and [BitConverter]::ToUInt32($bytes,$offset) -eq 0x4550) 'Original PE signature required'
  [int][BitConverter]::ToUInt16($bytes,$offset+4)
}
function Pins([object]$Config) {
  $seen=@{};$values=@()
  foreach ($pin in @($Config.files)) {
    if ($Phase -notin @($pin.phases)) { continue }
    Require (!$seen.ContainsKey($pin.role)) 'Duplicate selected phase input role'
    $seen[$pin.role]=$true
    Require ([IO.Path]::IsPathRooted($pin.path) -and !$pin.path.Contains('"') -and !$pin.path.EndsWith('\')) 'Canonical absolute input path required'
    Require ($pin.sha256 -cmatch '^[0-9a-f]{64}$' -and $pin.bytes -gt 0) 'Actual original input pin required'
    $actual=File $pin.path
    Require ($actual.sha256 -ceq $pin.sha256 -and $actual.bytes -eq $pin.bytes) ('Original phase input changed: '+$pin.role)
    if ($pin.role -in @('probe','frontend','core','loader','icd')) { Require ((Machine $pin.path) -eq 0x14c) 'Matching original I386 PE required' }
    $values+=@([ordered]@{role=$pin.role;path=$actual.path;bytes=$actual.bytes;sha256=$actual.sha256})
  }
  $expected=if ($Phase -ceq 'names') { @('probe') } else { @('probe','frontend','core','loader','icd','icd-json') }
  Require ((@($seen.Keys|Sort-Object)-join ',') -ceq (@($expected|Sort-Object)-join ',')) 'Exact phase input roles required'
  return $values
}
Require (!(Test-Path -LiteralPath $Output)) 'Use fresh original phase evidence'
Require ($Output -cmatch '^C:\\Users\\Public\\DxvkD3D8SystemPhase-(names|enumerate|offscreen|present)-[a-zA-Z0-9-]+\\output$') 'Canonical owned phase evidence root required'
Require ((Hash $Manifest) -ceq $ManifestHash) 'Reviewed manifest changed'
$config=Json $Manifest
Require ($config.schema -ceq 'system-d3d8-phase-inputs-v1' -and $config.ready) 'Actual native CPU/source inputs are still pending'
Require ($config.probe_source -ceq 'e3ac12646a1b55742d575109063ae78507af5cec' -and $config.core_source -ceq 'de72dc2e97bd8e4ea70c5bf89c26918d06065723' -and [string]$config.core_ci_run -ceq '37711793677') 'Exact separate harness/production identities required'
Require ($config.loader_source -ceq '6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7' -and $config.icd_source -ceq '8443c71a5ab32b9d58b904fa51f4bf2f9089db8d') 'Original distinct loader and ICD sources required'
Require ($config.adapter_luid -ceq 'ec6b000000000000' -and $config.source_id -eq 0) 'Fresh selected adapter identity required'
Require ($config.native_cpu.accepted -and $config.native_cpu.source -ceq $config.probe_source -and $config.native_cpu.original_archive_sha256 -ceq '9ab984590d37ba47a761d81ea76fbc42a8ed87666ca67ced1ef8f107e31b7ddc' -and $config.native_cpu.original_proof_sha256 -cmatch '^[0-9a-f]{64}$' -and $config.native_cpu.completion_archive_sha256 -cmatch '^[0-9a-f]{64}$' -and $config.native_cpu.posthash_original_sha256 -ceq 'd9e37a95907f0274ca4b962abba0e6b5052fcd96ea60a69e3a781ba0f74584eb') 'Accepted original split native CPU proof required'
Require ((Hash $Authorization) -ceq $AuthorizationHash) 'Explicit phase authorization changed'
$auth=Json $Authorization
Require ($auth.authorized -and $auth.owner -cin @('/root','/root/verify_ewdk_build') -and $auth.phase -ceq $Phase -and $auth.manifest_sha256 -ceq $ManifestHash -and $auth.output -ceq $Output) 'Exact exclusive phase ownership required'
foreach ($name in @('VIOGPU_DXVK_RUNTIME_DIAGNOSTIC','VIOGPU_DXVK_D3D8_CORE_PATH','VIOGPU_DXVK_D3D8_CORE_SHA256','VIOGPU_DXVK_D3D8_CORE_COMMIT','VK_DRIVER_FILES','VK_ICD_FILENAMES')) {
  Require (!(Test-Path ('Env:'+$name))) 'Original diagnostic and Vulkan environment must be absent'
}
$helperNames=@('run-native-d3d8-system-phase.ps1','invoke-native-d3d8-system-phase.ps1','collect-native-d3d8-system-phase.ps1','owned-raw-process-f4bf37f-02.cs','inspect-process-token.ps1','inspect-viogpu-readiness-fast-02.ps1','verify-registration-originals-09.py')
Require ((@($config.helpers.name | Sort-Object)-join ',') -ceq (@($helperNames | Sort-Object)-join ',')) 'Exact seven retained helpers required'
foreach ($pin in @($config.helpers)) {
  Require ($pin.name -ceq [IO.Path]::GetFileName($pin.name)) 'Safe helper filename required'
  Require ((Hash (Join-Path $Helpers $pin.name)) -ceq $pin.sha256) ('Frozen helper changed: '+$pin.name)
}
$token=& (Join-Path $Helpers 'inspect-process-token.ps1')
Require ($token.sid -ceq 'S-1-5-21-362894365-441372107-2852668596-1000' -and $token.session_id -eq 1 -and !$token.elevated -and $token.elevation_type -eq 3 -and $token.integrity_rid -eq 8192) 'Actual original limited USER/session1 token required'
Require ($token.user -ceq $config.user.account -and $token.sid -ceq $config.user.sid) 'Pinned original USER account required'
$before=@(Pins $config)
$previous=$null;$registered=$null
if ($Phase -cne 'names') {
  Require ($PriorAdmissionHash -cmatch '^[0-9a-f]{64}$' -and (Hash $PriorAdmission) -ceq $PriorAdmissionHash) 'Independent original previous-phase proof required'
  $previous=Json $PriorAdmission
  $expected=@{enumerate='names';offscreen='enumerate';present='offscreen'}[$Phase]
  Require ($previous.schema -ceq 'system-d3d8-phase-admission-v1' -and $previous.verified -and $previous.phase -ceq $expected -and $previous.manifest_sha256 -ceq $ManifestHash -and $previous.probe_source -ceq $config.probe_source -and $previous.core_source -ceq $config.core_source) 'Previous independent original admission does not match this phase'
  Require ($previous.luid -ceq $config.adapter_luid -and $previous.source -eq $config.source_id -and $previous.sid -ceq $token.sid) 'Previous actual USER/adapter identity mismatch'
  $registered=[string]$previous.registered_I386_filename
  Require ($registered.Length -gt 3 -and $registered.Length -lt 260 -and $registered[1] -ceq ':' -and $registered.IndexOfAny([char[]]@([char]0,[char]10,[char]13,[char]34)) -lt 0) 'Actual original I386 KMT filename required'
}
New-Item -ItemType Directory -Path $Output | Out-Null
$result=[ordered]@{schema='system-d3d8-phase-result-v1';phase=$Phase;completed=$false;process_passed=$false;failure=$null;
  probe_source=$config.probe_source;core_source=$config.core_source;core_ci_run=$config.core_ci_run;loader_source=$config.loader_source;icd_source=$config.icd_source;
  manifest_sha256=$ManifestHash;authorization_sha256=$AuthorizationHash;process_token=$token;inputs_before=$before;inputs_after=$null;
  readiness_before=$null;readiness_after=$null;system_before=$null;system_after=$null;child=$null;registered_I386_filename=$registered;
  independent_admission=$false;registry_driver_writes=$false;installation=$false;VM_changes=$false}
function System-Files {
  @('C:\Windows\SysWOW64\d3d8.dll','C:\Windows\SysWOW64\d3d8thk.dll','C:\Windows\SysWOW64\gdi32.dll','C:\Windows\System32\d3d9.dll') | ForEach-Object { File $_ }
}
# Reuse the independently controlled 34-name static registration policy.
# Every raw value remains in readiness originals; unlisted-value equality
# is outside this predicate, rather than a wildcard Native* exclusion.
$policyPath=Join-Path $Helpers 'verify-registration-originals-09.py'
Require ((Hash $policyPath) -ceq '28f29b0e69e06adb78fc2d6da10d3cd3d1709cfbb7da4619c47b0f90cbbbe051') 'Original explicit static registration policy changed'
$policyMatch=[regex]::Match([IO.File]::ReadAllText($policyPath),'(?ms)^PROTECTED = \(\r?\n(.*?)^\)')
$protected=@([regex]::Matches($policyMatch.Groups[1].Value,"'([^']+)'") | ForEach-Object { $_.Groups[1].Value })
Require ($protected.Count -eq 34 -and @($protected|Sort-Object -Unique).Count -eq 34) 'Exact original protected field set required'
function Static-Values([object]$Values) {
  $result=[ordered]@{}
  $names=@($Values.PSObject.Properties.Name)
  Require (@($names|ForEach-Object { $_.ToLowerInvariant() }|Sort-Object -Unique).Count -eq $names.Count) 'Duplicate original registry value name'
  foreach ($name in $protected) {
    $found=@($Values.PSObject.Properties | Where-Object { $_.Name -ceq $name })
    $value=if ($found.Count) { $found[0].Value } else { $null }
    $result[$name]=[ordered]@{present=($found.Count -eq 1);value=$value}
  }
  $result
}
function Critical([object]$Snapshot) {
  $active=[ordered]@{}
  foreach ($property in $Snapshot.active_device.PSObject.Properties) {
    $active[$property.Name]=if ($property.Name -ceq 'values') { Static-Values $property.Value } else { $property.Value }
  }
  Require (@($Snapshot.driver_registry).Count -eq 2) 'Two original class frames required'
  $frames=[ordered]@{}
  foreach ($frame in @($Snapshot.driver_registry|Sort-Object key)) {
    Require (!$frames.Contains($frame.key)) 'Duplicate original driver class frame'
    $frames[$frame.key]=Static-Values $frame.values
  }
  [ordered]@{active_device=$active;driver_registry=$frames;desktop=$Snapshot.desktop}
}
try {
  Copy-Item -LiteralPath $Manifest -Destination (Join-Path $Output 'manifest-original.json')
  Copy-Item -LiteralPath $Authorization -Destination (Join-Path $Output 'authorization-original.json')
  if ($previous) { Copy-Item -LiteralPath $PriorAdmission -Destination (Join-Path $Output 'prior-admission-original.json') }
  $retained=Join-Path $Output 'helpers';New-Item -ItemType Directory -Path $retained | Out-Null
  foreach ($pin in @($config.helpers)) { Copy-Item -LiteralPath (Join-Path $Helpers $pin.name) -Destination (Join-Path $retained $pin.name) }
  $raw=Join-Path $retained 'owned-raw-process-f4bf37f-02.cs'
  Require ((Hash $raw) -ceq 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') 'Native-tested raw process owner required'
  Require (!('DxvkRawProcessF4_02' -as [type])) 'Use fresh USER process for child ownership'
  Add-Type -Path $raw
  [ordered]@{sha256=(Hash $raw);type='DxvkRawProcessF4_02';compiled=$true} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output 'raw-type-original.json') -Encoding UTF8
  & (Join-Path $retained 'inspect-viogpu-readiness-fast-02.ps1') -Output (Join-Path $Output 'readiness-before.json')
  $result.readiness_before=Json (Join-Path $Output 'readiness-before.json');$result.system_before=@(System-Files)
  $systemCopies=Join-Path $Output 'system-originals';New-Item -ItemType Directory -Path $systemCopies | Out-Null
  foreach ($row in $result.system_before) {
    $leaf=if ($row.path -ieq 'C:\Windows\System32\d3d9.dll') { 'system32-d3d9.dll' } else { [IO.Path]::GetFileName($row.path) }
    Copy-Item -LiteralPath $row.path -Destination (Join-Path $systemCopies $leaf)
    Require ((Hash (Join-Path $systemCopies $leaf)) -ceq $row.sha256) 'Original system file copy changed'
  }
  Require ($result.readiness_before.active_device.binary.sha256 -ceq 'd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a') 'Signed installed SYS changed'
  $desktopIds=@($result.readiness_before.desktop.Id | Sort-Object)
  Require (($desktopIds -join ',') -ceq '1864,4464') 'Original DWM/Explorer identity changed'
  $system8=@($result.system_before | Where-Object { $_.path -ieq 'C:\Windows\SysWOW64\d3d8.dll' })[0]
  Require ($system8.sha256 -ceq '65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8' -and $system8.bytes -eq 737280) 'Original genuine Microsoft I386 D3D8 changed'
  $probe=(@($config.files|Where-Object role -CEQ 'probe'))[0].path
  if ($Phase -ceq 'names') { $arguments='--kmt-names '+$config.adapter_luid+' '+$config.source_id }
  else {
    $front=(@($config.files|Where-Object role -CEQ 'frontend'))[0].path
    $core=(@($config.files|Where-Object role -CEQ 'core'))[0]
    $mode=@{enumerate='--front-enumerate';offscreen='--front-offscreen';present='--front-present'}[$Phase]
    $arguments=$mode+' "'+$front+'" "'+$registered+'" "'+$core.path+'" '+$core.sha256+' '+$config.core_source
    $arguments+=' '+$config.adapter_luid+' '+$config.source_id
  }
  $result.command=[ordered]@{executable=$probe;arguments=$arguments;working_directory=$Output;deadline_ms=30000;runner_sha256=(Hash $raw)}
  $result.command|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Output 'command-original.json') -Encoding UTF8
  $child=[DxvkRawProcessF4_02]::Run($probe,$arguments,$Output,(Join-Path $Output 'probe.stdout.raw'),(Join-Path $Output 'probe.stderr.raw'),30000)
  $child|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $Output 'process-original.json') -Encoding UTF8
  $result.child=$child
  Require ($child.ProcessHandle -ne 0 -and $child.Exited -and $child.ExitCodeAvailable -and $child.PipesDrained -and !$child.ChildStillRunning -and !$child.TimedOut -and !$child.Failure -and $child.ExitCode -eq 0) 'Owned original phase failed; no subsequent phase permitted'
  $result.process_passed=$true
} catch { $result.failure=$_.Exception.ToString() }
finally {
  try {
    $result.inputs_after=@(Pins $config);$result.system_after=@(System-Files)
    & (Join-Path $Helpers 'inspect-viogpu-readiness-fast-02.ps1') -Output (Join-Path $Output 'readiness-after.json')
    $result.readiness_after=Json (Join-Path $Output 'readiness-after.json')
    Require (($result.inputs_before|ConvertTo-Json -Depth 8 -Compress) -ceq ($result.inputs_after|ConvertTo-Json -Depth 8 -Compress)) 'Original input bytes changed'
    Require (($result.system_before|ConvertTo-Json -Depth 8 -Compress) -ceq ($result.system_after|ConvertTo-Json -Depth 8 -Compress)) 'Original System32/SysWOW64 libraries changed'
    Require (((Critical $result.readiness_before)|ConvertTo-Json -Depth 16 -Compress) -ceq ((Critical $result.readiness_after)|ConvertTo-Json -Depth 16 -Compress)) 'Original SYS/binding/package/service/PnP/static34/desktop changed'
  } catch { $result.failure=([string]$result.failure+"`nFinal retention: "+$_.Exception.ToString()) }
  $result.completed=$true;$result|ConvertTo-Json -Depth 20|Set-Content -LiteralPath (Join-Path $Output 'result-original.json') -Encoding UTF8
}
Require ($result.process_passed -and !$result.failure) 'Phase failed; original process/output/readiness retained for independent review'
'SYSTEM_D3D8_PHASE_ORIGINALS_RETAINED independent_admission=pending'
