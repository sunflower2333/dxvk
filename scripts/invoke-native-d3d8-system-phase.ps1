param(
  [Parameter(Mandatory)][ValidateSet('Prepare','Start','Run','Result')][string]$Action,
  [Parameter(Mandatory)][ValidateSet('names','enumerate','offscreen','present')][string]$Phase,
  [Parameter(Mandatory)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$RunId,
  [string]$Manifest,[ValidatePattern('^[0-9a-f]{64}$')][string]$ManifestHash,
  [string]$PriorAdmission,[ValidatePattern('^[0-9a-f]{64}$')][string]$PriorAdmissionHash,
  [string]$Authorization,[ValidatePattern('^[0-9a-f]{64}$')][string]$AuthorizationHash
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Require([bool]$Value,[string]$Message) { if (!$Value) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Read([string]$Path) { [IO.File]::ReadAllText($Path) | ConvertFrom-Json }
$root='C:\Users\Public\DxvkD3D8SystemPhase-'+$Phase+'-'+$RunId
$name='VioGpu-D3D8-System-'+$Phase+'-'+$RunId
$configPath=Join-Path $root 'task-config-original.json'
$done=Join-Path $root 'task-result-original.json'
$helpers=Join-Path $root 'helpers'
$powershell='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
if ($Action -ceq 'Prepare') {
  Require (!(Test-Path -LiteralPath $root) -and !(Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue)) 'Fresh owned phase and task required'
  Require ($ManifestHash -cmatch '^[0-9a-f]{64}$' -and (Hash $Manifest) -ceq $ManifestHash) 'Reviewed actual-input manifest required'
  $pins=Read $Manifest
  Require ($pins.schema -ceq 'system-d3d8-phase-inputs-v1' -and $pins.ready -and $pins.native_cpu.accepted) 'Native CPU originals have not been admitted'
  $expected=@('run-native-d3d8-system-phase.ps1','invoke-native-d3d8-system-phase.ps1','collect-native-d3d8-system-phase.ps1','owned-raw-process-f4bf37f-02.cs','inspect-process-token.ps1','inspect-viogpu-readiness-fast-02.ps1','verify-registration-originals-09.py')
  Require ((@($pins.helpers.name|Sort-Object)-join ',') -ceq (@($expected|Sort-Object)-join ',')) 'Exact reviewed helper set required'
  Require ($pins.user.account -ceq 'DROIDVM\USER' -and $pins.user.sid -ceq 'S-1-5-21-362894365-441372107-2852668596-1000') 'Original USER identity required'
  $sid=([Security.Principal.NTAccount]$pins.user.account).Translate([Security.Principal.SecurityIdentifier])
  Require ($sid.Value -ceq $pins.user.sid) 'Resolved original USER SID mismatch'
  $explorer=Get-Process -Id 4464
  Require ($explorer.ProcessName -ceq 'explorer' -and $explorer.SessionId -eq 1) 'Original USER desktop changed'
  New-Item -ItemType Directory -Path $root,$helpers | Out-Null
  $created=$false
  try {
    $acl=Get-Acl -LiteralPath $root
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($sid,'Modify','ContainerInherit,ObjectInherit','None','Allow')))
    Set-Acl -LiteralPath $root $acl
    Copy-Item -LiteralPath $Manifest -Destination (Join-Path $root 'manifest-original.json')
    foreach ($row in @($pins.helpers)) {
      Require ($row.name -ceq [IO.Path]::GetFileName($row.name) -and (Hash (Join-Path $PSScriptRoot $row.name)) -ceq $row.sha256) 'Reviewed original helper changed'
      Copy-Item -LiteralPath (Join-Path $PSScriptRoot $row.name) -Destination (Join-Path $helpers $row.name)
    }
    $prior=$null
    if ($Phase -cne 'names') {
      Require ($PriorAdmissionHash -cmatch '^[0-9a-f]{64}$' -and (Hash $PriorAdmission) -ceq $PriorAdmissionHash) 'Previous phase must be independently reviewed'
      $prior=Join-Path $root 'prior-admission-original.json'
      Copy-Item -LiteralPath $PriorAdmission -Destination $prior
    } else { Require (!$PriorAdmission -and !$PriorAdmissionHash) 'Names phase has no prior runtime admission' }
    $config=[ordered]@{schema='system-d3d8-task-v1';phase=$Phase;run_id=$RunId;task_name=$name;output=(Join-Path $root 'output');
      manifest=(Join-Path $root 'manifest-original.json');manifest_sha256=$ManifestHash;helpers=$pins.helpers;
      prior_admission=$prior;prior_admission_sha256=$PriorAdmissionHash;task_definition_sha256=$null}
    $script=Join-Path $helpers 'invoke-native-d3d8-system-phase.ps1'
    $arguments='-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "'+$script+'" -Action Run -Phase '+$Phase+' -RunId '+$RunId
    $taskAction=New-ScheduledTaskAction -Execute $powershell -Argument $arguments
    $principal=New-ScheduledTaskPrincipal -UserId $pins.user.account -LogonType Interactive -RunLevel Limited
    $settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Seconds 90)
    Register-ScheduledTask -TaskName $name -Action $taskAction -Principal $principal -Settings $settings | Out-Null
    $created=$true
    $definition=Join-Path $root 'task-definition-original.xml'
    Export-ScheduledTask -TaskName $name | Set-Content -LiteralPath $definition -Encoding UTF8
    $config.task_definition_sha256=Hash $definition
    $config|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $configPath -Encoding UTF8
  } catch {
    $_.Exception.ToString()|Set-Content -LiteralPath (Join-Path $root 'prepare-failure-original.txt') -Encoding UTF8
    if ($created -or (Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue)) { Unregister-ScheduledTask -TaskName $name -Confirm:$false }
    throw
  }
  'SYSTEM_D3D8_TASK_PREPARED='+$name+'; runtime_execution=0'
  exit 0
}
$config=Read $configPath
Require ($config.schema -ceq 'system-d3d8-task-v1' -and $config.phase -ceq $Phase -and $config.run_id -ceq $RunId -and $config.task_name -ceq $name) 'Original task configuration mismatch'
Require ((Hash $config.manifest) -ceq $config.manifest_sha256) 'Frozen actual inputs changed'
foreach ($row in @($config.helpers)) { Require ((Hash (Join-Path $helpers $row.name)) -ceq $row.sha256) 'Frozen task helper changed' }
if ($Action -ceq 'Start') {
  Require (!(Test-Path -LiteralPath $done) -and !(Test-Path -LiteralPath (Join-Path $root 'start-original.json'))) 'Never retry an existing attempt'
  $definition=Join-Path $root 'task-definition-before-start.xml'
  Export-ScheduledTask -TaskName $name|Set-Content -LiteralPath $definition -Encoding UTF8
  Require ((Hash $definition) -ceq $config.task_definition_sha256) 'Original Limited task definition changed'
  Require ($AuthorizationHash -cmatch '^[0-9a-f]{64}$' -and (Hash $Authorization) -ceq $AuthorizationHash) 'Explicit exclusive phase authorization required'
  $auth=Read $Authorization
  Require ($auth.authorized -and $auth.owner -cin @('/root','/root/verify_ewdk_build') -and $auth.phase -ceq $Phase -and $auth.manifest_sha256 -ceq $config.manifest_sha256 -and $auth.output -ceq $config.output) 'Authorization is for a different attempt'
  Copy-Item -LiteralPath $Authorization -Destination (Join-Path $root 'authorization-original.json')
  [ordered]@{config_sha256=(Hash $configPath);authorization_sha256=$AuthorizationHash}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'start-original.json') -Encoding UTF8
  Start-ScheduledTask -TaskName $name
  'SYSTEM_D3D8_TASK_STARTED='+$name
  exit 0
}
if ($Action -ceq 'Run') {
  $result=[ordered]@{schema='system-d3d8-task-result-v1';phase=$Phase;run_id=$RunId;completed=$false;worker_started=$false;process_passed=$false;failure=$null}
  try {
    $start=Read (Join-Path $root 'start-original.json')
    Require ((Hash $configPath) -ceq $start.config_sha256) 'Task configuration changed after explicit start'
    $workerArgs=@{Phase=$Phase;Manifest=$config.manifest;ManifestHash=$config.manifest_sha256;Output=$config.output;Helpers=$helpers;
      Authorization=(Join-Path $root 'authorization-original.json');AuthorizationHash=$start.authorization_sha256}
    if ($Phase -cne 'names') { $workerArgs.PriorAdmission=$config.prior_admission;$workerArgs.PriorAdmissionHash=$config.prior_admission_sha256 }
    $result.worker_started=$true
    & (Join-Path $helpers 'run-native-d3d8-system-phase.ps1') @workerArgs *> (Join-Path $root 'runner-original.log')
    $result.process_passed=$true
  } catch { $result.failure=$_.Exception.ToString() }
  finally { $result.completed=$true;$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $done -Encoding UTF8 }
  if (!$result.process_passed -or $result.failure) { exit 1 }
  exit 0
}
$clock=[Diagnostics.Stopwatch]::StartNew()
do {
  $task=Get-ScheduledTask -TaskName $name
  if ($task.State -ne 'Running' -and (Test-Path -LiteralPath $done)) { break }
  Start-Sleep -Milliseconds 200
} while ($clock.Elapsed.TotalSeconds -lt 95)
$info=Get-ScheduledTaskInfo -TaskName $name
$collection=[ordered]@{schema='system-d3d8-task-collection-v1';phase=$Phase;task_name=$name;state=[string]$task.State;task_exit=$info.LastTaskResult;task_removed=$false;child_closed=$false;unresolved=$true}
Export-ScheduledTask -TaskName $name|Set-Content -LiteralPath (Join-Path $root 'task-definition-after.xml') -Encoding UTF8
$collection.task_definition_retained=(Hash (Join-Path $root 'task-definition-after.xml')) -ceq $config.task_definition_sha256
if ($task.State -ne 'Running' -and (Test-Path -LiteralPath $done)) {
  $result=Read $done
  if ($result.completed) {
    if (!$result.worker_started) { $collection.child_closed=$true }
    elseif (Test-Path -LiteralPath (Join-Path $config.output 'result-original.json')) {
      $worker=Read (Join-Path $config.output 'result-original.json')
      $collection.child_closed=$worker.completed -and (!$worker.child -or ($worker.child.Exited -and $worker.child.ExitCodeAvailable -and $worker.child.PipesDrained -and !$worker.child.ChildStillRunning))
    }
  }
  if ($collection.child_closed) {
    Unregister-ScheduledTask -TaskName $name -Confirm:$false
    $collection.task_removed=!(Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue)
    $collection.unresolved=!$collection.task_removed
  }
}
$collection|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $root 'task-collection-original.json') -Encoding UTF8
Require (!$collection.unresolved) 'Owned task or child closure is unproven; preserve evidence and ownership for explicit diagnosis'
Require ($collection.task_definition_retained -and $collection.task_exit -eq 0 -and $result.completed -and $result.process_passed -and !$result.failure) 'Original attempt failed; closed owned task removed, originals retained'
'SYSTEM_D3D8_TASK_CLOSED='+$name+'; independent_admission=pending'
