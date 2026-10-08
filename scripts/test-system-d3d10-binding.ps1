# SPDX-License-Identifier: MIT
# Explicit reversible adapter-wide validation experiment. No reset/reboot/caps edits.
param(
    [ValidateSet('Controller','Watchdog','Worker')][string]$Role='Controller',
    [string]$Config='', [string]$ConfigSha256='', [string]$RunRoot='',
    [string]$InstanceId='', [string]$Luid='',
    [ValidateSet('9','9ex','10','11')][string]$Api='10',
    [ValidateSet('offscreen','present')][string]$D9Phase='offscreen',
    [string]$PrivateLoader='', [string]$PrivateLoaderSha256='',
    [string]$Probe='', [string]$ProbeSha256='', [string]$Front='', [string]$FrontSha256='',
    [string]$Core='', [string]$CoreSha256='', [string]$Runner='',
    [string]$DriverSys='', [string]$DriverSysSha256='',
    [string]$VulkanIcd='', [string]$VulkanIcdSha256='',
    [string]$VulkanLibrarySha256='', [string]$VulkanLoaderSha256='',
    [string]$ApprovedPayload='', [string]$ApprovedPayloadSha256='',
    [string]$TokenScript='', [string]$TokenScriptSha256='',
    [switch]$ApplyReviewedTuple, [switch]$WaitForReviewedRefresh
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Check-File([string]$Path,[string]$Sha) {
    if ($Sha -cnotmatch '^[0-9a-f]{64}$' -or !(Test-Path -LiteralPath $Path -PathType Leaf) -or (Hash $Path) -cne $Sha) { throw "Pinned file differs: $Path" }
}
function Held-ModuleCensus($Child,$Value) {
    # Enumerate only the original held child, while retaining its handle. No
    # native entry is called by this census, and no other process is inspected.
    $start=$Child.StartTime.ToUniversalTime().ToString('o'); $handle=$Child.Handle
    $required=[Collections.Generic.List[DxvkApprovedFile01]]::new()
    foreach ($row in $Value.module_files) {
        $file=[DxvkApprovedFile01]::new(); $file.Path=$row.Path; $file.Bytes=$row.Bytes; $file.Sha256=$row.Sha256; $required.Add($file)
    }
    $selectedNames=@($required | ForEach-Object { [IO.Path]::GetFileName($_.Path) })
    $actual=[Collections.Generic.List[DxvkApprovedFile01]]::new()
    foreach ($module in $Child.Modules) {
        $file=[DxvkApprovedFile01]::new(); $file.Path=$module.FileName
        if ([IO.Path]::GetFileName($file.Path) -iin $selectedNames) { $file.Bytes=(Get-Item -LiteralPath $file.Path).Length; $file.Sha256=Hash $file.Path }
        $actual.Add($file)
    }
    [DxvkApprovedPayloadPolicy01]::RequireModules($required.ToArray(),$actual.ToArray())
    if ($Child.HasExited -or $Child.StartTime.ToUniversalTime().ToString('o') -cne $start -or $Child.MainModule.FileName -ine $Value.probe) { throw 'Original held process changed during module census' }
    [ordered]@{schema=1;pid=$Child.Id;start_utc=$start;retained_handle=$handle.ToInt64();source_commit=$Value.payload_source;ci_run=$Value.payload_ci_run;approved_payload_sha256=$Value.approved_payload_sha256;required=$required.ToArray();actual=$actual.ToArray();passed=$true;gpu_calls=0;registry_mutations=0;hardware_admission=$false}
}
function Write-Json([string]$Path,$Value) {
    $temporary=$Path+'.'+$PID+'.new'
    if (Test-Path -LiteralPath $temporary) { throw 'Fresh atomic evidence path required' }
    [IO.File]::WriteAllText($temporary,($Value | ConvertTo-Json -Depth 16),[Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporary,$Path)
}
function Write-MutationIntent([string]$Path,$Value) {
    # Close and force the complete protected intent record before RegSet. A
    # partial intent can only precede a write that has not yet been attempted.
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 8))
    $file=[IO.FileStream]::new($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::WriteThrough)
    try { $file.Write($bytes,0,$bytes.Length); $file.Flush($true) } finally { $file.Dispose() }
}
function Quote([string]$Value) {
    if (!$Value -or $Value.Contains('"') -or $Value.EndsWith('\') -or $Value.Contains([char]0)) { throw 'Unambiguous owned argument required' }
    '"'+$Value+'"'
}
function Process-Receipt($Row,[string]$RunnerSha) {
    [ordered]@{pid=$Row.Pid;retained_process_handle=$Row.ProcessHandle;start_utc=$Row.StartUtc;exited=$Row.Exited;exit_code_available=$Row.ExitCodeAvailable;exit_code=$Row.ExitCode;timed_out=$Row.TimedOut;child_still_running=$Row.ChildStillRunning;pipes_drained=$Row.PipesDrained;stdout_bytes=$Row.StdoutBytes;stderr_bytes=$Row.StderrBytes;seconds=$Row.Seconds;capture_failure=$Row.Failure;expected_exit=0;runner_sha256=$RunnerSha}
}
function Raw-Rows($Rows) {
    $values=[Collections.Generic.List[DxvkBindingRawValue01]]::new()
    foreach ($row in $Rows) {
        $value=[DxvkBindingRawValue01]::new()
        $value.View=[uint32]$row.View; $value.Name=[string]$row.Name; $value.Exists=[bool]$row.Exists
        $value.Type=[uint32]$row.Type; $value.Data=[byte[]]$row.Data; $values.Add($value)
    }
    $values.ToArray()
}
function Check-Names($Names,[string]$ExpectedLuid) {
    if ($Names.Luid -cne $ExpectedLuid -or $Names.OpenStatus -ne 0 -or $Names.CloseStatus -ne 0 -or $Names.Names.Count -ne 3) { throw 'Genuine KMT LUID/open/query/close failed' }
    for ($index=0;$index -lt 3;++$index) {
        if ($Names.Names[$index].Version -ne $index -or $Names.Names[$index].Status -ne 0 -or !$Names.Names[$index].Name) { throw 'Original KMT name query failed' }
    }
}
function Same-Names($First,$Second) {
    if ($First.Luid -cne $Second.Luid -or $First.OpenStatus -ne 0 -or $Second.OpenStatus -ne 0 -or $First.CloseStatus -ne 0 -or $Second.CloseStatus -ne 0) { return $false }
    for ($index=0;$index -lt 3;++$index) {
        if ($First.Names[$index].Status -ne 0 -or $Second.Names[$index].Status -ne 0 -or $First.Names[$index].Name -ine $Second.Names[$index].Name) { return $false }
    }
    $true
}
function Driver-State([string]$Device) {
    $service=[string](Get-PnpDeviceProperty -InstanceId $Device -KeyName 'DEVPKEY_Device_Service').Data
    if ($service -cnotmatch '^[A-Za-z0-9_.-]+$') { throw 'Exact display driver service required' }
    $image=[string](Get-ItemProperty -LiteralPath ('HKLM:\SYSTEM\CurrentControlSet\Services\'+$service) -Name ImagePath).ImagePath
    $image=[Environment]::ExpandEnvironmentVariables($image)
    if ($image.StartsWith('\??\')) { $image=$image.Substring(4) }
    if ($image.StartsWith('\SystemRoot\',[StringComparison]::OrdinalIgnoreCase)) { $image=Join-Path $env:SystemRoot $image.Substring(12) }
    if ($image.StartsWith('System32\',[StringComparison]::OrdinalIgnoreCase)) { $image=Join-Path $env:SystemRoot $image }
    $image=[IO.Path]::GetFullPath($image)
    $signature=Get-AuthenticodeSignature -LiteralPath $image
    $thumbprint=''; if ($signature.SignerCertificate) { $thumbprint=$signature.SignerCertificate.Thumbprint }
    [ordered]@{service=$service;image=$image;sha256=(Hash $image);signature_status=$signature.Status.ToString();signer_thumbprint=$thumbprint;inf=[string](Get-PnpDeviceProperty -InstanceId $Device -KeyName 'DEVPKEY_Device_DriverInfPath').Data;version=[string](Get-PnpDeviceProperty -InstanceId $Device -KeyName 'DEVPKEY_Device_DriverVersion').Data;provider=[string](Get-PnpDeviceProperty -InstanceId $Device -KeyName 'DEVPKEY_Device_DriverProvider').Data}
}
function Restore-Tuple($Value,[string]$Reason) {
    $mutex=[Threading.Mutex]::new($false,$Value.mutex)
    $locked=$false
    try {
        try { $locked=$mutex.WaitOne(10000) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (!$locked) { throw 'Restoration mutex deadline exceeded' }
        $rows=Raw-Rows $Value.original
        $intent=Join-Path $Value.control 'mutation-intent.json'
        $replay=Test-Path -LiteralPath $intent
        if ($replay) {
            $phase=Get-Content -LiteralPath $intent -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($phase.schema -ne 2 -or $phase.owner_pid -ne $Value.owner_pid -or $phase.registry_subkey -cne $Value.registry_subkey -or $phase.selected_native_slot -ne $Value.native_slot -or $Value.native_slot -notin @(0,1,2) -or $phase.only_native_tuple_slot -ne $true) { throw 'Protected mutation intent differs from this backup owner/selected slot' }
            # Intent is durable BEFORE RegSet; a partial/failed write still needs
            # raw replay. Never replay an unused pre-write backup on cancellation.
            [DxvkBindingNative01]::Restore($Value.registry_subkey,$rows)
        }
        $actual=[DxvkBindingNative01]::Snapshot($Value.registry_subkey)
        $matches=$false
        try { [DxvkBindingNative01]::RequireSnapshot($rows,$actual); $matches=$true }
        catch { if ($replay) { throw } }
        $settled=[ordered]@{schema=1;actor_pid=$PID;reason=$Reason;utc=[DateTime]::UtcNow.ToString('o');mutation_intent_present=$replay;backup_replayed=$replay;raw_tuple_restored=($replay -and $matches);cancelled_without_registry_write=(!$replay);original_readback_matches=$matches;original=$Value.original;actual=$actual;hardware_admission=$false}
        $proof=Join-Path $Value.control 'restored.json'
        if (!(Test-Path -LiteralPath $proof)) {
            Write-Json $proof $settled
        }
        # This protected file is writable only by administrators/SYSTEM; USER
        # can read it. The interactive worker signals its own Local event.
        $release=Join-Path $Value.control 'release.json'
        if (!(Test-Path -LiteralPath $release)) { Write-Json $release ([ordered]@{restored_sha256=(Hash $proof);raw_tuple_restored=$settled.raw_tuple_restored;cancelled_without_registry_write=$settled.cancelled_without_registry_write}) }
        return $settled
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
}
function Remove-OwnedTask($Value) {
    $task=Get-ScheduledTask -TaskName $Value.task_name -ErrorAction SilentlyContinue
    if ($task) {
        if ($task.State -eq 'Running') { Stop-ScheduledTask -TaskName $Value.task_name }
        Unregister-ScheduledTask -TaskName $Value.task_name -Confirm:$false
    }
}
function Close-HeldProbeIfWorkerLost($Value) {
    $held=Join-Path $Value.output 'worker-held.json'
    if (!(Test-Path -LiteralPath $held)) { return }
    $row=Get-Content -LiteralPath $held -Raw -Encoding UTF8 | ConvertFrom-Json
    $child=$null
    try {
        $child=[Diagnostics.Process]::GetProcessById([int]$row.pid)
        $handle=$child.Handle # Retain this exact process object before validation.
        if ($child.StartTime.ToUniversalTime().ToString('o') -cne $row.start_utc -or $child.MainModule.FileName -ine $Value.probe -or (Hash $child.MainModule.FileName) -cne $Value.probe_sha256) { throw 'Held probe identity changed; refusing unrelated process termination' }
        if (!$child.WaitForExit(10000)) { $child.Kill(); if (!$child.WaitForExit(5000)) { throw 'Owned held probe survived bounded cleanup' } }
        Write-Json (Join-Path $Value.control ('probe-cleanup-'+$PID+'.json')) ([ordered]@{pid=$row.pid;retained_handle=$handle.ToInt64();exited=$child.HasExited;exit_code=$child.ExitCode;scope='Recovery cleanup only; not a successful producer receipt'})
    } catch [ArgumentException] { # A process that already exited has no live PID.
        Write-Json (Join-Path $Value.control ('probe-cleanup-'+$PID+'.json')) ([ordered]@{pid=$row.pid;already_absent=$true;scope='No original exit-code claim'})
    } finally { if ($child) { $child.Dispose() } }
}

$native=Join-Path $PSScriptRoot 'system-d3d10-binding-native.cs'
if ($Role -ne 'Controller') {
    Check-File $Config $ConfigSha256
    $value=Get-Content -LiteralPath $Config -Raw -Encoding UTF8 | ConvertFrom-Json
    # A changed probe/payload must stop new GPU work, not prevent replaying the
    # protected raw backup. Only the restoration implementation is required
    # before the watchdog can restore; the worker checks every selected input.
    if ($Role -eq 'Worker') {
        foreach ($file in $value.files) { Check-File $file.path $file.sha256 }
        foreach ($pair in @(@($value.probe,$value.probe_sha256),@($value.front,$value.front_sha256),@($value.core,$value.core_sha256))) { Check-File $pair[0] $pair[1] }
    }
    else {
        foreach ($path in @($PSCommandPath,$native)) {
            $selected=@($value.files | Where-Object {$_.path -ieq $path})
            if ($selected.Count -ne 1) { throw 'Pinned restoration source required' }
            Check-File $path $selected[0].sha256
        }
    }
}
Add-Type -Path $native
if ($Role -ne 'Controller') {
    if ($Role -eq 'Watchdog') {
        $owner=$null; $reason='deadline'; $status=[ordered]@{schema=1;role='Watchdog';restored=$false;hardware_admission=$false}
        try {
            $owner=[Diagnostics.Process]::GetProcessById([int]$value.owner_pid)
            $ownerHandle=$owner.Handle
            if ($owner.StartTime.ToUniversalTime().Ticks -ne [long]$value.owner_start_ticks) { throw 'Controller PID was reused before watchdog armed' }
            Write-Json (Join-Path $value.control 'watchdog-ready.json') ([ordered]@{pid=$PID;start_utc=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToString('o');owner_pid=$owner.Id;owner_handle=$ownerHandle.ToInt64();owner_start_ticks=$value.owner_start_ticks;deadline_utc=$value.deadline_utc})
            while ([DateTime]::UtcNow -lt [DateTime]::Parse($value.deadline_utc).ToUniversalTime()) {
                if (Test-Path -LiteralPath (Join-Path $value.control 'restored.json')) { $reason='controller-restored'; break }
                if ($owner.WaitForExit(200)) { $reason='owner-loss'; break }
            }
        } catch { $reason='watchdog-owner-error'; $status.failure=$_.Exception.ToString() }
        finally {
            $status.reason=$reason
            try {
                $settled=Restore-Tuple $value $reason
                $status.settled=$true; $status.restored=$settled.raw_tuple_restored; $status.cancelled_without_registry_write=$settled.cancelled_without_registry_write
                # Owner-loss cleanup is delayed until restoration/release is
                # durable. The normal worker retains its original child handle.
                if ($reason -ne 'controller-restored') {
                    $clock=[Diagnostics.Stopwatch]::StartNew()
                    while (!(Test-Path -LiteralPath (Join-Path $value.output 'worker-result.json')) -and $clock.ElapsedMilliseconds -lt 25000) { Start-Sleep -Milliseconds 200 }
                    try { if (!(Test-Path -LiteralPath (Join-Path $value.output 'worker-result.json'))) { Close-HeldProbeIfWorkerLost $value } }
                    finally { Remove-OwnedTask $value }
                }
            } catch { $status.restoration_failure=$_.Exception.ToString() }
            if ($owner) { $owner.Dispose() }
            Write-Json (Join-Path $value.control 'watchdog-result.json') $status
            if ($reason -ne 'controller-restored' -and $status.Contains('settled') -and $status.settled -and !$status.Contains('restoration_failure')) {
                $ownTask=Get-ScheduledTask -TaskName $value.restore_task_name -ErrorAction SilentlyContinue
                if ($ownTask) { Unregister-ScheduledTask -TaskName $value.restore_task_name -Confirm:$false }
            }
        }
        if (!$status.Contains('settled') -or !$status.settled -or $status.Contains('restoration_failure')) { exit 1 }; exit 0
    }

    # Interactive Limited USER, matching the actual existing runtime helpers.
    Add-Type -Path (Join-Path $PSScriptRoot 'system-runtime-approved-payload.cs') -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll')
    $status=[ordered]@{schema=1;role='Worker';passed=$false;held_module_census_passed=$false;registry_mutations=0;hardware_admission=$false}
    $event=$null
    try {
        $token=& $value.token_script
        if ($token.sid -cne $value.desktop_sid -or $token.session_id -ne $value.desktop_session -or $token.elevated -or $token.elevation_type -ne 3 -or $token.integrity_rid -ne 8192 -or $env:PROCESSOR_ARCHITECTURE -cne 'ARM64') { throw 'Expected native Limited USER in the existing desktop session' }
        $status.token=$token
        $status.worker_job_handle=[DxvkBindingNative01]::OwnWorkerLifetime()
        # The separately pinned progress variant of the retained-process runner owns Start/Wait/Exit
        # and raw pipes. A tiny pure C# async wrapper allows Local-event release.
        $async=@'
public static class DxvkBindingAsync01 {
    public static System.Threading.Tasks.Task<DxvkRawProcessResultF4_02> Start(string exe,string args,string cwd,string stdout,string stderr,int deadline) {
        return System.Threading.Tasks.Task.Factory.StartNew(delegate { return DxvkRawProcessF4_02.Run(exe,args,cwd,stdout,stderr,deadline); });
    }
}
'@
        Add-Type -TypeDefinition ((Get-Content -LiteralPath $value.runner -Raw -Encoding UTF8)+"`n"+$async)
        if ($value.api -in @('10','11')) {
            $negativeOut=Join-Path $value.output 'entry-negative.stdout.raw'; $negativeErr=Join-Path $value.output 'entry-negative.stderr.raw'
            $negative=[DxvkRawProcessF4_02]::Run($value.probe,('--entry-negative '+(Quote $value.front)),$value.output,$negativeOut,$negativeErr,20000)
            Write-Json (Join-Path $value.output 'entry-negative.process.json') (Process-Receipt $negative $value.runner_sha256)
            $negativeText=[DxvkBindingNative01]::ReadSharedText($negativeOut)
            $negativeMarker='SYSTEM_RUNTIME_VALIDATION_ENTRY_NEGATIVE_PASS checks=5 core_loaded=0 registry_changes=0 gpu_calls=0'
            if ($value.api -ceq '11') { $negativeMarker='SYSTEM_D3D11_VALIDATION_NEGATIVE_PASS checks=9 core_loaded=0 registry_changes=0 gpu_calls=0' }
            if (!$negative.Exited -or !$negative.ExitCodeAvailable -or $negative.ExitCode -ne 0 -or $negative.TimedOut -or $negative.ChildStillRunning -or !$negative.PipesDrained -or $negative.Failure -or $negative.StderrBytes -ne 0 -or $negativeText -cnotmatch ('(?m)^'+[regex]::Escape($negativeMarker)+'\r?$')) { throw 'Actual native typed entry controls failed before binding' }
            $status.native_entry_negative_passed=$true
        } else { $status.native_entry_negative_executed=$false; $status.native_entry_negative_contract='Frozen739de05 ordinary D9 probe has no typed entry-negative mode' }
        $before=[DxvkBindingNative01]::Names($value.luid); Check-Names $before $value.luid
        for ($index=0;$index -lt 3;++$index) { if ($before.Names[$index].Name -ine $value.original_slots[$index]) { throw 'Actual original KMT names differ from the three-slot tuple' } }
        Write-Json (Join-Path $value.output 'kmt-before.json') $before
        Write-Json (Join-Path $value.output 'worker-ready.json') ([ordered]@{pid=$PID;sid=$token.sid;session=$token.session_id;worker_job_handle=$status.worker_job_handle})
        $clock=[Diagnostics.Stopwatch]::StartNew()
        while (!(Test-Path -LiteralPath (Join-Path $value.control 'bound.json'))) {
            if ((Test-Path -LiteralPath (Join-Path $value.control 'release.json')) -or $clock.ElapsedMilliseconds -gt 60000) { throw 'Binding was cancelled or never armed' }
            Start-Sleep -Milliseconds 100
        }
        $bound=[DxvkBindingNative01]::Names($value.luid); Check-Names $bound $value.luid
        Write-Json (Join-Path $value.output 'kmt-bound.json') $bound
        $status.effective_candidate_selected=$bound.Names[$value.native_slot].Name -ieq $value.front
        # Existing proven runtime-lifecycle payload contract, confined to this
        # worker and its owned probe; no desktop/system environment changes.
        $env:VK_DRIVER_FILES=$value.vulkan_icd; $env:VK_ICD_FILENAMES=$value.vulkan_icd
        $env:VK_LOADER_DEBUG='error,warn,driver'; $env:DXVK_LOG_PATH=$value.output; $env:DXVK_LOG_LEVEL='info'; $env:TU_WDDM_DIAGNOSTICS='1'
        $status.vulkan_environment=[ordered]@{VK_DRIVER_FILES=$value.vulkan_icd;VK_ICD_FILENAMES=$value.vulkan_icd;TU_WDDM_DIAGNOSTICS='1';scope='worker/owned child only'}
        $stdout=Join-Path $value.output 'probe.stdout.raw'; $stderr=Join-Path $value.output 'probe.stderr.raw'
        $raw=Join-Path $value.output 'originals'
        if ($value.api -ceq '10') {
            $args='10 '+$value.luid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
        } elseif ($value.api -ceq '11') {
            $args=$value.luid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $value.private_loader)+' '+(Quote $value.vulkan_library)+' '+(Quote $value.vulkan_icd)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
        } else {
            # Frozen739de05 argv7 is the loaded ICD DLL, not its JSON manifest.
            $args=$value.api+' '+$value.d9_phase+' '+$value.luid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $value.private_loader)+' '+(Quote $value.vulkan_library)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
        }
        Write-Json (Join-Path $value.output 'probe-start.json') ([ordered]@{executable=$value.probe;arguments=$args;runner_sha256=$value.runner_sha256;utc=[DateTime]::UtcNow.ToString('o')})
        $task=[DxvkBindingAsync01]::Start($value.probe,$args,$value.output,$stdout,$stderr,115000)
        $published=$false; $publishedPid=0; $publishedStart=''; $publishedHandle=0L; $released=$false
        while (!$task.IsCompleted) {
            $heldPid=0; $pendingExit=0
            if ($value.api -ceq '10') {
                $text=[DxvkBindingNative01]::ReadSharedText($stdout)
                $match=[regex]::Match($text,'(?m)^SYSTEM_RUNTIME_VALIDATION_HELD pid=(\d+) timeout_ms=60000 pending_exit=([01]) registry_restoration_not_proved_by_event=1\r?$')
                if ($match.Success) { $heldPid=[int]$match.Groups[1].Value; $pendingExit=[int]$match.Groups[2].Value }
            } elseif (!$published -and (Test-Path -LiteralPath ($raw+'.held.json'))) {
                $closed=[DxvkBindingNative01]::ReadClosedText($raw+'.held.json')
                if ($null -ne $closed) {
                    $held=$closed | ConvertFrom-Json
                    if ($value.api -ceq '11') {
                        $fields=@($held.PSObject.Properties.Name | Sort-Object) -join ','
                        if ($fields -cne 'api,event,factoryCalled,factoryResult,output,pid,pixelsPassed,result,schema,stage,timeout_ms' -or $held.schema -ne 1 -or $held.api -ne 11 -or $held.pid -le 0 -or $held.pid -gt [int]::MaxValue -or $held.timeout_ms -ne 60000 -or $held.event -cne $value.hold_event -or $held.output -ine $raw -or $held.factoryCalled -isnot [bool] -or $held.pixelsPassed -isnot [bool] -or $held.stage -isnot [string]) { throw 'Exact closed dedicated D11 hold checkpoint required' }
                        foreach ($number in @($held.schema,$held.api,$held.pid,$held.timeout_ms,$held.factoryResult,$held.result)) {
                            if (($number -isnot [int] -and $number -isnot [long]) -or $number -lt [int]::MinValue -or $number -gt [int]::MaxValue) { throw 'Actual signed D11 checkpoint integers required' }
                        }
                        $heldPid=[int]$held.pid; $pendingExit=1
                        if ($held.factoryCalled -and $held.factoryResult -eq 0 -and $held.pixelsPassed -and $held.result -eq 0 -and $held.stage -ceq '') { $pendingExit=0 }
                        $status.d11_closed_checkpoint=$held
                    } else {
                        if ($held.schema -cne 'ordinary-system-d3d9-held-v1' -or $held.pid -le 0 -or $held.timeout_ms -ne 60000 -or $held.pending_exit -notin @(0,1) -or $held.hold_event -cne $value.hold_event -or $held.restoration_proved_by_event -ne $false) { throw 'Exact closed ordinary D9 hold checkpoint required' }
                        $heldPid=[int]$held.pid; $pendingExit=[int]$held.pending_exit
                    }
                }
            }
            if ($heldPid -gt 0 -and !$published) {
                $child=[Diagnostics.Process]::GetProcessById($heldPid)
                try {
                    $handle=$child.Handle
                    $publishedStart=$child.StartTime.ToUniversalTime().ToString('o'); $publishedHandle=$handle.ToInt64()
                    if ($child.MainModule.FileName -ine $value.probe) { throw 'Actual held probe module differs' }
                    # Even failure-held producers must reach restore/release.
                    # A census failure denies success, but does not skip that
                    # bounded restoration protocol or lose the original PID.
                    try {
                        $census=Held-ModuleCensus $child $value
                        if ($census.start_utc -cne $publishedStart -or $census.pid -ne $child.Id -or $census.retained_handle -le 0) { throw 'Held census identity differs from the retained published process' }
                        Write-Json (Join-Path $value.output 'held-module-census.json') $census
                        $status.held_module_census_passed=$true
                    } catch {
                        $pendingExit=1; $status.held_module_census_passed=$false; $status.held_module_census_failure=$_.Exception.ToString()
                        Write-Json (Join-Path $value.output 'held-module-census.json') ([ordered]@{schema=1;pid=$child.Id;start_utc=$child.StartTime.ToUniversalTime().ToString('o');retained_handle=$handle.ToInt64();passed=$false;failure=$status.held_module_census_failure;hardware_admission=$false})
                    }
                    Write-Json (Join-Path $value.output 'worker-held.json') ([ordered]@{pid=$child.Id;start_utc=$child.StartTime.ToUniversalTime().ToString('o');retained_handle=$handle.ToInt64();pending_exit=$pendingExit;stdout_bytes=(Get-Item -LiteralPath $stdout).Length;api=$value.api;native_slot=$value.native_slot})
                } finally { $child.Dispose() }
                $publishedPid=$heldPid; $published=$true
            }
            $release=Join-Path $value.control 'release.json'
            if ($published -and !$released -and (Test-Path -LiteralPath $release)) {
                $proof=Join-Path $value.control 'restored.json'
                $permission=Get-Content -LiteralPath $release -Raw -Encoding UTF8 | ConvertFrom-Json
                Check-File $proof $permission.restored_sha256
                if ($permission.raw_tuple_restored -ne $true -or $permission.cancelled_without_registry_write -ne $false) { throw 'Held factory cannot be released by a no-mutation cancellation checkpoint' }
                $after=[DxvkBindingNative01]::Names($value.luid); Check-Names $after $value.luid
                Write-Json (Join-Path $value.output 'kmt-after-restoration-before-release.json') $after
                $status.effective_original_names_restored=Same-Names $before $after
                $event=[Threading.EventWaitHandle]::OpenExisting($value.hold_event)
                if (!$event.Set()) { throw 'Cannot release actual session-local held probe' }
                $released=$true
            }
            Start-Sleep -Milliseconds 100
        }
        $row=$task.GetAwaiter().GetResult()
        $receipt=Process-Receipt $row $value.runner_sha256
        Write-Json (Join-Path $value.output 'probe.process.json') $receipt
        $status.probe=$receipt; $status.held=$published; $status.released_after_raw_restore=$released
        if ($status.held_module_census_passed) {
            [DxvkApprovedPayloadPolicy01]::RequireProcessJoin($publishedPid,$publishedStart,$publishedHandle,$row.Pid,$row.StartUtc,$row.ProcessHandle)
            $status.held_identity_joined_to_original_runner=$true
        }
        if (!$published -or !$released -or !$status.held_module_census_passed -or $row.Pid -ne $publishedPid -or !$row.Exited -or !$row.ExitCodeAvailable -or $row.ExitCode -ne 0 -or $row.TimedOut -or $row.ChildStillRunning -or !$row.PipesDrained -or $row.Failure -or !$status.effective_candidate_selected -or !$status.effective_original_names_restored) { throw 'Actual factory/selection/held/modules/restore process gate failed; raw failure retained' }
        if ($value.api -ceq '11') {
            $finished=[DxvkBindingNative01]::ReadSharedText($stdout)
            if ($finished -cnotmatch '(?m)^SYSTEM_D3D11_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=0 software_fallback=0 production_admission=0 registry_changes=0\r?$' -or $finished -cnotmatch ('(?m)^SYSTEM_D3D11_HELD pid='+$publishedPid+' timeout_ms=60000 pixels_passed=1 stage= hr=00000000\r?$') -or $row.StderrBytes -ne 0) { throw 'Actual dedicated D11 factory/readback/release markers failed; independent raw reader remains required' }
        } elseif ($value.api -in @('9','9ex')) {
            $finished=[DxvkBindingNative01]::ReadSharedText($stdout)
            $presents=0; $screenPixels=0; if ($value.d9_phase -ceq 'present') { $presents=2; $screenPixels=512 }
            $ready='D9_SYSTEM_RESULTS_READY api='+$value.api+' phase='+$value.d9_phase+' pixels=512 presents='+$presents+' screen_pixels='+$screenPixels+' production_admission=0'
            if ($finished -cnotmatch ('(?m)^'+[regex]::Escape($ready)+'\r?$') -or $finished -cnotmatch '(?m)^D9_SYSTEM_DEVICE_RELEASE remaining=0\r?$' -or $finished -cnotmatch '(?m)^D9_SYSTEM_DONE exit=0 production_admission=0 registry_changes=0\r?$' -or $row.StderrBytes -ne 0) { throw 'Actual ordinary D9 render/release markers failed; independent raw reader remains required' }
        }
        $status.passed=$true
    } catch { $status.failure=$_.Exception.ToString() }
    finally {
        if ($event) { $event.Dispose() }
        Write-Json (Join-Path $value.output 'worker-result.json') $status
    }
    if (!$status.passed) { exit 1 }; exit 0
}

# Controller defaults to exact readonly snapshot/query. Apply is a separate,
# explicit reviewed experiment after actual ARM64X/module input acceptance.
if ($env:PROCESSOR_ARCHITECTURE -cne 'ARM64' -or $Luid -cnotmatch '^[0-9a-f]{8}:[0-9a-f]{8}$') { throw 'Native ARM64 controller and explicit nonzero LUID required' }
[DxvkBindingNative01]::RequireAbsolute($RunRoot)
if (!(Test-Path -LiteralPath $RunRoot)) { New-Item -ItemType Directory -Path $RunRoot | Out-Null } else { throw 'Fresh run directory required' }
$RunRoot=[IO.Path]::GetFullPath($RunRoot)
$device=Get-PnpDevice -PresentOnly -InstanceId $InstanceId
if ($device.Status -cne 'OK' -or $device.Class -cne 'Display' -or $InstanceId -inotmatch '^PCI\\VEN_1AF4&DEV_1050') { throw 'Expected exact present VIOGPU display devnode' }
$driver=[string](Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName 'DEVPKEY_Device_Driver').Data
if ($driver -cnotmatch '^\{4d36e968-e325-11ce-bfc1-08002be10318\}\\[0-9]{4}$') { throw 'Exact display-adapter class binding required' }
$subkey='SYSTEM\CurrentControlSet\Control\Class\'+$driver
$nativeSlot=1; if ($Api -ceq '11') { $nativeSlot=2 } elseif ($Api -cne '10') { $nativeSlot=0 }
if (!$ApplyReviewedTuple) {
    $readonly=[DxvkBindingNative01]::Snapshot($subkey)
    $readonlyNames=[DxvkBindingNative01]::Names($Luid); Check-Names $readonlyNames $Luid
    Write-Json (Join-Path $RunRoot 'readonly-original.json') ([ordered]@{schema=1;instance=$InstanceId;driver=$driver;luid=$Luid;api=$Api;native_slot=$nativeSlot;registry_subkey=$subkey;original=$readonly;names=$readonlyNames;hardware_admission=$false;mutation=$false})
    exit 0
}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if (!([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Explicit tuple experiment requires administrator controller' }
$lease=[Threading.Mutex]::new($false,'Global\VioGpuD10ValidationAdapterLease01'); $leaseHeld=$false
try {
try { $leaseHeld=$lease.WaitOne(0) } catch [Threading.AbandonedMutexException] { $leaseHeld=$true }
if (!$leaseHeld) { throw 'Another adapter validation controller owns the global lease' }
if (@(Get-ScheduledTask | Where-Object {$_.TaskName -like 'VioGpu-D10-System-Validation-*-restore'}).Count -ne 0) { throw 'A previous restoration task must finish or be independently reviewed first' }
# This authoritative Apply backup is captured only under the exclusive lease,
# after any prior rescue finished. Readonly observations never become a backup.
$original=[DxvkBindingNative01]::Snapshot($subkey)
$slots=[DxvkBindingNative01]::Tuple($original[0])
$names=[DxvkBindingNative01]::Names($Luid); Check-Names $names $Luid
Write-Json (Join-Path $RunRoot 'apply-original.json') ([ordered]@{schema=1;instance=$InstanceId;driver=$driver;luid=$Luid;api=$Api;native_slot=$nativeSlot;registry_subkey=$subkey;original=$original;names=$names;exclusive_adapter_lease_held=$true;hardware_admission=$false;mutation=$false})
foreach ($pair in @(@($Probe,$ProbeSha256),@($Front,$FrontSha256),@($Core,$CoreSha256),@($TokenScript,$TokenScriptSha256))) { Check-File $pair[0] $pair[1]; [DxvkBindingNative01]::RequireAbsolute($pair[0]) }
$driverState=Driver-State $InstanceId
Check-File $DriverSys $DriverSysSha256
if ($driverState.image -ine $DriverSys -or $driverState.sha256 -cne $DriverSysSha256) { throw 'Expected unchanged installed KMD image/service bytes required' }
Write-Json (Join-Path $RunRoot 'driver-state-before.json') $driverState
$runnerSha='7def540f912623e6e4a4925bf3e747e0cf617327367d659bc2cd8e41a9c69513'
# D11's frozen independent reader requires the original retained runner. Its
# closed held JSON provides readiness without reading buffered short stdout.
if ($Api -ceq '11') { $runnerSha='d8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad' }
Check-File $Runner $runnerSha
if ($Api -ceq '10') {
    if ((Split-Path $Core -Leaf) -cne 'viogpudxvk.dll' -or (Split-Path $Core -Parent) -ine (Split-Path $Front -Parent)) { throw 'D10 requires its exact sibling native core' }
} elseif ($Api -ceq '11') {
    $d11Architecture=[DxvkBindingNative01]::ProbeArchitecture([IO.File]::ReadAllBytes($Probe))
    $d11CoreName='viogpudxvk.dll'; if ($d11Architecture -ceq 'x64') { $d11CoreName='viogpudxvk_x64.dll' }
    $expectedCore=Join-Path (Split-Path $Front -Parent) $d11CoreName
    if ($Core -ine $expectedCore) { throw 'D11 validation frontend requires its architecture-matched sibling core' }
} else {
    if ((Split-Path $Core -Leaf) -cne 'viogpudxvk.dll') { throw 'Exact reviewed D9 core basename required' }
    $d9Architecture=[DxvkBindingNative01]::ProbeArchitecture([IO.File]::ReadAllBytes($Probe))
    $expectedCore=Join-Path (Join-Path (Split-Path $Front -Parent) $d9Architecture) 'viogpudxvk.dll'
    if ($Core -ine $expectedCore) { throw 'Production D9 frontend requires architecture-matched arm64/x64 child core path' }
}
$payloadSource=Join-Path $PSScriptRoot 'system-runtime-approved-payload.cs'
Add-Type -Path $payloadSource -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll')
$selection=[DxvkApprovedSelection01]::new()
$selection.Api=$Api; $selection.Architecture=[DxvkBindingNative01]::ProbeArchitecture([IO.File]::ReadAllBytes($Probe))
$selection.Core=$Core; $selection.CoreSha256=$CoreSha256; $selection.PrivateLoader=$PrivateLoader; $selection.PrivateLoaderSha256=$PrivateLoaderSha256
$selection.IcdJson=$VulkanIcd; $selection.IcdJsonSha256=$VulkanIcdSha256; $selection.IcdLibrarySha256=$VulkanLibrarySha256; $selection.VulkanLoaderSha256=$VulkanLoaderSha256
$approved=[DxvkApprovedPayloadPolicy01]::ValidateFiles($ApprovedPayload,$ApprovedPayloadSha256,$selection)
$vulkanLibrary=$approved.IcdLibrary; $vulkanLoader=$approved.PrivateLoader
Write-Json (Join-Path $RunRoot 'approved-payload-verified.json') ([ordered]@{schema=1;approved_payload=$ApprovedPayload;approved_payload_sha256=$ApprovedPayloadSha256;source_commit=$approved.SourceCommit;ci_run=$approved.CiRun;arch=$approved.Architecture;files=$approved.Files;metadata_only=$true;native_module_calls=0;hardware_admission=$false})
$explorer=@(Get-Process explorer -IncludeUserName); $dwm=@(Get-Process dwm)
if ($explorer.Count -ne 1 -or $dwm.Count -ne 1 -or !$explorer[0].UserName -or $explorer[0].SessionId -le 0 -or $dwm[0].SessionId -ne $explorer[0].SessionId) { throw 'Exactly one existing interactive desktop required' }
$account=$explorer[0].UserName; $sid=([Security.Principal.NTAccount]$account).Translate([Security.Principal.SecurityIdentifier])
$acl=[Security.AccessControl.DirectorySecurity]::new(); $acl.SetAccessRuleProtection($true,$false)
foreach ($ownerSid in @('S-1-5-18','S-1-5-32-544')) { $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($ownerSid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')) }
$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow')); Set-Acl -LiteralPath $RunRoot -AclObject $acl
$control=Join-Path $RunRoot 'controller'; $output=Join-Path $RunRoot 'interactive'
New-Item -ItemType Directory -Path $control,$output | Out-Null
$outputAcl=Get-Acl -LiteralPath $output; $outputAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'Modify','ContainerInherit,ObjectInherit','None','Allow')); Set-Acl -LiteralPath $output -AclObject $outputAcl
$runId=[Guid]::NewGuid().ToString('N'); $taskName='VioGpu-D10-System-Validation-'+$runId
$bundle=Join-Path $control 'bundle'; New-Item -ItemType Directory -Path $bundle | Out-Null
$controllerScript=Join-Path $bundle 'test-system-d3d10-binding.ps1'; $nativeCopy=Join-Path $bundle 'system-d3d10-binding-native.cs'; $payloadCopy=Join-Path $bundle 'system-runtime-approved-payload.cs'
$runnerCopyName='owned-raw-process-f4bf37f-progress-01.cs'; if ($Api -ceq '11') { $runnerCopyName='owned-raw-process-f4bf37f-02.cs' }
$runnerCopy=Join-Path $bundle $runnerCopyName; $tokenCopy=Join-Path $bundle 'inspect-process-token.ps1'
foreach ($copy in @(@($PSCommandPath,$controllerScript),@($native,$nativeCopy),@($payloadSource,$payloadCopy),@($Runner,$runnerCopy),@($TokenScript,$tokenCopy))) {
    $beforeHash=Hash $copy[0]; Copy-Item -LiteralPath $copy[0] -Destination $copy[1]; Check-File $copy[1] $beforeHash
}
$self=(Get-Process -Id $PID)
$value=[ordered]@{schema=2;api=$Api;native_slot=$nativeSlot;d9_phase=$D9Phase;private_loader=$vulkanLoader;vulkan_library=$vulkanLibrary;owner_pid=$PID;owner_start_ticks=$self.StartTime.ToUniversalTime().Ticks;deadline_utc=[DateTime]::UtcNow.AddSeconds(120).ToString('o');registry_subkey=$subkey;original=$original;original_slots=$slots;luid=$Luid;control=$control;output=$output;mutex=('Global\VioGpuD10Binding-'+$runId);task_name=$taskName;restore_task_name=($taskName+'-restore');desktop_sid=$sid.Value;desktop_session=$explorer[0].SessionId;desktop_dwm_pid=$dwm[0].Id;desktop_explorer_pid=$explorer[0].Id;probe=$Probe;probe_sha256=$ProbeSha256;front=$Front;front_sha256=$FrontSha256;core=$Core;core_sha256=$CoreSha256;runner=$runnerCopy;runner_sha256=$runnerSha;token_script=$tokenCopy;vulkan_icd=$VulkanIcd;hold_event=('Local\VioGpuD10Validation-'+$runId)}
$value.files=@($controllerScript,$nativeCopy,$runnerCopy,$tokenCopy,$Probe,$Front,$Core,$DriverSys,$VulkanIcd,$vulkanLibrary,$vulkanLoader | ForEach-Object { [ordered]@{path=$_;sha256=(Hash $_)} })
$value.payload_source=$approved.SourceCommit; $value.payload_ci_run=$approved.CiRun; $value.approved_payload_sha256=$ApprovedPayloadSha256
$value.module_files=@([ordered]@{Path=$Front;Bytes=(Get-Item -LiteralPath $Front).Length;Sha256=$FrontSha256})+@($approved.ModuleFiles)
$value.files+=@([ordered]@{path=$payloadCopy;sha256=(Hash $payloadCopy)},[ordered]@{path=$ApprovedPayload;sha256=$ApprovedPayloadSha256})
$value.files+=@($approved.Files | ForEach-Object { [ordered]@{path=$_.Path;sha256=$_.Sha256} })
$configFile=Join-Path $control 'config.json'; Write-Json $configFile $value; $configHash=Hash $configFile
$sourceProfile='unregistered-validation8eeb20'; if ($Api -ceq '11') { $sourceProfile='dedicated-SYSTEM-D11-FL10_0-validation' } elseif ($Api -cne '10') { $sourceProfile='ordinary-D9-probe739de05' }
$watchdog=$null; $registered=$false; $restoreRegistered=$false; $status=[ordered]@{schema=1;source=$sourceProfile;api=$Api;native_slot=$nativeSlot;registry_restored=$false;hardware_admission=$false;production_admission=$false;default_replacement=$false}
try {
    $power=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    # A service-owned task avoids inheriting an SSH session's process job.
    # Its retained controller handle and bounded deadline survive owner loss.
    $restorePrincipal=New-ScheduledTaskPrincipal -UserId 'S-1-5-18' -LogonType ServiceAccount -RunLevel Highest
    $restoreArgs='-NoProfile -ExecutionPolicy Bypass -File '+(Quote $controllerScript)+' -Role Watchdog -Config '+(Quote $configFile)+' -ConfigSha256 '+$configHash
    $restoreAction=New-ScheduledTaskAction -Execute $power -Argument $restoreArgs
    $restoreSettings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Seconds 240)
    $restoreTrigger=New-ScheduledTaskTrigger -AtStartup
    Register-ScheduledTask -TaskName $value.restore_task_name -Action $restoreAction -Trigger $restoreTrigger -Principal $restorePrincipal -Settings $restoreSettings | Out-Null; $restoreRegistered=$true
    Start-ScheduledTask -TaskName $value.restore_task_name
    $clock=[Diagnostics.Stopwatch]::StartNew()
    while (!(Test-Path -LiteralPath (Join-Path $control 'watchdog-ready.json'))) { if ($clock.ElapsedMilliseconds -gt 15000) { throw 'Independent watchdog never armed' }; Start-Sleep -Milliseconds 100 }
    $armed=Get-Content -LiteralPath (Join-Path $control 'watchdog-ready.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $watchdog=[Diagnostics.Process]::GetProcessById([int]$armed.pid)
    $watchHandle=$watchdog.Handle
    if ($watchdog.StartTime.ToUniversalTime().ToString('o') -cne $armed.start_utc -or $watchdog.MainModule.FileName -ine $power) { throw 'Original service-owned watchdog process differs' }
    $status.watchdog_pid=$watchdog.Id; $status.watchdog_handle=$watchHandle.ToInt64(); $status.watchdog_start_utc=$armed.start_utc
    $principal=New-ScheduledTaskPrincipal -UserId $account -LogonType Interactive -RunLevel Limited
    $arguments='-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File '+(Quote $controllerScript)+' -Role Worker -Config '+(Quote $configFile)+' -ConfigSha256 '+$configHash
    $action=New-ScheduledTaskAction -Execute $power -Argument $arguments
    $settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Seconds 180)
    Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings | Out-Null; $registered=$true
    Start-ScheduledTask -TaskName $taskName
    $clock.Restart()
    while (!(Test-Path -LiteralPath (Join-Path $output 'worker-ready.json'))) { if ((Test-Path -LiteralPath (Join-Path $output 'worker-result.json')) -or $clock.ElapsedMilliseconds -gt 20000) { throw 'Interactive original-name/token gate failed' }; Start-Sleep -Milliseconds 100 }
    $mutex=[Threading.Mutex]::new($false,$value.mutex); $locked=$false
    try {
        try { $locked=$mutex.WaitOne(5000) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (!$locked -or (Test-Path -LiteralPath (Join-Path $control 'restored.json')) -or [DateTime]::UtcNow -ge [DateTime]::Parse($value.deadline_utc).ToUniversalTime()) { throw 'Binding cannot start after watchdog restoration/deadline' }
        [DxvkBindingNative01]::RequireSnapshot($original,[DxvkBindingNative01]::Snapshot($subkey))
        $replacement=[DxvkBindingNative01]::ReplaceNativeSlot($original[0],$Front,$nativeSlot)
        # Same restore mutex covers recheck, durable intent and the first write.
        # Owner loss after intent but before/during RegSet conservatively restores.
        Write-MutationIntent (Join-Path $control 'mutation-intent.json') ([ordered]@{schema=2;owner_pid=$PID;registry_subkey=$subkey;selected_native_slot=$nativeSlot;only_native_tuple_slot=$true;utc=[DateTime]::UtcNow.ToString('o')})
        [DxvkBindingNative01]::Write($subkey,$replacement)
        if (![DxvkBindingNative01]::Same($replacement,[DxvkBindingNative01]::Read($subkey,0x100,'UserModeDriverName'))) { throw 'Exact selected native-slot write did not read back' }
        # WoW and installed-driver values are never edited. SYSTEM views can alias.
        foreach ($index in @(1,2,4,5)) { if (![DxvkBindingNative01]::Same($original[$index],([DxvkBindingNative01]::Snapshot($subkey))[$index])) { throw 'Untouched WoW/installed tuple changed' } }
        Write-Json (Join-Path $control 'candidate-tuple-written.json') ([ordered]@{utc=[DateTime]::UtcNow.ToString('o');actual=[DxvkBindingNative01]::Snapshot($subkey);luid=$Luid;instance=$InstanceId;external_refresh_wait=$WaitForReviewedRefresh.IsPresent})
        $status.registry_changed=$true
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
    if ($WaitForReviewedRefresh) {
        # ROOT may independently perform a reviewed exact-devnode restart and
        # write this protected checkpoint. This script never runs a reset,
        # pnputil, a reboot or VM/image operation. The rescue task stays armed.
        $refresh=Join-Path $control 'refresh-complete.json'; $clock.Restart()
        while (!(Test-Path -LiteralPath $refresh)) {
            if ((Test-Path -LiteralPath (Join-Path $control 'restored.json')) -or $clock.ElapsedMilliseconds -gt 30000) { throw 'Reviewed refresh checkpoint absent/cancelled; restoring tuple' }
            Start-Sleep -Milliseconds 100
        }
        $checkpoint=Get-Content -LiteralPath $refresh -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($checkpoint.schema -ne 1 -or $checkpoint.instance -cne $InstanceId -or $checkpoint.luid -cne $Luid -or $checkpoint.reboot_requested -ne $false -or $checkpoint.actual_exit_code -ne 0) { throw 'Exact reviewed refresh/unchanged-LUID checkpoint required' }
        foreach ($file in $checkpoint.actual_originals) { Check-File $file.path $file.sha256 }
        if (@($checkpoint.actual_originals).Count -lt 2) { throw 'Actual owned refresh process and raw originals required' }
        $status.external_refresh_checkpoint=$checkpoint
    }
    $mutex=[Threading.Mutex]::new($false,$value.mutex); $locked=$false
    try {
        try { $locked=$mutex.WaitOne(5000) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (!$locked -or (Test-Path -LiteralPath (Join-Path $control 'restored.json')) -or [DateTime]::UtcNow -ge [DateTime]::Parse($value.deadline_utc).ToUniversalTime()) { throw 'Probe cannot start after rescue restoration' }
        if (![DxvkBindingNative01]::Same($replacement,[DxvkBindingNative01]::Read($subkey,0x100,'UserModeDriverName'))) { throw 'Candidate tuple changed during refresh checkpoint' }
        Write-Json (Join-Path $control 'bound.json') ([ordered]@{utc=[DateTime]::UtcNow.ToString('o');actual=[DxvkBindingNative01]::Snapshot($subkey);api=$Api;selected_native_slot=$nativeSlot;only_native_tuple_slot_edited=$true;adapter_wide=$true})
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
    $clock.Restart()
    while (!(Test-Path -LiteralPath (Join-Path $output 'worker-held.json'))) { if ((Test-Path -LiteralPath (Join-Path $output 'worker-result.json')) -or $clock.ElapsedMilliseconds -gt 80000) { throw 'Actual probe did not reach bounded hold' }; Start-Sleep -Milliseconds 100 }
    $settled=Restore-Tuple $value 'held-probe'; $status.registry_restored=$settled.raw_tuple_restored
    if (!$status.registry_restored) { throw 'Held probe requires actual intent-backed raw restoration' }
    $clock.Restart()
    while (!(Test-Path -LiteralPath (Join-Path $output 'worker-result.json'))) { if ($clock.ElapsedMilliseconds -gt 30000) { throw 'Interactive runner did not reap after restoration' }; Start-Sleep -Milliseconds 100 }
    $worker=Get-Content -LiteralPath (Join-Path $output 'worker-result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $status.worker=$worker
    if (!$worker.passed) { throw 'Actual SYSTEM factory or cached-name gate failed; exact originals retained' }
    $status.slice_passed=$true
} catch { $status.failure=$_.Exception.ToString() }
finally {
    try {
        $settled=Restore-Tuple $value 'controller-finally'
        $status.registry_settled=$true; $status.registry_restored=$settled.raw_tuple_restored; $status.cancelled_without_registry_write=$settled.cancelled_without_registry_write
        $clock=[Diagnostics.Stopwatch]::StartNew()
        while ($registered -and !(Test-Path -LiteralPath (Join-Path $output 'worker-result.json')) -and $clock.ElapsedMilliseconds -lt 25000) { Start-Sleep -Milliseconds 100 }
        try { if ($registered -and !(Test-Path -LiteralPath (Join-Path $output 'worker-result.json'))) { Close-HeldProbeIfWorkerLost $value } }
        finally { if ($registered) { Remove-OwnedTask $value } }
        $status.final_names=[DxvkBindingNative01]::Names($Luid); Check-Names $status.final_names $Luid
        $status.effective_original_names_restored=Same-Names $names $status.final_names
        $afterDwm=@(Get-Process dwm); $afterExplorer=@(Get-Process explorer)
        $status.desktop_retained=$afterDwm.Count -eq 1 -and $afterExplorer.Count -eq 1 -and $afterDwm[0].Id -eq $value.desktop_dwm_pid -and $afterExplorer[0].Id -eq $value.desktop_explorer_pid
        $status.devnode_retained=(Get-PnpDevice -PresentOnly -InstanceId $InstanceId).Status -ceq 'OK' -and [string](Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName 'DEVPKEY_Device_Driver').Data -ceq $driver
        $driverAfter=Driver-State $InstanceId; Write-Json (Join-Path $control 'driver-state-after.json') $driverAfter
        $status.kmd_package_retained=($driverState | ConvertTo-Json -Compress) -ceq ($driverAfter | ConvertTo-Json -Compress)
        if (!$status.effective_original_names_restored -or !$status.desktop_retained -or !$status.devnode_retained -or !$status.kmd_package_retained) { throw 'Effective name/desktop/devnode/KMD continuity changed; registry bytes restored, further recovery required' }
    } catch { $status.restoration_failure=$_.Exception.ToString() }
    if ($watchdog) {
        if ($watchdog.WaitForExit(10000)) { $status.watchdog_exit=$watchdog.ExitCode } else { $status.watchdog_still_running=$true }
        $watchdog.Dispose()
    }
    $watchResult=Join-Path $control 'watchdog-result.json'
    if (Test-Path -LiteralPath $watchResult) { $status.watchdog_result=Get-Content -LiteralPath $watchResult -Raw -Encoding UTF8 | ConvertFrom-Json }
    foreach ($file in $value.files) { try { Check-File $file.path $file.sha256 } catch { $status.input_continuity_failure=$_.Exception.Message } }
    # Never stop an uncompleted rescue task while restoration is unproved.
    if ($restoreRegistered -and $status.Contains('registry_settled') -and $status.registry_settled -and !($status.Contains('watchdog_still_running'))) {
        $restoreTask=Get-ScheduledTask -TaskName $value.restore_task_name -ErrorAction SilentlyContinue
        if ($restoreTask) {
            if ($restoreTask.State -ne 'Running') { $status.watchdog_task_exit=(Get-ScheduledTaskInfo -TaskName $value.restore_task_name).LastTaskResult }
            Unregister-ScheduledTask -TaskName $value.restore_task_name -Confirm:$false
        }
    }
    $self.Dispose()
    Write-Json (Join-Path $control 'controller-result.json') $status
}
} finally { if ($leaseHeld) { $lease.ReleaseMutex() }; $lease.Dispose() }
if ($status.Contains('failure') -or $status.Contains('restoration_failure') -or $status.Contains('input_continuity_failure') -or !$status.registry_restored -or !$status.Contains('slice_passed') -or !$status.Contains('watchdog_exit') -or $status.watchdog_exit -ne 0 -or !$status.Contains('watchdog_result') -or !$status.watchdog_result.restored) { exit 1 }
exit 0
