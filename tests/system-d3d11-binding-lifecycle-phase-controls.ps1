# Actual Restore/Cleanup/Settle source with memory/task/process doubles only.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Fresh CPU phase output required'}
$before=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)
if(@($errors).Count -ne 0){throw 'Actual production PS5.1 AST must pass'}
foreach($name in @('Hash','Write-Json','Write-MutationIntent','Raw-Rows','Quote','Restore-Tuple','Lifecycle-Enabled','Lifecycle-Cleanup','Check-LifecycleIntent','Settle-Lifecycle')){
 $functions=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
 if($functions.Count -ne 1){throw 'One actual production phase function required'};Invoke-Expression $functions[0].Extent.Text
}
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
public sealed class DxvkBindingRawValue01 { public uint View,Type; public string Name; public bool Exists; public byte[] Data; }
public sealed class PhaseNames01 { public string[] Slots; }
public static class DxvkBindingNative01 {
 public static DxvkBindingRawValue01[] Current; public static int Restores; public static List<string> Trace=new List<string>();
 public static DxvkBindingRawValue01[] Snapshot(string unused){return Current;}
 public static void Restore(string unused,DxvkBindingRawValue01[] value){Trace.Add("raw-restore");Restores++;Current=value;}
 public static void RequireSnapshot(DxvkBindingRawValue01[] a,DxvkBindingRawValue01[] b){if(a.Length!=6 || b.Length!=6)throw new Exception();for(int i=0;i<6;i++){if(a[i].View!=b[i].View || a[i].Name!=b[i].Name || a[i].Exists!=b[i].Exists || a[i].Type!=b[i].Type || a[i].Data.Length!=b[i].Data.Length)throw new Exception();for(int k=0;k<a[i].Data.Length;k++)if(a[i].Data[k]!=b[i].Data[k])throw new Exception();}}
 public static void RequireLifecycleRestartPermit(bool raw,bool worker,bool probe){if(!raw || !worker || !probe)throw new Exception("Live producer denies reverse");}
 public static void RequireLifecycleNames(string unused,string[] expected,PhaseNames01 actual){if(actual==null || actual.Slots.Length!=3 || expected.Length!=3)throw new Exception();for(int i=0;i<3;i++)if(actual.Slots[i]!=expected[i])throw new Exception("Selection differs");}
}
'@
$checks=0;$rows=[Collections.Generic.List[object]]::new()
function Check([bool]$v){$script:checks++;if(!$v){throw ('CPU lifecycle phase control '+$script:checks)}}
function Original([byte]$Sentinel){@(0..5|ForEach-Object {$r=[DxvkBindingRawValue01]::new();$r.View=256;$r.Type=7;$r.Name='memory'+$_;$r.Exists=$true;$r.Data=[byte[]]@($Sentinel,$_);$r})}
function State([bool]$Mesa){$names=[PhaseNames01]::new();$names.Slots=@('mesa0','mesa1',$(if($Mesa){'mesa2'}else{'candidate'}));[ordered]@{identity=[ordered]@{Luid='00000000:000092ab';Generation=2};names=$names;instance='MEMORY-INSTANCE'}}
function Get-ScheduledTask {param([string]$TaskName,[string]$ErrorAction);$script:task}
function Stop-ScheduledTask {param([string]$TaskName);[DxvkBindingNative01]::Trace.Add('stop-worker');$script:task.State='Ready';$script:stopped=$true}
function Lifecycle-OriginalProcess([int]$PidValue,[string]$Start,[string]$Executable,[string]$ExpectedHash,[int]$WaitMs,[bool]$Kill){
 [DxvkBindingNative01]::Trace.Add($(if($PidValue -eq 100){'observe-worker'}else{'observe-probe'}))
 $gone=$true;if($PidValue -eq 100){$gone=($script:case -notin @('worker-survives-stop','worker-needs-stop') -or ($script:stopped -and $script:case -eq 'worker-needs-stop'))}else{$gone=$script:case -ne 'live-probe'}
 [ordered]@{pid=$PidValue;gone=$gone;pid_reused=($script:case -eq 'reused-worker');original_exit_claim=$false}
}
function Lifecycle-AcquireJob($Value){[DxvkBindingNative01]::Trace.Add('retain-original-job');[ordered]@{retained_handle=77;previous_boot=$false}}
function Lifecycle-CompleteJob($Scope){[DxvkBindingNative01]::Trace.Add('wait-job-zero');if($script:case -eq 'unheld-blocked-teardown'){throw 'Original job still contains terminating unpublished probe'};[ordered]@{active_processes=0;exact_job_completion=$true;previous_boot=$false}}
function Wait-LifecycleState($Value){[DxvkBindingNative01]::Trace.Add('query-state');if($script:case -eq 'unready-forward-KMD' -and !$script:reversed){throw 'No current paired160'};State ($script:case -eq 'already-Mesa' -or $script:reversed)}
function Lifecycle-Restart($Value,[string]$Phase){
 [DxvkBindingNative01]::Trace.Add('reverse');Check ((Test-Path -LiteralPath (Join-Path $Value.control 'restored.json')) -and (Test-Path -LiteralPath (Join-Path $Value.control 'release.json')))
 if($script:case -eq 'reverse-failed'){throw 'Actual restart did not close0'};$script:reversed=$true
 [ordered]@{state=(State ($script:case -ne 'still-candidate'));receipt=[ordered]@{exit_code=0}}
}
function Wait-LifecycleDesktop($Value){[DxvkBindingNative01]::Trace.Add('desktop');if($script:case -eq 'desktop-unhealthy'){throw 'Wrong/missing original desktop session'};[ordered]@{healthy=$true;pid_retention_required=$false}}
$work=$Output+'.cases';New-Item -ItemType Directory -Path $work|Out-Null
foreach($caseName in @('default-path','unused-stale-backup','reverse-success-new-LUID','already-Mesa','worker-needs-stop','reused-worker','live-probe','worker-survives-stop','unready-forward-KMD','unheld-started','unheld-blocked-teardown','reverse-failed','still-candidate','desktop-unhealthy','foreign-forward-intent','foreign-reverse-intent')){
 $script:case=$caseName;$script:stopped=$false;$script:reversed=$false
 [DxvkBindingNative01]::Restores=0;[DxvkBindingNative01]::Trace.Clear()
 $dir=Join-Path $work $caseName;$control=Join-Path $dir 'controller';$outputDir=Join-Path $dir 'worker';New-Item -ItemType Directory -Path $control,$outputDir|Out-Null
 $value=[pscustomobject]@{lifecycle_mode=($caseName -ne 'default-path');control=$control;output=$outputDir;original=(Original 11);owner_pid=$PID;registry_subkey='MEMORY-ONLY';native_slot=2;mutex=('Local\LifecyclePhase-'+[Guid]::NewGuid().ToString('N'));luid='00000000:00006bec';instance='MEMORY-INSTANCE';original_slots=@('mesa0','mesa1','mesa2');desktop_sid='MEMORY-SID';desktop_session=1;task_name='MEMORY-TASK';lifecycle_controller='C:\MEMORY\controller.ps1';lifecycle_powershell='C:\MEMORY\powershell.exe';lifecycle_powershell_sha256=('1'*64);probe='C:\MEMORY\probe.exe';probe_sha256=('2'*64)}
 [DxvkBindingNative01]::Current=Original 22
 if($caseName -ne 'unused-stale-backup'){Write-MutationIntent (Join-Path $control 'mutation-intent.json') ([ordered]@{schema=2;owner_pid=$PID;registry_subkey='MEMORY-ONLY';selected_native_slot=2;only_native_tuple_slot=$true})}
 Write-Json (Join-Path $control 'config.json') ([ordered]@{memory_only=$true})
 $arguments='-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File '+(Quote $value.lifecycle_controller)+' -Role Worker -Config '+(Quote (Join-Path $control 'config.json'))+' -ConfigSha256 '+(Hash (Join-Path $control 'config.json'))
 $script:task=[pscustomobject]@{State='Running';Actions=@([pscustomobject]@{Execute=$value.lifecycle_powershell;Arguments=$arguments})}
 Write-Json (Join-Path $control 'lifecycle-worker-original.json') ([ordered]@{pid=100;start_utc='2026-10-09T00:00:00.0000000Z';sid='MEMORY-SID';session=1;worker_job_handle=1})
 if($caseName -notin @('unheld-started','unheld-blocked-teardown')){Write-Json (Join-Path $outputDir 'worker-held.json') ([ordered]@{pid=200;start_utc='2026-10-09T00:00:01.0000000Z'})}
 if($caseName -eq 'foreign-forward-intent'){Write-Json (Join-Path $control 'forward-lifecycle-intent.json') ([ordered]@{schema=1;owner_pid=($PID+1);instance=$value.instance;registry_subkey=$value.registry_subkey;original_backup_luid=$value.luid})}
 if($caseName -eq 'foreign-reverse-intent'){Write-Json (Join-Path $control 'reverse-lifecycle-intent.json') ([ordered]@{schema=1;owner_pid=($PID+1);instance=$value.instance;registry_subkey=$value.registry_subkey;original_backup_luid=$value.luid})}
 $failed=$false;$settled=$null;try{$settled=Restore-Tuple $value 'CPU-control'}catch{$failed=$true}
 $expectedFail=$caseName -in @('live-probe','worker-survives-stop','unheld-blocked-teardown','reverse-failed','still-candidate','desktop-unhealthy','foreign-forward-intent','foreign-reverse-intent')
 Check ($failed -eq $expectedFail)
 $trace=@([DxvkBindingNative01]::Trace);$reverseIndex=[Array]::IndexOf($trace,'reverse')
 if($caseName -eq 'unused-stale-backup'){Check ([DxvkBindingNative01]::Restores -eq 0);Check ($settled.cancelled_without_registry_write);Check ($trace.Count -eq 0)}
 else{Check ([DxvkBindingNative01]::Restores -eq 1);Check ($trace[0] -ceq 'raw-restore');Check (Test-Path -LiteralPath (Join-Path $control 'restored.json'))}
 if($caseName -eq 'default-path'){Check ($trace.Count -eq 1)}
 if($caseName -in @('live-probe','worker-survives-stop','unheld-blocked-teardown','foreign-forward-intent','foreign-reverse-intent','already-Mesa')){Check ($reverseIndex -eq -1)}
 if($reverseIndex -ge 0){Check ($reverseIndex -gt [Array]::IndexOf($trace,'observe-worker'));Check ($reverseIndex -gt [Array]::IndexOf($trace,'observe-probe'));Check ($reverseIndex -gt [Array]::IndexOf($trace,'wait-job-zero'))}
 if(!$failed -and $caseName -notin @('default-path','unused-stale-backup')){Check ($settled.lifecycle.restored_luid -ceq '00000000:000092ab');Check ($settled.lifecycle.original_backup_luid -ceq '00000000:00006bec');Check ($settled.lifecycle.effective_original_names_restored)}
 if($failed){Check (@(Get-ChildItem -LiteralPath $control -Filter 'lifecycle-restored-*.json').Count -eq 0)}
 $rows.Add([ordered]@{case=$caseName;failed=$failed;trace=$trace;reverse_started=($reverseIndex -ge 0);hardware_admission=$false})
}
Check ($before -ceq (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant())
[ordered]@{schema='same-owner-lifecycle-memory-phase-controls-v1';passed=$true;source_sha256=$before;cases=$rows;checks=$checks;actual_source_functions=10;native_calls=0;registry_changes=0;task_changes=0;GPU_calls=0;hardware_admission=$false}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $Output -Encoding UTF8
'SYSTEM_D11_LIFECYCLE_PHASE_PASS cases=16 checks='+$checks+' native_calls=0 registry_changes=0 gpu_calls=0 hardware_admission=0'
