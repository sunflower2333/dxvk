# SPDX-License-Identifier: MIT
# Explicit reversible adapter-wide validation experiment; lifecycle is ROOT opt-in.
param(
    [ValidateSet('Controller','Watchdog','Worker')][string]$Role='Controller',
    [string]$Config='', [string]$ConfigSha256='', [string]$RunRoot='',
    [string]$InstanceId='', [string]$Luid='',
    [ValidateSet('8','9','9ex','10','11')][string]$Api='10',
    [string]$D8SourceId='',
    [ValidateSet('10_0','10_1')][string]$D10Profile='10_0',
    [ValidateSet('offscreen','present')][string]$D9Phase='offscreen',
    [ValidateSet('offscreen','present')][string]$D11Phase='offscreen',
    [string]$PrivateLoader='', [string]$PrivateLoaderSha256='',
    [string]$Probe='', [string]$ProbeSha256='', [string]$Front='', [string]$FrontSha256='',
    [string]$Core='', [string]$CoreSha256='', [string]$Runner='',
    [string]$DriverSys='', [string]$DriverSysSha256='',
    [string]$VulkanIcd='', [string]$VulkanIcdSha256='',
    [string]$VulkanLibrarySha256='', [string]$VulkanLoaderSha256='',
    [string]$ApprovedPayload='', [string]$ApprovedPayloadSha256='',
    [string]$TokenScript='', [string]$TokenScriptSha256='',
    [switch]$ApplyReviewedTuple, [switch]$WaitForReviewedRefresh, [switch]$RootAuthorizeLifecycleRestart
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$script:LifecycleJobHandle=0L
$script:LifecycleWorkerJobHandle=0L
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
    # Retain actual paths even if strict module selection rejects a fallback.
    # This pending observation never substitutes for the completed census.
    Write-Json (Join-Path $Value.output 'held-module-observation.json') ([ordered]@{schema=1;pid=$Child.Id;start_utc=$start;retained_handle=$handle.ToInt64();source_commit=$Value.payload_source;ci_run=$Value.payload_ci_run;approved_payload_sha256=$Value.approved_payload_sha256;required=$required.ToArray();actual=$actual.ToArray();validation_pending=$true;passed=$false;gpu_calls=0;registry_mutations=0;hardware_admission=$false})
    [DxvkApprovedPayloadPolicy01]::RequireModulesForApi($Value.api,$required.ToArray(),$actual.ToArray())
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
function Require-D11Phase([string]$Api,[string]$Phase) {
    if ($Api -cnotin @('8','9','9ex','10','11') -or $Phase -cnotin @('offscreen','present') -or ($Api -cne '11' -and $Phase -cne 'offscreen')) { throw 'D11 Present phase requires API11 and an exact reviewed phase' }
}
function D8-Names($Value,[string]$Stage,[string]$CurrentLuid,[string[]]$Expected) {
    if ($Value.api -cne '8' -or $Stage -cnotmatch '^[a-z-]+$' -or $Expected.Count -ne 3) { throw 'Exact API8 read-only names phase required' }
    Check-File $Value.probe $Value.probe_sha256
    Check-File $Value.runner $Value.runner_sha256
    if (-not ('DxvkRawProcessF4_02' -as [type])) { Add-Type -Path $Value.runner }
    if ($script:LifecycleJobHandle -eq 0) { $script:LifecycleJobHandle=[DxvkBindingNative01]::OwnWorkerLifetime() }
    $prefix=Join-Path $Value.control ('d8-names-'+$Stage+'-'+$PID+'-'+[Guid]::NewGuid().ToString('N'))
    $out=$prefix+'.stdout.raw'; $err=$prefix+'.stderr.raw'
    $arguments='--system-names '+[DxvkBindingNative01]::LuidBytesForProbe($CurrentLuid)+' '+(Quote $Expected[0])+' '+(Quote $Expected[1])+' '+(Quote $Expected[2])+' '+(Quote $prefix)
    $row=[DxvkRawProcessF4_02]::Run($Value.probe,$arguments,$Value.output,$out,$err,20000)
    Write-Json ($prefix+'.process.json') (Process-Receipt $row $Value.runner_sha256)
    if (!$row.Exited -or !$row.ExitCodeAvailable -or $row.ExitCode -ne 0 -or $row.TimedOut -or $row.ChildStillRunning -or !$row.PipesDrained -or $row.Failure -or $row.StderrBytes -ne 0) { throw 'Actual original I386 read-only effective names producer failed' }
    $text=[DxvkBindingNative01]::ReadClosedText($prefix+'.names.json')
    if ($null -eq $text) { throw 'Closed original I386 effective names JSON required' }
    $proof=$text | ConvertFrom-Json
    $fields=@($proof.PSObject.Properties.Name | Sort-Object) -join ','
    if ($fields -cne 'core_loads,identity,luid16,names,registry_writes,runtime_calls,schema' -or $proof.schema -cne 'ordinary-system-d3d8-names-v1' -or $proof.luid16 -cne [DxvkBindingNative01]::LuidBytesForProbe($CurrentLuid) -or $proof.names.Count -ne 3 -or $proof.runtime_calls -ne 0 -or $proof.core_loads -ne 0 -or $proof.registry_writes -ne 0) { throw 'Exact original I386 names receipt identity required' }
    $paired=[DxvkBindingNative01]::RuntimeIdentity160([byte[]]$proof.identity,$CurrentLuid)
    [DxvkBindingNative01]::RequireIdentityMatch($paired,[DxvkBindingNative01]::CurrentIdentity())
    for ($index=0;$index -lt 3;++$index) {
        $name=$proof.names[$index]
        if ($name.version -ne $index -or $name.status -ne 0 -or $name.name -ine $Expected[$index] -or $name.expected -ine $Expected[$index] -or $name.words.Count -ne 260) { throw 'Actual I386 effective WoW slot differs' }
    }
    $stdout=[DxvkBindingNative01]::ReadSharedText($out)
    if ($stdout -cnotmatch '(?m)^D3D8_SYSTEM_NAMES_COMPLETE versions=3 runtime_calls=0 core_loads=0 registry_writes=0\r?$') { throw 'Actual I386 read-only names completion marker required' }
    [ordered]@{schema=1;phase=$Stage;actor_pid=$PID;current_luid=$CurrentLuid;expected=$Expected;owner_job_handle=$script:LifecycleJobHandle;process=(Process-Receipt $row $Value.runner_sha256);names=$proof;json_sha256=(Hash ($prefix+'.names.json'));passed=$true;runtime_calls=0;core_loads=0;registry_writes=0;hardware_admission=$false}
}
function Require-D10Profile([string]$Api,[string]$Profile) {
    if ($Profile -cnotin @('10_0','10_1') -or ($Api -cne '10' -and $Profile -cne '10_0')) { throw 'Exact D10.1 profile requires API10; other APIs retain their default' }
}
function D11-ValidationMarker([string]$Phase) {
    Require-D11Phase '11' $Phase
    if ($Phase -ceq 'present') { return 'SYSTEM_D3D11_PRESENT_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=2 software_fallback=0 production_admission=0 registry_changes=0' }
    'SYSTEM_D3D11_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=0 software_fallback=0 production_admission=0 registry_changes=0'
}
function Probe-Arguments($Value,[string]$ProbeLuid) {
    $raw=Join-Path $value.output 'originals'
    if ($value.api -ceq '8') {
        $args='--system-'+$value.d9_phase+' '+(Quote $value.core)+' '+$value.core_sha256+' '+$value.payload_source+' '+[DxvkBindingNative01]::LuidBytesForProbe($probeLuid)+' '+$value.d8_source_id+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
    } elseif ($value.api -ceq '10') {
        Require-D10Profile $value.api $value.d10_profile
        if ($value.d10_profile -ceq '10_1') { $args='10.1 '+$probeLuid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000' }
        else { $args='10 '+$probeLuid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000' }
    } elseif ($value.api -ceq '11') {
        $args=$probeLuid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $value.private_loader)+' '+(Quote $value.vulkan_library)+' '+(Quote $value.vulkan_icd)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
    } else {
        # Frozen739de05 argv7 is the loaded ICD DLL, not its JSON manifest.
        $args=$value.api+' '+$value.d9_phase+' '+$probeLuid+' '+(Quote $value.front)+' '+(Quote $value.core)+' '+(Quote $value.private_loader)+' '+(Quote $value.vulkan_library)+' '+(Quote $raw)+' '+(Quote $value.hold_event)+' 60000'
    }
    $args
}
function Require-ProbeInvocation($Value,$Ready,[string]$ExpectedArguments,[string]$ExpectedConfigSha,$Marker) {
    $fields=@($Marker.PSObject.Properties.Name | Sort-Object) -join ','
    if ($fields -cne 'arguments,config_sha256,executable,invocation_frequency,invocation_timestamp,output,owner_pid,owner_start_ticks,probe_sha256,runner_sha256,schema,utc,worker_job_handle,worker_pid,worker_start_utc' -or $Marker.schema -isnot [int] -or $Marker.schema -ne 2) { throw 'Exact owned probe invocation receipt required' }
    foreach ($field in @('owner_pid','owner_start_ticks','worker_pid','worker_job_handle','invocation_timestamp','invocation_frequency')) {
        if (($Marker.$field -isnot [int] -and $Marker.$field -isnot [long]) -or $Marker.$field -le 0) { throw 'Actual integer invocation identity/clock required' }
    }
    if ($Marker.owner_pid -ne $Value.owner_pid -or $Marker.owner_start_ticks -ne $Value.owner_start_ticks -or $Marker.worker_pid -ne $Ready.pid -or $Marker.worker_start_utc -cne $Ready.start_utc -or $Marker.worker_job_handle -ne $Ready.worker_job_handle -or $Marker.config_sha256 -cne $ExpectedConfigSha -or $Marker.executable -ine $Value.probe -or $Marker.probe_sha256 -cne $Value.probe_sha256 -or $Marker.runner_sha256 -cne $Value.runner_sha256 -or $Marker.output -ine $Value.output -or $Marker.arguments -cne $ExpectedArguments) { throw 'Probe invocation differs from the protected original owner/worker/config/argv' }
    foreach ($field in @('worker_start_utc','config_sha256','executable','probe_sha256','runner_sha256','output','arguments')) { if ($Marker.$field -isnot [string] -or !$Marker.$field) { throw 'Actual nonempty invocation strings required' } }
    $utc=[DateTime]::MinValue
    if ($Marker.utc -isnot [string] -or ![DateTime]::TryParseExact($Marker.utc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind,[ref]$utc) -or $utc.Kind -ne [DateTimeKind]::Utc) { throw 'Actual invocation UTC observation required; it does not prove child PID/start' }
    $Marker
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
function Lifecycle-Enabled($Value) { if ($Value -is [Collections.IDictionary]) { return $Value.Contains('lifecycle_mode') -and $Value.lifecycle_mode -eq $true }; $null -ne $Value.PSObject.Properties['lifecycle_mode'] -and $Value.lifecycle_mode -eq $true }
function Lifecycle-Installed($Value) {
    $devices=@(Get-PnpDevice -PresentOnly -Class Display | Where-Object {$_.InstanceId -match '^PCI\\VEN_1AF4&DEV_1050(?:&|\\)'})
    if ($devices.Count -ne 1 -or $devices[0].InstanceId -cne $Value.instance) { throw 'Same one present VIOGPU instance required for lifecycle operation' }
    $key=[string](Get-PnpDeviceProperty -InstanceId $Value.instance -KeyName 'DEVPKEY_Device_Driver').Data
    if (('SYSTEM\CurrentControlSet\Control\Class\'+$key) -cne $Value.registry_subkey) { throw 'Display class binding changed during lifecycle transition' }
    $driver=Driver-State $Value.instance
    if (($driver | ConvertTo-Json -Compress) -cne ($Value.lifecycle_driver_state | ConvertTo-Json -Compress)) { throw 'Installed KMD identity/package changed during lifecycle transition' }
    [ordered]@{instance=$Value.instance;registry_subkey=$Value.registry_subkey;driver=$driver;status=$devices[0].Status}
}
function Lifecycle-State($Value) {
    $installed=Lifecycle-Installed $Value
    if ($installed.status -cne 'OK') { throw 'Current VIOGPU devnode is not healthy' }
    $driver=$installed.driver
    $identity=[DxvkBindingNative01]::CurrentIdentity()
    $names=[DxvkBindingNative01]::Names($identity.Luid); Check-Names $names $identity.Luid
    [ordered]@{schema=1;instance=$Value.instance;registry_subkey=$Value.registry_subkey;identity=$identity;names=$names;driver=$driver;utc=[DateTime]::UtcNow.ToString('o')}
}
function Wait-LifecycleState($Value) {
    $timer=[Diagnostics.Stopwatch]::StartNew(); $last=''
    do { try { return Lifecycle-State $Value } catch { $last=$_.Exception.Message }; Start-Sleep -Milliseconds 200 } while ($timer.ElapsedMilliseconds -lt 12000)
    throw ('Current healthy paired160 lifecycle identity unavailable: '+$last)
}
function Lifecycle-Desktop($Value) {
    $explorer=@(Get-Process explorer -IncludeUserName); $dwm=@(Get-Process dwm)
    if ($explorer.Count -ne 1 -or $dwm.Count -ne 1 -or !$explorer[0].UserName -or $explorer[0].SessionId -ne $Value.desktop_session -or $dwm[0].SessionId -ne $Value.desktop_session) { throw 'Healthy original interactive desktop session required after lifecycle recovery' }
    $sid=([Security.Principal.NTAccount]$explorer[0].UserName).Translate([Security.Principal.SecurityIdentifier]).Value
    if ($sid -cne $Value.desktop_sid) { throw 'Original desktop user changed during lifecycle recovery' }
    [ordered]@{healthy=$true;sid=$sid;session=$Value.desktop_session;dwm_pid=$dwm[0].Id;explorer_pid=$explorer[0].Id;pid_retention_required=$false}
}
function Lifecycle-Restart($Value,[string]$Phase) {
    if (!(Lifecycle-Enabled $Value) -or $Phase -notin @('forward','reverse')) { throw 'Explicit same-owner lifecycle mode required' }
    $installed=Lifecycle-Installed $Value
    Check-File $Value.lifecycle_pnputil $Value.lifecycle_pnputil_sha256
    Check-File $Value.runner $Value.runner_sha256
    if ($Value.lifecycle_pnputil -ine (Join-Path ([Environment]::SystemDirectory) 'pnputil.exe') -or $Value.instance -cnotmatch '^PCI\\VEN_1AF4&DEV_1050(?:&|\\)[A-Za-z0-9_&\\-]+$') { throw 'Exact installed devnode and genuine SYSTEM pnputil only' }
    if (-not ('DxvkRawProcessF4_02' -as [type])) { Add-Type -Path $Value.runner }
    if ($script:LifecycleJobHandle -eq 0) { $script:LifecycleJobHandle=[DxvkBindingNative01]::OwnWorkerLifetime() }
    $prefix=Join-Path $Value.control ($Phase+'-restart-'+$PID)
    foreach ($suffix in @('.stdout.raw','.stderr.raw','.process.json')) { if (Test-Path -LiteralPath ($prefix+$suffix)) { throw 'Fresh bounded lifecycle originals required' } }
    $arguments='/restart-device '+(Quote $Value.instance)
    $r=[DxvkRawProcessF4_02]::Run($Value.lifecycle_pnputil,$arguments,$Value.control,($prefix+'.stdout.raw'),($prefix+'.stderr.raw'),20000)
    $receipt=Process-Receipt $r $Value.runner_sha256
    $receipt.phase=$Phase; $receipt.instance=$Value.instance; $receipt.arguments=$arguments; $receipt.executable=$Value.lifecycle_pnputil; $receipt.executable_sha256=$Value.lifecycle_pnputil_sha256; $receipt.job_handle=$script:LifecycleJobHandle
    $receipt.stdout=[ordered]@{path=($prefix+'.stdout.raw');sha256=(Hash ($prefix+'.stdout.raw'))}; $receipt.stderr=[ordered]@{path=($prefix+'.stderr.raw');sha256=(Hash ($prefix+'.stderr.raw'))}
    Write-Json ($prefix+'.process.json') $receipt
    if (!$r.Exited -or !$r.ExitCodeAvailable -or $r.ExitCode -ne 0 -or $r.TimedOut -or $r.ChildStillRunning -or !$r.PipesDrained -or $r.Failure) { throw 'Exact-instance lifecycle operation failed/timed out; owned originals retained' }
    [ordered]@{receipt=$receipt;receipt_path=($prefix+'.process.json');state=(Wait-LifecycleState $Value)}
}
function Lifecycle-OriginalProcess([int]$PidValue,[string]$Start,[string]$Executable,[string]$ExpectedHash,[int]$WaitMs,[bool]$Kill) {
    $child=$null
    try { try { $child=[Diagnostics.Process]::GetProcessById($PidValue) } catch [ArgumentException] { return [ordered]@{pid=$PidValue;gone=$true;already_absent=$true;pid_reused=$false} }
        $handle=$child.Handle
        if ($child.WaitForExit(0)) { return [ordered]@{pid=$PidValue;gone=$true;exited_before_identity_query=$true;retained_handle=$handle.ToInt64();original_exit_claim=$false} }
        $actualStart=$child.StartTime.ToUniversalTime().ToString('o')
        if ($actualStart -cne $Start) { return [ordered]@{pid=$PidValue;gone=$true;already_absent=$false;pid_reused=$true;current_start_utc=$actualStart} }
        if ($child.MainModule.FileName -ine $Executable -or (Hash $child.MainModule.FileName) -cne $ExpectedHash) { throw 'Refusing unrelated or changed lifecycle child' }
        $exited=$child.WaitForExit($WaitMs); $killed=$false
        if (!$exited -and $Kill) { $child.Kill(); $killed=$true; $exited=$child.WaitForExit(5000) }
        [ordered]@{pid=$PidValue;start_utc=$Start;retained_handle=$handle.ToInt64();gone=$exited;killed=$killed;exit_code=$(if($exited){$child.ExitCode}else{$null});original_exit_claim=$false}
    } finally { if ($child) { $child.Dispose() } }
}
function Lifecycle-AcquireJob($Value) {
    if ($script:LifecycleWorkerJobHandle -gt 0) { return [ordered]@{retained_handle=$script:LifecycleWorkerJobHandle;already_retained=$true;previous_boot=$false} }
    $worker=$null; $handle=0L
    $readyPath=Join-Path $Value.control 'lifecycle-worker-original.json'
    if (Test-Path -LiteralPath $readyPath) {
        $ready=Get-Content -LiteralPath $readyPath -Raw -Encoding UTF8 | ConvertFrom-Json
        try {
            try { $worker=[Diagnostics.Process]::GetProcessById([int]$ready.pid) } catch [ArgumentException] {}
            if ($worker -and !$worker.WaitForExit(0) -and $worker.StartTime.ToUniversalTime().ToString('o') -ceq $ready.start_utc) {
                if ($worker.MainModule.FileName -ine $Value.lifecycle_powershell -or (Hash $worker.MainModule.FileName) -cne $Value.lifecycle_powershell_sha256) { throw 'Original job worker identity differs' }
                $handle=$worker.Handle.ToInt64()
            }
            $script:LifecycleWorkerJobHandle=[DxvkBindingNative01]::OpenLifecycleWorkerJob($Value.lifecycle_worker_job_name,$handle)
        } finally { if ($worker) { $worker.Dispose() } }
    }
    if ($script:LifecycleWorkerJobHandle -le 0) {
        $boot=(Get-CimInstance -ClassName Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().ToString('o')
        if ($boot -ceq $Value.lifecycle_boot_utc) { throw 'Same-boot original job absence cannot prove unpublished probe teardown' }
        return [ordered]@{retained_handle=0;already_retained=$false;previous_boot=$true;original_boot_utc=$Value.lifecycle_boot_utc;current_boot_utc=$boot}
    }
    [ordered]@{retained_handle=$script:LifecycleWorkerJobHandle;already_retained=$false;previous_boot=$false}
}
function Lifecycle-CompleteJob($Scope) {
    if ($Scope.previous_boot) { return [ordered]@{active_processes=0;previous_boot=$true;exact_job_completion=$false;old_boot_processes_cannot_span_restart=$true} }
    $active=[DxvkBindingNative01]::CompleteLifecycleWorkerJob([long]$Scope.retained_handle,10000)
    [ordered]@{retained_handle=$Scope.retained_handle;active_processes=$active;previous_boot=$false;exact_job_completion=$true;termination_requested_only_when_active=$true}
}

function Lifecycle-Cleanup($Value) {
    # Raw replay/release is already durable. The retained original job must
    # report zero active processes BEFORE reverse Stop/Start.
    $jobScope=Lifecycle-AcquireJob $Value
    $readyPath=Join-Path $Value.control 'lifecycle-worker-original.json'; $workerGone=$false; $worker=$null
    if (!(Test-Path -LiteralPath $readyPath) -and (Test-Path -LiteralPath (Join-Path $Value.control 'forward-lifecycle-intent.json'))) { throw 'Forward lifecycle intent requires its protected original worker checkpoint' }
    if (Test-Path -LiteralPath $readyPath) {
        $ready=Get-Content -LiteralPath $readyPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($ready.worker_job_handle -le 0 -or !$ready.start_utc -or $ready.sid -cne $Value.desktop_sid -or $ready.session -ne $Value.desktop_session) { throw 'Original lifecycle worker identity/job checkpoint required' }
        $worker=Lifecycle-OriginalProcess ([int]$ready.pid) $ready.start_utc $Value.lifecycle_powershell $Value.lifecycle_powershell_sha256 20000 $false
        $workerGone=$worker.gone
    }
    $task=Get-ScheduledTask -TaskName $Value.task_name -ErrorAction SilentlyContinue
    if (!$workerGone -or ($task -and $task.State -eq 'Running')) {
        if ($task) {
            $config=Join-Path $Value.control 'config.json'; $expected='-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File '+(Quote $Value.lifecycle_controller)+' -Role Worker -Config '+(Quote $config)+' -ConfigSha256 '+(Hash $config)
            if (@($task.Actions).Count -ne 1 -or $task.Actions[0].Execute -ine $Value.lifecycle_powershell -or $task.Actions[0].Arguments -cne $expected) { throw 'Refusing unrelated lifecycle scheduled worker' }
            Stop-ScheduledTask -TaskName $Value.task_name
        }
        if (Test-Path -LiteralPath $readyPath) { $worker=Lifecycle-OriginalProcess ([int]$ready.pid) $ready.start_utc $Value.lifecycle_powershell $Value.lifecycle_powershell_sha256 5000 $false; $workerGone=$worker.gone }
        else { $remaining=Get-ScheduledTask -TaskName $Value.task_name -ErrorAction SilentlyContinue; $workerGone=(!$remaining -or $remaining.State -ne 'Running') }
    }
    $jobCompletion=Lifecycle-CompleteJob $jobScope
    if (Test-Path -LiteralPath $readyPath) { $worker=Lifecycle-OriginalProcess ([int]$ready.pid) $ready.start_utc $Value.lifecycle_powershell $Value.lifecycle_powershell_sha256 5000 $false; $workerGone=$worker.gone }
    $taskClock=[Diagnostics.Stopwatch]::StartNew()
    do { $remaining=Get-ScheduledTask -TaskName $Value.task_name -ErrorAction SilentlyContinue; if (!$remaining -or $remaining.State -ne 'Running') { break }; Start-Sleep -Milliseconds 100 } while ($taskClock.ElapsedMilliseconds -lt 5000)
    $workerGone=$workerGone -and (!$remaining -or $remaining.State -ne 'Running')
    $probeGone=$jobCompletion.active_processes -eq 0; $probe=$null; $heldPath=Join-Path $Value.output 'worker-held.json'
    if (Test-Path -LiteralPath $heldPath) {
        $held=Get-Content -LiteralPath $heldPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $probe=Lifecycle-OriginalProcess ([int]$held.pid) $held.start_utc $Value.probe $Value.probe_sha256 5000 $true
        $probeGone=$probe.gone
    }
    [DxvkBindingNative01]::RequireLifecycleRestartPermit($true,$workerGone,$probeGone)
    [ordered]@{schema=1;worker_gone=$workerGone;probe_gone=$probeGone;worker=$worker;published_probe=$probe;original_job_completion=$jobCompletion;unpublished_children_closed_by_original_job_completion=($jobCompletion.active_processes -eq 0);complete_tree_attestation=$false}
}
function Check-LifecycleIntent($Value,[string]$Phase) {
    $path=Join-Path $Value.control ($Phase+'-lifecycle-intent.json')
    if (!(Test-Path -LiteralPath $path)) { return }
    $intent=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($intent.schema -ne 1 -or $intent.owner_pid -ne $Value.owner_pid -or $intent.instance -cne $Value.instance -or $intent.registry_subkey -cne $Value.registry_subkey -or $intent.original_backup_luid -cne $Value.luid) { throw 'Protected lifecycle intent differs from the original backup owner/adapter' }
    if ($Phase -ceq 'reverse' -and $intent.raw_restored_sha256 -cne (Hash (Join-Path $Value.control 'restored.json'))) { throw 'Reverse intent raw restoration proof differs' }
}
function Wait-LifecycleDesktop($Value) {
    $timer=[Diagnostics.Stopwatch]::StartNew(); $last=''
    do { try { return Lifecycle-Desktop $Value } catch { $last=$_.Exception.Message }; Start-Sleep -Milliseconds 200 } while ($timer.ElapsedMilliseconds -lt 12000)
    throw ('Original interactive desktop did not become healthy after recovery: '+$last)
}
function Settle-Lifecycle($Value,$RawSettlement) {
    if (!$RawSettlement.raw_tuple_restored) {
        if (Test-Path -LiteralPath (Join-Path $Value.control 'forward-lifecycle-intent.json')) { throw 'Lifecycle intent cannot be settled without actual raw replay' }
        return [ordered]@{enabled=$true;cancelled_without_registry_write=$true;effective_restoration_required=$false;hardware_admission=$false}
    }
    Check-LifecycleIntent $Value 'forward'; Check-LifecycleIntent $Value 'reverse'
    $cleanup=Lifecycle-Cleanup $Value
    [DxvkBindingNative01]::RequireSnapshot((Raw-Rows $Value.original),[DxvkBindingNative01]::Snapshot($Value.registry_subkey))
    $state=$null; $reverse=$null; $alreadyOriginal=$false; $beforeFailure=''
    # Failed forward Stop/Start can leave no current paired identity. Exact
    # installed instance/class/KMD, raw restore and closed producers authorize
    # the reverse recovery; full readiness is mandatory AFTER that operation.
    try { $state=Wait-LifecycleState $Value; [DxvkBindingNative01]::RequireLifecycleNames($state.identity.Luid,[string[]]$Value.original_slots,$state.names); if ($Value.api -ceq '8') { $wowBefore=D8-Names $Value 'before-reverse' $state.identity.Luid ([string[]]$Value.original_wow_slots) }; $alreadyOriginal=$true } catch { $beforeFailure=$_.Exception.Message }
    if (!$alreadyOriginal) {
        $intent=Join-Path $Value.control 'reverse-lifecycle-intent.json'
        if (!(Test-Path -LiteralPath $intent)) { Write-MutationIntent $intent ([ordered]@{schema=1;owner_pid=$Value.owner_pid;instance=$Value.instance;registry_subkey=$Value.registry_subkey;original_backup_luid=$Value.luid;raw_restored_sha256=(Hash (Join-Path $Value.control 'restored.json'));utc=[DateTime]::UtcNow.ToString('o')}) }
        $reverse=Lifecycle-Restart $Value 'reverse'; $state=$reverse.state
    }
    [DxvkBindingNative01]::RequireLifecycleNames($state.identity.Luid,[string[]]$Value.original_slots,$state.names)
    [DxvkBindingNative01]::RequireSnapshot((Raw-Rows $Value.original),[DxvkBindingNative01]::Snapshot($Value.registry_subkey))
    $wowRestored=$null; if ($Value.api -ceq '8') { $wowRestored=D8-Names $Value 'restored' $state.identity.Luid ([string[]]$Value.original_wow_slots) }
    $desktop=Wait-LifecycleDesktop $Value
    $proof=[ordered]@{schema=1;actor_pid=$PID;original_backup_luid=$Value.luid;forward_probe_luid=$null;restored_luid=$state.identity.Luid;raw_settlement=[ordered]@{raw_tuple_restored=$RawSettlement.raw_tuple_restored;mutation_intent_present=$RawSettlement.mutation_intent_present;proof_sha256=(Hash (Join-Path $Value.control 'restored.json'))};cleanup=$cleanup;reverse=$reverse;before_reverse_readiness_failure=$beforeFailure;already_effective_original=$alreadyOriginal;state=$state;desktop=$desktop;effective_original_names_restored=$true;hardware_admission=$false}
    if ($Value.api -ceq '8') { $proof.wow_restoration=$wowRestored }
    $forward=Join-Path $Value.control 'lifecycle-forward.json'
    if (Test-Path -LiteralPath $forward) { $forwardState=Get-Content -LiteralPath $forward -Raw -Encoding UTF8 | ConvertFrom-Json; $proof.forward_probe_luid=$forwardState.state.identity.Luid }
    $path=Join-Path $Value.control ('lifecycle-restored-'+$PID+'.json')
    if (!(Test-Path -LiteralPath $path)) { Write-Json $path $proof }
    return $proof
}

function Restore-Tuple($Value,[string]$Reason) {
    $mutex=[Threading.Mutex]::new($false,$Value.mutex)
    $locked=$false
    try {
        $waitMs=10000
        if (($Value -is [Collections.IDictionary] -and $Value.Contains('lifecycle_mode') -and $Value.lifecycle_mode) -or
            ($Value -isnot [Collections.IDictionary] -and $null -ne $Value.PSObject.Properties['lifecycle_mode'] -and $Value.lifecycle_mode)) { $waitMs=180000 }
        try { $locked=$mutex.WaitOne($waitMs) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (!$locked) { throw 'Restoration mutex deadline exceeded' }
        $rows=Raw-Rows $Value.original
        $intent=Join-Path $Value.control 'mutation-intent.json'
        $replay=Test-Path -LiteralPath $intent
        if ($replay) {
            $phase=Get-Content -LiteralPath $intent -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($Value.api -ceq '8') {
                if ($phase.schema -ne 3 -or $phase.owner_pid -ne $Value.owner_pid -or $phase.registry_subkey -cne $Value.registry_subkey -or $phase.selected_wow_slot -ne 0 -or $phase.selected_value_name -cne 'UserModeDriverNameWoW' -or $phase.selected_view -ne 0x100 -or $phase.only_wow_legacy_slot -ne $true -or $Value.binding_value_name -cne 'UserModeDriverNameWoW') { throw 'Protected API8 WoW mutation intent differs from this backup owner' }
            } elseif ($phase.schema -ne 2 -or $phase.owner_pid -ne $Value.owner_pid -or $phase.registry_subkey -cne $Value.registry_subkey -or $phase.selected_native_slot -ne $Value.native_slot -or $Value.native_slot -notin @(0,1,2) -or $phase.only_native_tuple_slot -ne $true) { throw 'Protected mutation intent differs from this backup owner/selected slot' }
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
        if (($Value -is [Collections.IDictionary] -and $Value.Contains('lifecycle_mode') -and $Value.lifecycle_mode) -or
            ($Value -isnot [Collections.IDictionary] -and $null -ne $Value.PSObject.Properties['lifecycle_mode'] -and $Value.lifecycle_mode)) {
            $settled.lifecycle=Settle-Lifecycle $Value $settled
        }
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

if ($Role -ceq 'Controller') { Require-D11Phase $Api $D11Phase; Require-D10Profile $Api $D10Profile }
$native=Join-Path $PSScriptRoot 'system-d3d10-binding-native.cs'
if ($Role -ne 'Controller') {
    Check-File $Config $ConfigSha256
    $value=Get-Content -LiteralPath $Config -Raw -Encoding UTF8 | ConvertFrom-Json
    # A changed probe/payload must stop new GPU work, not prevent replaying the
    # protected raw backup. Only the restoration implementation is required
    # before the watchdog can restore; the worker checks every selected input.
    if ($Role -eq 'Worker') {
        Require-D11Phase $value.api $value.d11_phase
        Require-D10Profile $value.api $value.d10_profile
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
if ($Role -ceq 'Controller') {
    $lifecyclePhase=$D11Phase; if ($Api -in @('8','9','9ex')) { $lifecyclePhase=$D9Phase }
    [DxvkBindingNative01]::RequireLifecycleMode($RootAuthorizeLifecycleRestart.IsPresent,$ApplyReviewedTuple.IsPresent,$Api,$lifecyclePhase,$WaitForReviewedRefresh.IsPresent)
}
if ($Role -ne 'Controller') {
    if ($Role -eq 'Watchdog') {
        $owner=$null; $reason='deadline'; $status=[ordered]@{schema=1;role='Watchdog';restored=$false;hardware_admission=$false}
        try {
            $owner=[Diagnostics.Process]::GetProcessById([int]$value.owner_pid)
            $ownerHandle=$owner.Handle
            if ($owner.StartTime.ToUniversalTime().Ticks -ne [long]$value.owner_start_ticks) { throw 'Controller PID was reused before watchdog armed' }
            Write-Json (Join-Path $value.control 'watchdog-ready.json') ([ordered]@{pid=$PID;start_utc=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToString('o');owner_pid=$owner.Id;owner_handle=$ownerHandle.ToInt64();owner_start_ticks=$value.owner_start_ticks;deadline_utc=$value.deadline_utc})
            $watchdogDeadline=[DateTime]::Parse($value.deadline_utc).ToUniversalTime()
            $watchdogRemainingMs=[Math]::Max(0,[Math]::Min(($watchdogDeadline-[DateTime]::UtcNow).TotalMilliseconds,600000))
            $watchdogClock=[Diagnostics.Stopwatch]::StartNew()
            while ([DateTime]::UtcNow -lt $watchdogDeadline -and $watchdogClock.ElapsedMilliseconds -lt $watchdogRemainingMs) {
                if ((Lifecycle-Enabled $value) -and $script:LifecycleWorkerJobHandle -eq 0 -and (Test-Path -LiteralPath (Join-Path $value.control 'lifecycle-worker-original.json'))) {
                    $scope=Lifecycle-AcquireJob $value
                    if ($scope.previous_boot -or $scope.retained_handle -le 0) { throw 'Actual same-boot worker job must arm before binding' }
                    Write-Json (Join-Path $value.control 'lifecycle-watchdog-job-ready.json') ([ordered]@{schema=1;pid=$PID;owner_pid=$value.owner_pid;job_name=$value.lifecycle_worker_job_name;retained_handle=$scope.retained_handle})
                }
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
        if (Lifecycle-Enabled $value) { $status.worker_job_handle=[DxvkBindingNative01]::OwnLifecycleWorkerLifetime($value.lifecycle_worker_job_name,$value.desktop_sid) }
        else { $status.worker_job_handle=[DxvkBindingNative01]::OwnWorkerLifetime() }
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
        } else { $status.native_entry_negative_executed=$false; $status.native_entry_negative_contract='Frozen739de05 ordinary D9 probe has no typed entry-negative mode'; if ($value.api -ceq '8') { $status.native_entry_negative_contract='Original I386 ordinary API8 probe has no typed entry-negative mode' } }
        $probeLuid=$value.luid
        $before=[DxvkBindingNative01]::Names($value.luid); Check-Names $before $value.luid
        for ($index=0;$index -lt 3;++$index) { if ($before.Names[$index].Name -ine $value.original_slots[$index]) { throw 'Actual original KMT names differ from the three-slot tuple' } }
        Write-Json (Join-Path $value.output 'kmt-before.json') $before
        $workerStart=[Diagnostics.Process]::GetCurrentProcess().StartTime.ToUniversalTime().ToString('o')
        Write-Json (Join-Path $value.output 'worker-ready.json') ([ordered]@{pid=$PID;start_utc=$workerStart;sid=$token.sid;session=$token.session_id;worker_job_handle=$status.worker_job_handle})
        $clock=[Diagnostics.Stopwatch]::StartNew()
        while (!(Test-Path -LiteralPath (Join-Path $value.control 'bound.json'))) {
            if ((Test-Path -LiteralPath (Join-Path $value.control 'release.json')) -or $clock.ElapsedMilliseconds -gt 60000) { throw 'Binding was cancelled or never armed' }
            Start-Sleep -Milliseconds 100
        }
        if (Lifecycle-Enabled $value) {
            $forward=Get-Content -LiteralPath (Join-Path $value.control 'lifecycle-forward.json') -Raw -Encoding UTF8 | ConvertFrom-Json
            if ($forward.schema -ne 1 -or $forward.owner_pid -ne $value.owner_pid -or $forward.instance -cne $value.instance -or $forward.original_backup_luid -cne $value.luid) { throw 'Protected current forward lifecycle identity required' }
            $protected=[DxvkBindingNative01]::RuntimeIdentity160([byte[]]$forward.state.identity.Raw,$forward.state.identity.Luid)
            $current=Lifecycle-State $value; [DxvkBindingNative01]::RequireIdentityMatch($protected,$current.identity)
            $probeLuid=$protected.Luid; $status.original_backup_luid=$value.luid; $status.forward_probe_luid=$probeLuid
        }
        $bound=[DxvkBindingNative01]::Names($probeLuid); Check-Names $bound $probeLuid
        Write-Json (Join-Path $value.output 'kmt-bound.json') $bound
        if ($value.api -ceq '8') {
            [DxvkBindingNative01]::RequireLifecycleNames($probeLuid,[string[]]$value.original_slots,$bound)
            if ($forward.wow_names.passed -ne $true -or $forward.wow_names.current_luid -cne $probeLuid -or $forward.wow_names.expected[0] -ine $value.core) { throw 'Protected actual I386 WoW candidate selection required' }
            $status.effective_candidate_selected=$true; $status.wow_candidate_names=$forward.wow_names
        } else { $status.effective_candidate_selected=$bound.Names[$value.native_slot].Name -ieq $value.front }
        # Existing proven runtime-lifecycle payload contract, confined to this
        # worker and its owned probe; no desktop/system environment changes.
        if ($value.api -ceq '8') {
            if ($null -ne [Environment]::GetEnvironmentVariable('VK_DRIVER_FILES','Process') -or $null -ne [Environment]::GetEnvironmentVariable('VK_ICD_FILENAMES','Process')) { throw 'Ordinary API8 probe requires absent inherited Vulkan selectors' }
            $status.vulkan_environment=[ordered]@{selectors_absent=$true;scope='original I386 probe owns its exact selector set/restore'}
        } else {
            $env:VK_DRIVER_FILES=$value.vulkan_icd; $env:VK_ICD_FILENAMES=$value.vulkan_icd
            $env:VK_LOADER_DEBUG='error,warn,driver'; $env:DXVK_LOG_PATH=$value.output; $env:DXVK_LOG_LEVEL='info'; $env:TU_WDDM_DIAGNOSTICS='1'
            # Exact Api10/11 success execution retains the strict zero-stderr gate.
            # Api9 diagnostic attempts keep the existing first-submit telemetry.
            if ($value.api -ceq '10') { $env:TU_WDDM_DIAGNOSTICS='0'; $env:VK_LOADER_DEBUG='error'; $status.d10_profile=$value.d10_profile }
            elseif ($value.api -ceq '11') { $env:TU_WDDM_DIAGNOSTICS='0'; $env:VK_LOADER_DEBUG='error' }
            $status.vulkan_environment=[ordered]@{VK_DRIVER_FILES=$value.vulkan_icd;VK_ICD_FILENAMES=$value.vulkan_icd;VK_LOADER_DEBUG=$env:VK_LOADER_DEBUG;TU_WDDM_DIAGNOSTICS=$env:TU_WDDM_DIAGNOSTICS;scope='worker/owned child only'}
        }
        $stdout=Join-Path $value.output 'probe.stdout.raw'; $stderr=Join-Path $value.output 'probe.stderr.raw'
        $raw=Join-Path $value.output 'originals'
        $probeArguments=Probe-Arguments $value $probeLuid
        # This is the invocation anchor, not an assertion of the child's PID
        # or process start. Those still come solely from the retained runner.
        $invocationTimestamp=[Diagnostics.Stopwatch]::GetTimestamp()
        Write-Json (Join-Path $value.output 'probe-start.json') ([ordered]@{schema=2;owner_pid=$value.owner_pid;owner_start_ticks=$value.owner_start_ticks;worker_pid=$PID;worker_start_utc=$workerStart;worker_job_handle=$status.worker_job_handle;config_sha256=$ConfigSha256;executable=$value.probe;probe_sha256=$value.probe_sha256;arguments=$probeArguments;runner_sha256=$value.runner_sha256;output=$value.output;invocation_timestamp=$invocationTimestamp;invocation_frequency=[Diagnostics.Stopwatch]::Frequency;utc=[DateTime]::UtcNow.ToString('o')})
        $task=[DxvkBindingAsync01]::Start($value.probe,$probeArguments,$value.output,$stdout,$stderr,[DxvkBindingHoldBudget01]::ProbeMilliseconds)
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
                    if ($value.api -ceq '8') {
                        $fields=@($held.PSObject.Properties.Name | Sort-Object) -join ','
                        if ($fields -cne 'device_alive,hold_event,modules_exact,output,pending_exit,pid,pixels_passed,restoration_proved_by_event,schema,selector_installed,stage,timeout_ms' -or $held.schema -cne 'ordinary-system-d3d8-held-v1' -or $held.pid -isnot [int] -or $held.pid -le 0 -or $held.timeout_ms -ne 60000 -or $held.pending_exit -notin @(0,1) -or $held.hold_event -cne $value.hold_event -or $held.output -ine $raw -or $held.restoration_proved_by_event -ne $false -or $held.selector_installed -ne $false -or $held.stage -isnot [string]) { throw 'Exact closed ordinary API8 hold checkpoint required' }
                        foreach ($field in @('pixels_passed','device_alive','modules_exact','selector_installed','restoration_proved_by_event')) { if ($held.$field -isnot [bool]) { throw 'Actual API8 hold booleans required' } }
                        if ($held.pending_exit -eq 0 -and (!$held.pixels_passed -or !$held.device_alive -or !$held.modules_exact)) { throw 'API8 successful hold requires actual pixels/live device/exact modules' }
                        $heldPid=[int]$held.pid; $pendingExit=[int]$held.pending_exit; $status.d8_closed_checkpoint=$held
                    } elseif ($value.api -ceq '11') {
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
                    $heldRecord=[ordered]@{pid=$child.Id;start_utc=$child.StartTime.ToUniversalTime().ToString('o');retained_handle=$handle.ToInt64();pending_exit=$pendingExit;stdout_bytes=(Get-Item -LiteralPath $stdout).Length;api=$value.api}
                    if ($value.api -ceq '8') { $heldRecord.selected_wow_slot=0; $heldRecord.selected_value_name='UserModeDriverNameWoW' } else { $heldRecord.native_slot=$value.native_slot }
                    Write-Json (Join-Path $value.output 'worker-held.json') $heldRecord
                } finally { $child.Dispose() }
                $publishedPid=$heldPid; $published=$true
            }
            $release=Join-Path $value.control 'release.json'
            if ($published -and !$released -and (Test-Path -LiteralPath $release)) {
                $proof=Join-Path $value.control 'restored.json'
                $permission=Get-Content -LiteralPath $release -Raw -Encoding UTF8 | ConvertFrom-Json
                Check-File $proof $permission.restored_sha256
                if ($permission.raw_tuple_restored -ne $true -or $permission.cancelled_without_registry_write -ne $false) { throw 'Held factory cannot be released by a no-mutation cancellation checkpoint' }
                $after=[DxvkBindingNative01]::Names($probeLuid); Check-Names $after $probeLuid
                Write-Json (Join-Path $value.output 'kmt-after-restoration-before-release.json') $after
                $status.effective_original_names_restored=Same-Names $before $after
                if (Lifecycle-Enabled $value) { $status.effective_original_names_restoration_deferred_until_probe_reaped=$true }
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
        if (!$published -or !$released -or !$status.held_module_census_passed -or $row.Pid -ne $publishedPid -or !$row.Exited -or !$row.ExitCodeAvailable -or $row.ExitCode -ne 0 -or $row.TimedOut -or $row.ChildStillRunning -or !$row.PipesDrained -or $row.Failure -or !$status.effective_candidate_selected -or (!(Lifecycle-Enabled $value) -and !$status.effective_original_names_restored)) { throw 'Actual factory/selection/held/modules/restore process gate failed; raw failure retained' }
        if ($value.api -ceq '8') {
            $finished=[DxvkBindingNative01]::ReadSharedText($stdout)
            $presents=0; $screenPixels=0; if ($value.d9_phase -ceq 'present') { $presents=1; $screenPixels=64 }
            $render='D3D8_SYSTEM_RENDER PASS stages=7 pixels=448 presents='+$presents+' screen_pixels='+$screenPixels+' selector_installed=0'
            $complete='D3D8_COMPLETE mode=system-'+$value.d9_phase+' adapters=[1-9][0-9]* create_device=1 presents='+$presents+' registry_writes=0'
            if ($finished -cnotmatch ('(?m)^'+[regex]::Escape($render)+'\r?$') -or $finished -cnotmatch ('(?m)^'+$complete+'\r?$') -or $finished -cnotmatch '(?m)^D3D8_SYSTEM_HELD_END wait=0 pending_exit=0 restoration_proved_by_event=0\r?$' -or $finished -cnotmatch '(?m)^D3D8_SYSTEM_EVENT_RELEASE released=1\r?$' -or $row.StderrBytes -ne 0) { throw 'Actual ordinary API8 render/hold/envrestore/release markers failed; independent raw reader remains required' }
            foreach ($role in @(0,1,2)) { if ($finished -cnotmatch ('(?m)^D3D8_SYSTEM_MODULE_RELEASE role='+$role+' released=1\r?$')) { throw 'Actual ordinary API8 owned private module release required' } }
            $status.d8_phase=$value.d9_phase
        } elseif ($value.api -ceq '11') {
            $finished=[DxvkBindingNative01]::ReadSharedText($stdout)
            $marker=D11-ValidationMarker $value.d11_phase
            if ($finished -cnotmatch ('(?m)^'+[regex]::Escape($marker)+'\r?$') -or $finished -cnotmatch ('(?m)^SYSTEM_D3D11_HELD pid='+$publishedPid+' timeout_ms=60000 pixels_passed=1 stage= hr=00000000\r?$') -or $row.StderrBytes -ne 0) { throw 'Actual dedicated D11 selected-phase factory/readback/release markers failed; independent raw reader remains required' }
            $status.d11_phase=$value.d11_phase
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
$bindingValueName='UserModeDriverName'; if ($Api -ceq '8') { $bindingValueName='UserModeDriverNameWoW' }
if (!$ApplyReviewedTuple) {
    $readonly=[DxvkBindingNative01]::Snapshot($subkey)
    $readonlyNames=[DxvkBindingNative01]::Names($Luid); Check-Names $readonlyNames $Luid
    $readonlyRecord=[ordered]@{schema=1;instance=$InstanceId;driver=$driver;luid=$Luid;api=$Api;native_slot=$nativeSlot;registry_subkey=$subkey;original=$readonly;names=$readonlyNames;hardware_admission=$false;mutation=$false}
    if ($Api -ceq '8') { $readonlyRecord.Remove('native_slot'); $readonlyRecord.selected_wow_slot=0; $readonlyRecord.selected_value_name='UserModeDriverNameWoW'; $readonlyRecord.wow_tuple=[DxvkBindingNative01]::Tuple($readonly[1]) }
    Write-Json (Join-Path $RunRoot 'readonly-original.json') $readonlyRecord
    exit 0
}
if ($Api -ceq '8' -and (!$RootAuthorizeLifecycleRestart -or $D8SourceId -cnotmatch '^(0|[1-9][0-9]{0,9})$' -or [uint64]$D8SourceId -gt [uint32]::MaxValue -or !$RunRoot.StartsWith('C:\Users\Public\DxvkD8Lifecycle-',[StringComparison]::OrdinalIgnoreCase))) { throw 'API8 requires ROOT lifecycle mode, explicit source ID and exact D8 run prefix' }
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
$applyRecord=[ordered]@{schema=1;instance=$InstanceId;driver=$driver;luid=$Luid;api=$Api;native_slot=$nativeSlot;registry_subkey=$subkey;original=$original;names=$names;exclusive_adapter_lease_held=$true;hardware_admission=$false;mutation=$false}
if ($Api -ceq '8') { $applyRecord.Remove('native_slot'); $applyRecord.selected_wow_slot=0; $applyRecord.selected_value_name='UserModeDriverNameWoW' }
Write-Json (Join-Path $RunRoot 'apply-original.json') $applyRecord
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
if ($Api -ceq '8') {
    if ($Front -ine $Core -or $FrontSha256 -cne $CoreSha256 -or (Split-Path $Core -Leaf) -cne 'viogpudxvk.dll' -or [DxvkBindingNative01]::ProbeArchitectureForApi('8',[IO.File]::ReadAllBytes($Probe)) -cne 'x86' -or [DxvkBindingNative01]::ProbeArchitectureForApi('8',[IO.File]::ReadAllBytes($Core)) -cne 'x86') { throw 'API8 requires matching original I386 probe and direct original core with no separate frontend' }
} elseif ($Api -ceq '10') {
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
$selection.Api=$Api; $selection.Architecture=[DxvkBindingNative01]::ProbeArchitectureForApi($Api,[IO.File]::ReadAllBytes($Probe))
$selection.Core=$Core; $selection.CoreSha256=$CoreSha256; $selection.PrivateLoader=$PrivateLoader; $selection.PrivateLoaderSha256=$PrivateLoaderSha256
$selection.IcdJson=$VulkanIcd; $selection.IcdJsonSha256=$VulkanIcdSha256; $selection.IcdLibrarySha256=$VulkanLibrarySha256; $selection.VulkanLoaderSha256=$VulkanLoaderSha256
$approved=[DxvkApprovedPayloadPolicy01]::ValidateFiles($ApprovedPayload,$ApprovedPayloadSha256,$selection)
$vulkanLibrary=$approved.IcdLibrary; $vulkanLoader=$approved.PrivateLoader
if ($Api -ceq '8') { $root='C:\Users\Public\DxvkD3D8Candidate-'+$approved.SourceCommit.Substring(0,7)+'-'+$approved.CiRun; if ($Core -ine ($root+'\viogpudxvk.dll') -and $Core -ine ($root+'-icd02\viogpudxvk.dll')) { throw 'API8 directcore path must match its exact selected published source/run' } }
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
$value.d11_phase=$D11Phase
[DxvkBindingNative01]::BindingValueName($Api,$selection.Architecture) | Out-Null
if ($Api -ceq '8') { $value.Remove('native_slot'); $value.selected_wow_slot=0; $value.binding_value_name='UserModeDriverNameWoW'; $value.original_wow_slots=[DxvkBindingNative01]::Tuple($original[1]); $value.d8_source_id=$D8SourceId; $value.hold_event='Local\VioGpuD8Validation-'+$runId }
$value.d10_profile=$D10Profile
$value.lifecycle_mode=$RootAuthorizeLifecycleRestart.IsPresent
if ($value.lifecycle_mode) {
    $script:LifecycleJobHandle=[DxvkBindingNative01]::OwnWorkerLifetime()
    $value.lifecycle_controller_job_handle=$script:LifecycleJobHandle; $value.deadline_utc=[DateTime]::UtcNow.AddSeconds([DxvkBindingHoldBudget01]::LifecycleWatchdogSeconds).ToString('o')
    $value.instance=$InstanceId; $value.lifecycle_driver_state=$driverState; $value.lifecycle_controller=$controllerScript
    $value.lifecycle_worker_job_name='Global\VioGpuLifecycleWorker-'+$runId
    $value.lifecycle_boot_utc=(Get-CimInstance -ClassName Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().ToString('o')
    $value.lifecycle_powershell=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'; $value.lifecycle_powershell_sha256=Hash $value.lifecycle_powershell
    $value.lifecycle_pnputil=Join-Path ([Environment]::SystemDirectory) 'pnputil.exe'; $value.lifecycle_pnputil_sha256=Hash $value.lifecycle_pnputil
    $initial=Lifecycle-State $value
    if ($initial.identity.Luid -cne $Luid) { throw 'Original current identity differs before lifecycle mode arms' }
    [DxvkBindingNative01]::RequireLifecycleNames($Luid,[string[]]$slots,$initial.names)
    Write-Json (Join-Path $control 'lifecycle-original.json') $initial
}
$value.files=@($controllerScript,$nativeCopy,$runnerCopy,$tokenCopy,$Probe,$Front,$Core,$DriverSys,$VulkanIcd,$vulkanLibrary,$vulkanLoader | ForEach-Object { [ordered]@{path=$_;sha256=(Hash $_)} })
$value.payload_source=$approved.SourceCommit; $value.payload_ci_run=$approved.CiRun; $value.approved_payload_sha256=$ApprovedPayloadSha256
$value.module_files=@($approved.ModuleFiles); if ($Api -cne '8') { $value.module_files=@([ordered]@{Path=$Front;Bytes=(Get-Item -LiteralPath $Front).Length;Sha256=$FrontSha256})+@($approved.ModuleFiles) }
if ($Api -ceq '8') { $value.original_wow_names=D8-Names $value 'original' $Luid ([string[]]$value.original_wow_slots) }
$value.files+=@([ordered]@{path=$payloadCopy;sha256=(Hash $payloadCopy)},[ordered]@{path=$ApprovedPayload;sha256=$ApprovedPayloadSha256})
$value.files+=@($approved.Files | ForEach-Object { [ordered]@{path=$_.Path;sha256=$_.Sha256} })
$configFile=Join-Path $control 'config.json'; Write-Json $configFile $value; $configHash=Hash $configFile
$sourceProfile='unregistered-validation8eeb20'; if ($Api -ceq '11') { $sourceProfile='dedicated-SYSTEM-D11-FL10_0-validation' } elseif ($Api -ceq '8') { $sourceProfile='ordinary-SYSTEM-D8-I386-direct-core' } elseif ($Api -cne '10') { $sourceProfile='ordinary-D9-probe739de05' }
if ($Api -ceq '10' -and $D10Profile -ceq '10_1') { $sourceProfile='ordinary-SYSTEM-D10.1-validation' }
$lifecycleWorker=$null; $watchdog=$null; $registered=$false; $restoreRegistered=$false; $status=[ordered]@{schema=1;source=$sourceProfile;api=$Api;native_slot=$nativeSlot;registry_restored=$false;hardware_admission=$false;production_admission=$false;default_replacement=$false}
if ($Api -ceq '8') { $status.Remove('native_slot'); $status.selected_wow_slot=0; $status.selected_value_name='UserModeDriverNameWoW' }
try {
    $power=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    # A service-owned task avoids inheriting an SSH session's process job.
    # Its retained controller handle and bounded deadline survive owner loss.
    $restorePrincipal=New-ScheduledTaskPrincipal -UserId 'S-1-5-18' -LogonType ServiceAccount -RunLevel Highest
    $restoreArgs='-NoProfile -ExecutionPolicy Bypass -File '+(Quote $controllerScript)+' -Role Watchdog -Config '+(Quote $configFile)+' -ConfigSha256 '+$configHash
    $restoreAction=New-ScheduledTaskAction -Execute $power -Argument $restoreArgs
    $restoreSeconds=240; if ($value.lifecycle_mode) { $restoreSeconds=[DxvkBindingHoldBudget01]::WatchdogTaskSeconds }
    $restoreSettings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Seconds $restoreSeconds)
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
    $workerSeconds=180; if ($value.lifecycle_mode) { $workerSeconds=[DxvkBindingHoldBudget01]::WorkerSeconds }
    $settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Seconds $workerSeconds)
    Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Settings $settings | Out-Null; $registered=$true
    Start-ScheduledTask -TaskName $taskName
    $clock.Restart()
    while (!(Test-Path -LiteralPath (Join-Path $output 'worker-ready.json'))) { if ((Test-Path -LiteralPath (Join-Path $output 'worker-result.json')) -or $clock.ElapsedMilliseconds -gt 20000) { throw 'Interactive original-name/token gate failed' }; Start-Sleep -Milliseconds 100 }
    if ($value.lifecycle_mode) {
        $ready=Get-Content -LiteralPath (Join-Path $output 'worker-ready.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $lifecycleWorker=[Diagnostics.Process]::GetProcessById([int]$ready.pid); $lifecycleWorkerHandle=$lifecycleWorker.Handle
        $command=Get-CimInstance -ClassName Win32_Process -Filter ('ProcessId='+[int]$ready.pid)
        if (!$ready.start_utc -or $lifecycleWorker.StartTime.ToUniversalTime().ToString('o') -cne $ready.start_utc -or $lifecycleWorker.MainModule.FileName -ine $power -or (Hash $power) -cne $value.lifecycle_powershell_sha256 -or $lifecycleWorker.SessionId -ne $value.desktop_session -or !$command.CommandLine -or !$command.CommandLine.EndsWith($arguments,[StringComparison]::Ordinal) -or $ready.worker_job_handle -le 0 -or $ready.sid -cne $value.desktop_sid -or $ready.session -ne $value.desktop_session) { throw 'Original exact scheduled lifecycle worker/PID/start/command/job required' }
        Write-Json (Join-Path $control 'lifecycle-worker-original.json') ([ordered]@{pid=$lifecycleWorker.Id;start_utc=$ready.start_utc;retained_handle=$lifecycleWorkerHandle.ToInt64();sid=$ready.sid;session=$ready.session;worker_job_handle=$ready.worker_job_handle;command_line=$command.CommandLine;task_name=$taskName})
        $jobScope=Lifecycle-AcquireJob $value; if ($jobScope.previous_boot -or $jobScope.retained_handle -le 0) { throw 'Original worker job not retained before binding' }
        $clock.Restart(); $armedPath=Join-Path $control 'lifecycle-watchdog-job-ready.json'
        while (!(Test-Path -LiteralPath $armedPath)) { if ($clock.ElapsedMilliseconds -gt 5000) { throw 'Independent watchdog did not retain original worker job before binding' }; Start-Sleep -Milliseconds 100 }
        $jobArmed=Get-Content -LiteralPath $armedPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($jobArmed.schema -ne 1 -or $jobArmed.pid -ne $watchdog.Id -or $jobArmed.owner_pid -ne $PID -or $jobArmed.job_name -cne $value.lifecycle_worker_job_name -or $jobArmed.retained_handle -le 0) { throw 'Original watchdog job checkpoint differs' }
    }
    $mutex=[Threading.Mutex]::new($false,$value.mutex); $locked=$false
    try {
        try { $locked=$mutex.WaitOne(5000) } catch [Threading.AbandonedMutexException] { $locked=$true }
        if (!$locked -or (Test-Path -LiteralPath (Join-Path $control 'restored.json')) -or [DateTime]::UtcNow -ge [DateTime]::Parse($value.deadline_utc).ToUniversalTime()) { throw 'Binding cannot start after watchdog restoration/deadline' }
        [DxvkBindingNative01]::RequireSnapshot($original,[DxvkBindingNative01]::Snapshot($subkey))
        # Same restore mutex covers recheck, durable intent and the first write.
        # Owner loss after intent but before/during RegSet conservatively restores.
        if ($Api -ceq '8') {
            $replacement=[DxvkBindingNative01]::ReplaceWowLegacy($original[1],$Core)
            Write-MutationIntent (Join-Path $control 'mutation-intent.json') ([ordered]@{schema=3;owner_pid=$PID;registry_subkey=$subkey;selected_wow_slot=0;selected_value_name='UserModeDriverNameWoW';selected_view=0x100;only_wow_legacy_slot=$true;utc=[DateTime]::UtcNow.ToString('o')})
        } else {
            $replacement=[DxvkBindingNative01]::ReplaceNativeSlot($original[0],$Front,$nativeSlot)
            Write-MutationIntent (Join-Path $control 'mutation-intent.json') ([ordered]@{schema=2;owner_pid=$PID;registry_subkey=$subkey;selected_native_slot=$nativeSlot;only_native_tuple_slot=$true;utc=[DateTime]::UtcNow.ToString('o')})
        }
        [DxvkBindingNative01]::Write($subkey,$replacement)
        if (![DxvkBindingNative01]::Same($replacement,[DxvkBindingNative01]::Read($subkey,0x100,$bindingValueName))) { throw 'Exact selected registration slot write did not read back' }
        if ($Api -ceq '8') { [DxvkBindingNative01]::RequireWowMutation($original,[DxvkBindingNative01]::Snapshot($subkey),$replacement) }
        else { foreach ($index in @(1,2,4,5)) { if (![DxvkBindingNative01]::Same($original[$index],([DxvkBindingNative01]::Snapshot($subkey))[$index])) { throw 'Untouched WoW/installed tuple changed' } } }
        Write-Json (Join-Path $control 'candidate-tuple-written.json') ([ordered]@{utc=[DateTime]::UtcNow.ToString('o');actual=[DxvkBindingNative01]::Snapshot($subkey);luid=$Luid;instance=$InstanceId;external_refresh_wait=$WaitForReviewedRefresh.IsPresent})
        $status.registry_changed=$true
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
    if ($value.lifecycle_mode) {
        $mutex=[Threading.Mutex]::new($false,$value.mutex); $locked=$false
        try {
            try { $locked=$mutex.WaitOne(5000) } catch [Threading.AbandonedMutexException] { $locked=$true }
            if (!$locked -or (Test-Path -LiteralPath (Join-Path $control 'restored.json'))) { throw 'Lifecycle restart cannot race rescue restoration' }
            if (![DxvkBindingNative01]::Same($replacement,[DxvkBindingNative01]::Read($subkey,0x100,$bindingValueName))) { throw 'Candidate changed before exact-instance forward restart' }
            Write-MutationIntent (Join-Path $control 'forward-lifecycle-intent.json') ([ordered]@{schema=1;owner_pid=$PID;instance=$InstanceId;registry_subkey=$subkey;original_backup_luid=$Luid;utc=[DateTime]::UtcNow.ToString('o')})
            $forward=Lifecycle-Restart $value 'forward'; $expected=[string[]]$slots.Clone(); $wowForward=$null
            if ($Api -ceq '8') {
                $wowExpected=[string[]]$value.original_wow_slots.Clone(); $wowExpected[0]=$Core
                $wowForward=D8-Names $value 'forward' $forward.state.identity.Luid $wowExpected
            } else { $expected[$nativeSlot]=$Front }
            [DxvkBindingNative01]::RequireLifecycleNames($forward.state.identity.Luid,$expected,$forward.state.names)
            $forwardRecord=[ordered]@{schema=1;owner_pid=$PID;instance=$InstanceId;original_backup_luid=$Luid;state=$forward.state;restart=$forward.receipt}
            if ($Api -ceq '8') { $forwardRecord.wow_names=$wowForward }
            Write-Json (Join-Path $control 'lifecycle-forward.json') $forwardRecord
            $status.original_backup_luid=$Luid; $status.forward_probe_luid=$forward.state.identity.Luid
        } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
    }
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
        if (![DxvkBindingNative01]::Same($replacement,[DxvkBindingNative01]::Read($subkey,0x100,$bindingValueName))) { throw 'Candidate tuple changed during refresh checkpoint' }
        $boundTimestamp=[Diagnostics.Stopwatch]::GetTimestamp()
        if ($Api -ceq '8') { Write-Json (Join-Path $control 'bound.json') ([ordered]@{utc=[DateTime]::UtcNow.ToString('o');actual=[DxvkBindingNative01]::Snapshot($subkey);api=$Api;selected_wow_slot=0;selected_value_name='UserModeDriverNameWoW';only_wow_legacy_slot_edited=$true;native_values_untouched=$true;adapter_wide=$true}) }
        else { Write-Json (Join-Path $control 'bound.json') ([ordered]@{utc=[DateTime]::UtcNow.ToString('o');actual=[DxvkBindingNative01]::Snapshot($subkey);api=$Api;selected_native_slot=$nativeSlot;only_native_tuple_slot_edited=$true;adapter_wide=$true}) }
    } finally { if ($locked) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
    $clock.Restart()
    $holdBudget=$null; $invocationPin=''; $invocationPath=Join-Path $output 'probe-start.json'
    if ($value.lifecycle_mode) {
        if (![Diagnostics.Stopwatch]::IsHighResolution) { throw 'Same-boot monotonic invocation clock required' }
        $holdBudget=[DxvkBindingHoldBudget01]::new($boundTimestamp,[Diagnostics.Stopwatch]::Frequency)
        $expectedProbeArguments=Probe-Arguments $value $forward.state.identity.Luid
    }
    while (!(Test-Path -LiteralPath (Join-Path $output 'worker-held.json')) -or ($value.lifecycle_mode -and !$invocationPin)) {
        if (Test-Path -LiteralPath (Join-Path $output 'worker-result.json')) { throw 'Actual probe ended before bounded hold' }
        if ($value.lifecycle_mode) {
            if (!$invocationPin -and (Test-Path -LiteralPath $invocationPath)) {
                # Read only this worker's fresh atomic path, freeze it once, and
                # account from its QPC invocation tick, not first observation.
                $pendingInvocationHash=Hash $invocationPath
                $invocationText=[DxvkBindingNative01]::ReadClosedText($invocationPath)
                if ($null -ne $invocationText) {
                    Check-File $invocationPath $pendingInvocationHash
                    $invocation=Require-ProbeInvocation $value $ready $expectedProbeArguments $configHash ($invocationText | ConvertFrom-Json)
                    $holdBudget.AdmitInvocation([long]$invocation.invocation_timestamp,[long]$invocation.invocation_frequency,[Diagnostics.Stopwatch]::GetTimestamp())
                    $invocationPin=$pendingInvocationHash
                    $status.probe_invocation_budget=[ordered]@{schema=1;invocation=$invocationPath;invocation_sha256=$invocationPin;bound_timestamp=$holdBudget.BoundTimestamp;invocation_timestamp=$holdBudget.InvocationTimestamp;frequency=$holdBudget.Frequency;startup_ms=[DxvkBindingHoldBudget01]::StartupMilliseconds;probe_ms=[DxvkBindingHoldBudget01]::ProbeMilliseconds;invocation_hold_ms=[DxvkBindingHoldBudget01]::InvocationHoldMilliseconds;overall_ms=[DxvkBindingHoldBudget01]::OverallMilliseconds;anchor='owned worker invocation; not claimed child PID/start';hardware_admission=$false}
                    Write-Json (Join-Path $control 'probe-invocation-budget.json') $status.probe_invocation_budget
                }
            }
            if ($invocationPin) { Check-File $invocationPath $invocationPin }
            $holdBudget.RequirePending([Diagnostics.Stopwatch]::GetTimestamp())
        } elseif ($clock.ElapsedMilliseconds -gt 80000) { throw 'Actual probe did not reach bounded hold' }
        Start-Sleep -Milliseconds 100
    }
    if ($value.lifecycle_mode) {
        if (!$invocationPin) { throw 'Owned invocation anchor must precede lifecycle hold' }
        # Admission is checked at acceptance too: a late hold written during
        # the final sleep cannot skip time or frozen-receipt continuity gates.
        Check-File $invocationPath $invocationPin
        $holdBudget.RequirePending([Diagnostics.Stopwatch]::GetTimestamp())
    }
    $settled=Restore-Tuple $value 'held-probe'; $status.registry_restored=$settled.raw_tuple_restored
    if ($value.lifecycle_mode) { $status.lifecycle_recovery=$settled.lifecycle; $status.restored_luid=$settled.lifecycle.restored_luid }
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
        if ($value.lifecycle_mode) {
            $status.lifecycle_recovery=$settled.lifecycle; $status.restored_luid=$settled.lifecycle.restored_luid
            $restoredState=Lifecycle-State $value; $status.final_names=$restoredState.names
            [DxvkBindingNative01]::RequireLifecycleNames($restoredState.identity.Luid,[string[]]$slots,$status.final_names)
            $status.effective_original_names_restored=$true
        } else {
            $status.final_names=[DxvkBindingNative01]::Names($Luid); Check-Names $status.final_names $Luid
            $status.effective_original_names_restored=Same-Names $names $status.final_names
        }
        $afterDwm=@(Get-Process dwm); $afterExplorer=@(Get-Process explorer)
        $status.desktop_retained=$afterDwm.Count -eq 1 -and $afterExplorer.Count -eq 1 -and $afterDwm[0].Id -eq $value.desktop_dwm_pid -and $afterExplorer[0].Id -eq $value.desktop_explorer_pid
        if ($value.lifecycle_mode) { $status.lifecycle_desktop=Lifecycle-Desktop $value; $status.desktop_healthy=$true }
        else { $status.desktop_healthy=$status.desktop_retained }
        $status.devnode_retained=(Get-PnpDevice -PresentOnly -InstanceId $InstanceId).Status -ceq 'OK' -and [string](Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName 'DEVPKEY_Device_Driver').Data -ceq $driver
        $driverAfter=Driver-State $InstanceId; Write-Json (Join-Path $control 'driver-state-after.json') $driverAfter
        $status.kmd_package_retained=($driverState | ConvertTo-Json -Compress) -ceq ($driverAfter | ConvertTo-Json -Compress)
        if (!$status.effective_original_names_restored -or !$status.desktop_healthy -or !$status.devnode_retained -or !$status.kmd_package_retained) { throw 'Effective name/desktop/devnode/KMD continuity changed; registry bytes restored, further recovery required' }
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
    if ($lifecycleWorker) { $lifecycleWorker.Dispose() }
    $self.Dispose()
    Write-Json (Join-Path $control 'controller-result.json') $status
}
} finally { if ($leaseHeld) { $lease.ReleaseMutex() }; $lease.Dispose() }
if ($status.Contains('failure') -or $status.Contains('restoration_failure') -or $status.Contains('input_continuity_failure') -or !$status.registry_restored -or !$status.Contains('slice_passed') -or !$status.Contains('watchdog_exit') -or $status.watchdog_exit -ne 0 -or !$status.Contains('watchdog_result') -or !$status.watchdog_result.restored) { exit 1 }
exit 0
