# Actual selected argv statements and held census with memory process doubles.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$PayloadSource,
 [Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Fresh legacy routing/census output required'}
function Pin([string]$Path){[ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
$nativeSource=Join-Path (Split-Path $Source -Parent) 'system-d3d10-binding-native.cs'
$paths=@($Source,$PayloadSource,$nativeSource,$PSCommandPath);$before=@($paths|ForEach-Object {Pin $_})
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)
if(@($errors).Count -ne 0){throw 'Actual production PS5.1 AST must pass'}
foreach($name in @('Hash','Write-Json','Quote','Held-ModuleCensus')){
 $functions=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
 if($functions.Count -ne 1){throw 'One actual production function required'};. ([scriptblock]::Create($functions[0].Extent.Text))
}
if (-not ('DxvkApprovedPayloadPolicy01' -as [type])) { Add-Type -Path $PayloadSource -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll') }
if (-not ('DxvkBindingNative01' -as [type])) { Add-Type -Path $nativeSource }
$checks=0;$rows=[Collections.Generic.List[object]]::new()
function Check([bool]$Value){$script:checks++;if(!$Value){throw ('Legacy routing/census control '+$script:checks)}}
$statements=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left -is [Management.Automation.Language.VariableExpressionAst] -and $n.Left.VariablePath.UserPath -ceq 'args'},$true))
Check ($statements.Count -eq 5)
$value=[pscustomobject]@{api='9';d9_phase='offscreen';d10_profile='10_0';luid='00000000:00006bec';front='C:\Fixture\front.dll';core='C:\Fixture\arm64\viogpudxvk.dll';core_sha256=('5'*64);payload_source=('6'*40);d8_source_id='0';private_loader='C:\Fixture\private-loader.dll';vulkan_library='C:\Fixture\icd.dll';vulkan_icd='C:\Fixture\icd.json';hold_event='Local\Fixture-Hold'}
$probeLuid='00000000:000084df';$raw='C:\Fixture\originals'
foreach($case in @(@('8','offscreen','10_0'),@('8','present','10_0'),@('9','offscreen','10_0'),@('9','present','10_0'),@('9ex','offscreen','10_0'),@('9ex','present','10_0'),@('10','offscreen','10_0'),@('10','offscreen','10_1'),@('11','offscreen','10_0'))){
 $value.api=$case[0];$value.d9_phase=$case[1];$value.d10_profile=$case[2];$value.core='C:\Fixture\arm64\viogpudxvk.dll';if($value.api -ceq '8'){$value.core='C:\Fixture\x86\viogpudxvk.dll'}
 if($value.api -ceq '8'){$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith("'--system-'")});$expected='--system-'+$value.d9_phase+' "C:\Fixture\x86\viogpudxvk.dll" '+('5'*64)+' '+('6'*40)+' df84000000000000 0 "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 elseif($value.api -ceq '10'){$token='10 ';if($value.d10_profile -ceq '10_1'){$token='10.1 '};$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith("'"+$token+"'")});$expected=$token+'00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 elseif($value.api -ceq '11'){$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith('$probeLuid')});$expected='00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\private-loader.dll" "C:\Fixture\icd.dll" "C:\Fixture\icd.json" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 else{$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith('$value.api')});$expected=$value.api+' '+$value.d9_phase+' 00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\private-loader.dll" "C:\Fixture\icd.dll" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 Check ($selected.Count -eq 1)
 $actual=Invoke-Expression ($selected[0].Extent.Text+"`n"+'$args')
 Check ($actual -ceq $expected);Check (!$actual.Contains($value.luid))
 $rows.Add([ordered]@{kind='actual-argv';api=$value.api;phase=$value.d9_phase;d10_profile=$value.d10_profile;arguments=$actual;original_backup_luid=$value.luid;forward_probe_luid=$probeLuid;gpu_calls=0})
}
# Execute only the actual child-local diagnostic assignment and API branch.
# Every case starts from the production default so quiet mode cannot leak into
# the following Api9 attempt or inherit a caller's diagnostic environment.
$diagnosticDefault=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left -is [Management.Automation.Language.VariableExpressionAst] -and $n.Left.VariablePath.UserPath -ieq 'env:TU_WDDM_DIAGNOSTICS' -and $n.Right.Extent.Text -ceq "'1'"},$true))
$diagnosticRoute=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -ceq '$value.api -ceq ''10''' -and $n.Extent.Text.Contains('$env:TU_WDDM_DIAGNOSTICS')},$true))
Check ($diagnosticDefault.Count -eq 1);Check ($diagnosticRoute.Count -eq 1)
$savedDiagnostic=$env:TU_WDDM_DIAGNOSTICS
try {
 foreach($case in @(@('11','offscreen','10_0','0'),@('9','offscreen','10_0','1'),@('11','present','10_0','0'),@('9ex','present','10_0','1'),@('10','offscreen','10_0','0'),@('9','present','10_0','1'),@('10','offscreen','10_1','0'),@('9ex','offscreen','10_0','1'))){
  $value.api=$case[0];$value.d9_phase=$case[1];$value.d10_profile=$case[2];$status=[ordered]@{}
  Invoke-Expression $diagnosticDefault[0].Extent.Text
  Invoke-Expression $diagnosticRoute[0].Extent.Text
  Check ($env:TU_WDDM_DIAGNOSTICS -ceq $case[3])
  $rows.Add([ordered]@{kind='actual-diagnostic-routing';api=$value.api;phase=$case[1];d10_profile=$value.d10_profile;TU_WDDM_DIAGNOSTICS=$env:TU_WDDM_DIAGNOSTICS;gpu_calls=0})
 }
} finally {if($null -eq $savedDiagnostic){Remove-Item Env:TU_WDDM_DIAGNOSTICS -ErrorAction SilentlyContinue}else{$env:TU_WDDM_DIAGNOSTICS=$savedDiagnostic}}
$work=$Output+'.cases';New-Item -ItemType Directory -Path $work|Out-Null
$required=[Collections.Generic.List[object]]::new()
foreach($leaf in @('front.dll','viogpudxvk.dll','private-loader.dll','icd.dll')){
 $path=Join-Path $work $leaf;[IO.File]::WriteAllBytes($path,[byte[]]@(1,2,3))
 $required.Add([pscustomobject]@{Path=$path;Bytes=3;Sha256=(Hash $path)})
}
foreach($caseName in @('approved','missing-core','public-Vulkan','WARP','already-exited')){
 $outDir=Join-Path $work $caseName;New-Item -ItemType Directory -Path $outDir|Out-Null
 $modules=@($required|ForEach-Object {[pscustomobject]@{FileName=$_.Path}})
 if($caseName -ceq 'missing-core'){$modules=@($modules|Where-Object {$_.FileName -cne $required[1].Path})}
 if($caseName -ceq 'public-Vulkan'){$modules+=@([pscustomobject]@{FileName='C:\Fixture\vulkan-1.dll'})}
 if($caseName -ceq 'WARP'){$modules+=@([pscustomobject]@{FileName='C:\Fixture\d3d10warp.dll'})}
 $child=[pscustomobject]@{Id=200;StartTime=[DateTime]::Parse('2026-10-09T00:00:00Z').ToUniversalTime();Handle=[IntPtr]123;Modules=$modules;HasExited=($caseName -ceq 'already-exited');MainModule=[pscustomobject]@{FileName=$required[0].Path}}
 $selection=[pscustomobject]@{api='9';output=$outDir;module_files=$required.ToArray();payload_source=('3'*40);payload_ci_run='MEMORY-ONLY';approved_payload_sha256=('4'*64);probe=$required[0].Path}
 $failed=$false;$census=$null;try{$census=Held-ModuleCensus $child $selection}catch{$failed=$true}
 Check ($failed -eq ($caseName -cne 'approved'))
 $observationPath=Join-Path $outDir 'held-module-observation.json';Check (Test-Path -LiteralPath $observationPath)
 $observation=Get-Content -LiteralPath $observationPath -Raw -Encoding UTF8|ConvertFrom-Json
 Check ($observation.validation_pending -eq $true -and $observation.passed -eq $false -and $observation.hardware_admission -eq $false)
 Check ($observation.pid -eq 200 -and $observation.retained_handle -eq 123 -and $observation.start_utc -ceq $child.StartTime.ToUniversalTime().ToString('o'))
 Check ($observation.required.Count -eq 4 -and $observation.actual.Count -eq $modules.Count)
 Check ($observation.source_commit -ceq $selection.payload_source -and $observation.approved_payload_sha256 -ceq $selection.approved_payload_sha256)
 if(!$failed){Check ($census.passed -eq $true)}
 $rows.Add([ordered]@{kind='actual-census';case=$caseName;failed=$failed;observation=(Pin $observationPath);actual_paths=@($observation.actual|ForEach-Object {$_.Path});native_calls=0;hardware_admission=$false})
}
$after=@($paths|ForEach-Object {Pin $_});Check (($before|ConvertTo-Json -Compress) -ceq ($after|ConvertTo-Json -Compress))
[ordered]@{schema='legacy-lifecycle-source-routing-and-census-controls-v1';passed=$true;checks=$checks;argv_cases=9;diagnostic_cases=8;census_cases=5;actual_source_functions=4;source_before=$before;source_after=$after;cases=$rows.ToArray();controller_invoked=$false;registry_changes=0;task_changes=0;process_changes=0;kmt_calls=0;gpu_calls=0;hardware_admission=$false}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $Output -Encoding UTF8
'SYSTEM_LEGACY_LIFECYCLE_SOURCE_PASS checks='+$checks+' argv_cases=9 diagnostic_cases=8 census_cases=5 native_calls=0 registry_changes=0 gpu_calls=0 hardware_admission=0'
