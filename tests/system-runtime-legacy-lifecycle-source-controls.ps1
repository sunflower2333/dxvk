# Actual selected argv statements and held census with memory process doubles.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$PayloadSource,
 [Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Fresh legacy routing/census output required'}
function Pin([string]$Path){[ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
$paths=@($Source,$PayloadSource,$PSCommandPath);$before=@($paths|ForEach-Object {Pin $_})
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)
if(@($errors).Count -ne 0){throw 'Actual production PS5.1 AST must pass'}
foreach($name in @('Hash','Write-Json','Quote','Held-ModuleCensus')){
 $functions=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq $name},$true))
 if($functions.Count -ne 1){throw 'One actual production function required'};. ([scriptblock]::Create($functions[0].Extent.Text))
}
Add-Type -Path $PayloadSource -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll')
$checks=0;$rows=[Collections.Generic.List[object]]::new()
function Check([bool]$Value){$script:checks++;if(!$Value){throw ('Legacy routing/census control '+$script:checks)}}
$statements=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left -is [Management.Automation.Language.VariableExpressionAst] -and $n.Left.VariablePath.UserPath -ceq 'args'},$true))
Check ($statements.Count -eq 3)
$value=[pscustomobject]@{api='9';d9_phase='offscreen';luid='00000000:00006bec';front='C:\Fixture\front.dll';core='C:\Fixture\arm64\viogpudxvk.dll';private_loader='C:\Fixture\private-loader.dll';vulkan_library='C:\Fixture\icd.dll';vulkan_icd='C:\Fixture\icd.json';hold_event='Local\Fixture-Hold'}
$probeLuid='00000000:000084df';$raw='C:\Fixture\originals'
foreach($case in @(@('9','offscreen'),@('9','present'),@('9ex','offscreen'),@('9ex','present'),@('10','offscreen'),@('11','offscreen'))){
 $value.api=$case[0];$value.d9_phase=$case[1]
 if($value.api -ceq '10'){$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith("'10 '")});$expected='10 00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 elseif($value.api -ceq '11'){$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith('$probeLuid')});$expected='00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\private-loader.dll" "C:\Fixture\icd.dll" "C:\Fixture\icd.json" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 else{$selected=@($statements|Where-Object {$_.Right.Extent.Text.StartsWith('$value.api')});$expected=$value.api+' '+$value.d9_phase+' 00000000:000084df "C:\Fixture\front.dll" "C:\Fixture\arm64\viogpudxvk.dll" "C:\Fixture\private-loader.dll" "C:\Fixture\icd.dll" "C:\Fixture\originals" "Local\Fixture-Hold" 60000'}
 Check ($selected.Count -eq 1)
 $actual=Invoke-Expression ($selected[0].Extent.Text+"`n"+'$args')
 Check ($actual -ceq $expected);Check (!$actual.Contains($value.luid))
 $rows.Add([ordered]@{kind='actual-argv';api=$value.api;phase=$value.d9_phase;arguments=$actual;original_backup_luid=$value.luid;forward_probe_luid=$probeLuid;gpu_calls=0})
}
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
 $selection=[pscustomobject]@{output=$outDir;module_files=$required.ToArray();payload_source=('3'*40);payload_ci_run='MEMORY-ONLY';approved_payload_sha256=('4'*64);probe=$required[0].Path}
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
[ordered]@{schema='legacy-lifecycle-source-routing-and-census-controls-v1';passed=$true;checks=$checks;argv_cases=6;census_cases=5;actual_source_functions=4;source_before=$before;source_after=$after;cases=$rows.ToArray();controller_invoked=$false;registry_changes=0;task_changes=0;process_changes=0;kmt_calls=0;gpu_calls=0;hardware_admission=$false}|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $Output -Encoding UTF8
'SYSTEM_LEGACY_LIFECYCLE_SOURCE_PASS checks='+$checks+' argv_cases=6 census_cases=5 native_calls=0 registry_changes=0 gpu_calls=0 hardware_admission=0'
