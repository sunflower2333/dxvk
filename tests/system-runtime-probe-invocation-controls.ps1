# Actual production invocation receipt/functions with memory values only.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Fresh invocation controls output required'}
function Pin([string]$Path){[ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
$paths=@($Source,$PSCommandPath);$before=@($paths|ForEach-Object {Pin $_})
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)
if(@($errors).Count -ne 0){throw 'Actual production PS5.1 AST must pass'}
foreach($name in @('Quote','Probe-Arguments','Require-ProbeInvocation')){
 $functions=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
 if($functions.Count -ne 1){throw 'One actual production invocation function required'};. ([scriptblock]::Create($functions[0].Extent.Text))
}
$checks=0;$rows=[Collections.Generic.List[object]]::new()
function Check([bool]$Value){$script:checks++;if(!$Value){throw ('Invocation control '+$script:checks)}}
$value=[pscustomobject]@{api='11';owner_pid=100;owner_start_ticks=638955648000000001L;probe='C:\Fixture\probe.exe';probe_sha256=('1'*64);runner_sha256=('2'*64);output='C:\Fixture\worker';front='C:\Fixture\front.dll';core='C:\Fixture\viogpudxvk.dll';private_loader='C:\Fixture\loader.dll';vulkan_library='C:\Fixture\icd.dll';vulkan_icd='C:\Fixture\icd.json';hold_event='Local\Fixture-Hold'}
$ready=[pscustomobject]@{pid=200;start_utc='2026-10-09T00:00:00.0000000Z';worker_job_handle=300L}
$expected=Probe-Arguments $value '00000000:000084df';$configSha='3'*64
$base=[ordered]@{schema=2;owner_pid=100;owner_start_ticks=$value.owner_start_ticks;worker_pid=200;worker_start_utc=$ready.start_utc;worker_job_handle=300L;config_sha256=$configSha;executable=$value.probe;probe_sha256=$value.probe_sha256;arguments=$expected;runner_sha256=$value.runner_sha256;output=$value.output;invocation_timestamp=12345678901234L;invocation_frequency=10000000L;utc='2026-10-09T00:00:01.0000000Z'}
foreach($caseName in @('valid','owner-pid','owner-start','worker-pid','worker-start','worker-job','config','executable','probe','runner','output','argv','schema','utc','zero-tick','zero-frequency','string-tick','string-worker','decimal-frequency','boolean-owner','extra-field','missing-field','null-argv','empty-output','array-executable')){
 $marker=$base|ConvertTo-Json -Depth 8|ConvertFrom-Json
 switch($caseName){
  'owner-pid'{$marker.owner_pid=101}
  'owner-start'{$marker.owner_start_ticks=$value.owner_start_ticks+1}
  'worker-pid'{$marker.worker_pid=201}
  'worker-start'{$marker.worker_start_utc='2026-10-09T00:00:00.0000001Z'}
  'worker-job'{$marker.worker_job_handle=301L}
  'config'{$marker.config_sha256='4'*64}
  'executable'{$marker.executable='C:\Fixture\other.exe'}
  'probe'{$marker.probe_sha256='4'*64}
  'runner'{$marker.runner_sha256='4'*64}
  'output'{$marker.output='C:\Fixture\other'}
  'argv'{$marker.arguments=$expected+' extra'}
  'schema'{$marker.schema=3}
  'utc'{$marker.utc='2026-10-09T00:00:01.0000000+08:00'}
  'zero-tick'{$marker.invocation_timestamp=0L}
  'zero-frequency'{$marker.invocation_frequency=0L}
  'string-tick'{$marker.invocation_timestamp='12345678901234'}
  'string-worker'{$marker.worker_pid='200'}
  'decimal-frequency'{$marker.invocation_frequency=[decimal]10000000}
  'boolean-owner'{$marker.owner_pid=$true}
  'extra-field'{$marker|Add-Member -NotePropertyName extra -NotePropertyValue 0}
  'missing-field'{$marker.PSObject.Properties.Remove('arguments')}
  'null-argv'{$marker.arguments=$null}
  'empty-output'{$marker.output=''}
  'array-executable'{$marker.executable=@($value.probe,$value.probe)}
 }
 $failed=$false;$result=$null
 try{$result=Require-ProbeInvocation $value $ready $expected $configSha $marker}catch{$failed=$true}
 Check ($failed -eq ($caseName -cne 'valid'))
 if($failed){Check ($null -eq $result)}else{Check ($result.worker_pid -eq 200 -and $result.arguments -ceq $expected -and $result.invocation_timestamp -eq 12345678901234L)}
 $rows.Add([ordered]@{case=$caseName;rejected=$failed;controller_invoked=$false;hardware_admission=$false})
}
Check ($rows.Count -eq 25)
$after=@($paths|ForEach-Object {Pin $_});Check (($before|ConvertTo-Json -Compress) -ceq ($after|ConvertTo-Json -Compress))
[ordered]@{schema='owned-probe-invocation-source-memory-controls-v1';passed=$true;checks=$checks;receipt_cases=$rows.Count;actual_source_functions=3;source_before=$before;source_after=$after;cases=$rows.ToArray();controller_invoked=$false;registry_changes=0;task_changes=0;process_changes=0;kmt_calls=0;gpu_calls=0;hardware_admission=$false}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $Output -Encoding UTF8
'SYSTEM_RUNTIME_PROBE_INVOCATION_SOURCE_PASS checks='+$checks+' receipt_cases='+$rows.Count+' native_calls=0 registry_changes=0 gpu_calls=0 hardware_admission=0'
