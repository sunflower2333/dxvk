param([Parameter(Mandatory)][string]$EvidenceRoot,[Parameter(Mandatory)][string]$Archive)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Require([bool]$Value,[string]$Message) { if (!$Value) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
Require ($EvidenceRoot -cmatch '^C:\\Users\\Public\\DxvkD3D8SystemPhase-(names|enumerate|offscreen|present)-[a-zA-Z0-9-]+$') 'Exact owned evidence root required'
Require ($Archive -ceq ($EvidenceRoot+'.tar.gz') -and !(Test-Path -LiteralPath $Archive)) 'Fresh adjacent original archive required'
$config=[IO.File]::ReadAllText((Join-Path $EvidenceRoot 'task-config-original.json'))|ConvertFrom-Json
$task=Get-ScheduledTask -TaskName $config.task_name -ErrorAction SilentlyContinue
Require (!$task) 'Finish explicit owned task finalization before collection'
$collection=[IO.File]::ReadAllText((Join-Path $EvidenceRoot 'task-collection-original.json'))|ConvertFrom-Json
Require ($collection.child_closed -and $collection.task_removed -and !$collection.unresolved) 'Original child closure is not proven'
$raw=Join-Path $EvidenceRoot 'helpers\owned-raw-process-f4bf37f-02.cs'
Require ((Hash $raw) -ceq 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') 'Exact native-tested collector owner required'
Add-Type -Path $raw
function Members {
  @(Get-ChildItem -LiteralPath $EvidenceRoot -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{name=$_.FullName.Substring($EvidenceRoot.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Hash $_.FullName)}
  })
}
$before=Members
$result=[ordered]@{schema='system-d3d8-collection-v1';evidence_root=$EvidenceRoot;archive=$Archive;runner_sha256=(Hash $raw);members=$before;process=$null;archive_bytes=$null;archive_sha256=$null;completed=$false;unchanged=$false;failure=$null}
try {
  $arguments='-czf "'+$Archive+'" -C "'+$EvidenceRoot+'" .'
  $result.command=[ordered]@{executable='C:\Windows\System32\tar.exe';arguments=$arguments;working_directory='C:\Users\Public';deadline_ms=60000}
  $child=[DxvkRawProcessF4_02]::Run($result.command.executable,$arguments,$result.command.working_directory,($Archive+'.stdout.raw'),($Archive+'.stderr.raw'),60000)
  $result.process=$child
  Require ($child.ProcessHandle -ne 0 -and $child.Exited -and $child.ExitCodeAvailable -and $child.PipesDrained -and !$child.ChildStillRunning -and !$child.TimedOut -and !$child.Failure -and $child.ExitCode -eq 0) 'Original archive child failed; preserve ownership if incomplete'
  $after=Members
  Require (($before|ConvertTo-Json -Depth 6 -Compress) -ceq ($after|ConvertTo-Json -Depth 6 -Compress)) 'Evidence changed while archiving'
  $result.unchanged=$true
  $result.archive_bytes=(Get-Item -LiteralPath $Archive).Length;$result.archive_sha256=Hash $Archive
} catch { $result.failure=$_.Exception.ToString() }
finally { $result.completed=$true;$result|ConvertTo-Json -Depth 12|Set-Content -LiteralPath ($Archive+'.collection-original.json') -Encoding UTF8 }
Require ($result.unchanged -and !$result.failure) 'Collection failed; original receipt retained'
'SYSTEM_D3D8_ORIGINAL_ARCHIVE='+$Archive+'; sha256='+$result.archive_sha256
