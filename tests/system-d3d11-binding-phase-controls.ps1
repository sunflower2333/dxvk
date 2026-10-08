# SPDX-License-Identifier: MIT
# Parse the owner, then invoke only its two pure D11 routing functions.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
if (Test-Path -LiteralPath $Output) { throw 'Fresh D11 routing output required' }
function Pin([string]$Path) { [ordered]@{path=$Path;bytes=(Get-Item -LiteralPath $Path).Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()} }
$paths=@($Source,$PSCommandPath); $before=@($paths | ForEach-Object { Pin $_ })
$ast=$null
foreach ($path in $paths) {
    $tokens=$null; $errors=$null
    $parsed=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
    if ($errors.Count -ne 0) { throw 'Actual Windows PowerShell AST errors' }
    if ($path -ceq $Source) { $ast=$parsed }
}
$names=@('Require-D11Phase','D11-ValidationMarker')
foreach ($name in $names) {
    $functions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
    if ($functions.Count -ne 1) { throw 'One exact actual-source routing function required' }
    . ([scriptblock]::Create($functions[0].Extent.Text))
}
$rows=[Collections.Generic.List[object]]::new()
foreach ($api in @('9','9ex','10','11')) {
    foreach ($phase in @('offscreen','present','','other','Present')) {
        $expected=$phase -ceq 'offscreen' -or ($api -ceq '11' -and $phase -ceq 'present')
        $passed=$true; try { Require-D11Phase $api $phase } catch { $passed=$false }
        if ($passed -ne $expected) { throw 'Actual-source API/phase guard differs' }
        $rows.Add([ordered]@{kind='guard';api=$api;phase=$phase;accepted=$passed})
    }
}
foreach ($phase in @('offscreen','present')) {
    $passed=$true; try { Require-D11Phase '12' $phase } catch { $passed=$false }
    if ($passed) { throw 'Unsupported API accepted' }
    $rows.Add([ordered]@{kind='guard';api='12';phase=$phase;accepted=$false})
}
$expectedMarkers=[ordered]@{
    offscreen='SYSTEM_D3D11_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=0 software_fallback=0 production_admission=0 registry_changes=0'
    present='SYSTEM_D3D11_PRESENT_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=2 software_fallback=0 production_admission=0 registry_changes=0'
}
foreach ($phase in $expectedMarkers.Keys) {
    $marker=D11-ValidationMarker $phase
    if ($marker -cne $expectedMarkers[$phase]) { throw 'Selected marker differs from independently pinned producer contract' }
    $rows.Add([ordered]@{kind='marker';phase=$phase;marker=$marker})
}
foreach ($phase in @('','other','Present')) {
    $passed=$true; try { D11-ValidationMarker $phase | Out-Null } catch { $passed=$false }
    if ($passed) { throw 'Malformed marker phase accepted' }
    $rows.Add([ordered]@{kind='marker';phase=$phase;accepted=$false})
}
$parameter=@($ast.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -ceq 'D11Phase' })
if ($parameter.Count -ne 1 -or $parameter[0].DefaultValue.Value -cne 'offscreen') { throw 'Offscreen must remain the sole D11 default' }
$after=@($paths | ForEach-Object { Pin $_ })
if (($before | ConvertTo-Json -Compress) -cne ($after | ConvertTo-Json -Compress)) { throw 'Routing source changed' }
$result=[ordered]@{schema=1;verified=$true;actual_ps_ast_files=2;actual_source_functions=$names;cases=$rows.ToArray();phase_cases=$rows.Count;default_phase='offscreen';source_before=$before;source_after=$after;controller_invoked=$false;module_calls=0;registry_calls=0;task_calls=0;kmt_calls=0;gpu_calls=0;hardware_admission=$false;registration=$false}
[IO.File]::WriteAllText($Output,($result | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Output "SYSTEM D11 binding routing controls PASS ast=2 cases=$($rows.Count) default=offscreen module_calls=0 registry_calls=0 hardware_admission=0"
