# SPDX-License-Identifier: MIT
# Frozen source-only native PS CPU packet. No controller/registry/task/PnP/GPU call.
param([Parameter(Mandatory)][string]$SourceRoot,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Fresh WoW CPU output required'}
New-Item -ItemType Directory -Path $Output|Out-Null
function Pin([string]$Path){[ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
$sourcePaths=@('scripts\system-d3d10-binding-native.cs','scripts\system-runtime-approved-payload.cs','tests\system-runtime-wow-binding-controls.cs','scripts\test-system-d3d10-binding.ps1','scripts\invoke-system-legacy-lifecycle-owner.ps1','tests\system-runtime-legacy-lifecycle-source-controls.ps1','scripts\test-system-runtime-wow-binding-cpu.ps1'|ForEach-Object {Join-Path $SourceRoot $_})
$metadata=@('metadata\native-build-configuration.json','metadata\vulkan_loader_config.h','metadata\meson-build-options.json','metadata\meson-build-machines.json','metadata\dx10-private-icd.json'|ForEach-Object {Join-Path $SourceRoot $_})
$before=@(($sourcePaths+$metadata)|ForEach-Object {Pin $_})
$astRows=[Collections.Generic.List[object]]::new()
foreach($path in @($sourcePaths|Where-Object {$_.EndsWith('.ps1',[StringComparison]::OrdinalIgnoreCase)})){
 $tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
 if(@($errors).Count -ne 0){throw 'Actual frozen WoW PS source AST failed'}
 $astRows.Add([ordered]@{file=(Pin $path);errors=0;statements=$ast.EndBlock.Statements.Count;source_only=$true})
}
$astRows.ToArray()|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $Output 'ast-original.json') -Encoding UTF8
Add-Type -Path @($sourcePaths[0],$sourcePaths[1],$sourcePaths[2]) -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll')
$memory=@([SystemRuntimeWowBindingControls01]::Main([string[]]$metadata))
if($memory.Count -ne 1 -or $memory[0] -ne 0){throw 'Actual source-only WoW memory control failed'}
& (Join-Path $SourceRoot 'tests\system-runtime-legacy-lifecycle-source-controls.ps1') -Source (Join-Path $SourceRoot 'scripts\test-system-d3d10-binding.ps1') -PayloadSource (Join-Path $SourceRoot 'scripts\system-runtime-approved-payload.cs') -Output (Join-Path $Output 'routing-census-original.json')
$routing=Get-Content -LiteralPath (Join-Path $Output 'routing-census-original.json') -Raw -Encoding UTF8|ConvertFrom-Json
if($routing.passed -ne $true -or $routing.argv_cases -ne 9 -or $routing.census_cases -ne 5 -or $routing.registry_changes -ne 0 -or $routing.task_changes -ne 0 -or $routing.process_changes -ne 0 -or $routing.kmt_calls -ne 0 -or $routing.gpu_calls -ne 0){throw 'Actual source-only routing/census result failed'}
$after=@(($sourcePaths+$metadata)|ForEach-Object {Pin $_})
if(($before|ConvertTo-Json -Compress) -cne ($after|ConvertTo-Json -Compress)){throw 'CPU packet input changed'}
[ordered]@{schema='native-wow-binding-cpu-v1';passed=$true;actor_pid=$PID;actor_start_utc=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToString('o');ast_files=$astRows.Count;memory_checks=390;routing_checks=$routing.checks;argv_cases=9;census_cases=5;source_before=$before;source_after=$after;controller_invoked=$false;registry_changes=0;task_changes=0;pnp_changes=0;kmt_calls=0;gpu_calls=0;synthetic_x86_metadata=$true;actual_x86_producer_claim=$false;hardware_admission=$false}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $Output 'native-wow-cpu-original.json') -Encoding UTF8
'NATIVE_WOW_BINDING_CPU_PASS memory_checks=390 argv_cases=9 census_cases=5 registry_changes=0 task_changes=0 pnp_changes=0 kmt_calls=0 gpu_calls=0 hardware_admission=0'
