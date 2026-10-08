# SPDX-License-Identifier: MIT
# Windows PS5.1 parse/compile and pure metadata controls. Never invokes owner.
param([Parameter(Mandatory)][string]$HistoricalOutput,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
if (Test-Path -LiteralPath $Output) { throw 'Fresh metadata preflight output required' }
$root=Split-Path $PSScriptRoot -Parent
$helper=Join-Path $PSScriptRoot 'system-runtime-approved-payload.cs'
$controls=Join-Path $root 'tests\system-runtime-approved-payload-controls.cs'
$controller=Join-Path $PSScriptRoot 'test-system-d3d10-binding.ps1'
$originals=@('inputs\ci-configuration\native-build-configuration.json','inputs\ci-config-0\vulkan_loader_config.h','inputs\ci-config-1\meson-build-options.json','inputs\ci-config-2\meson-build-machines.json','module\dx10-private-icd.json' | ForEach-Object { Join-Path $HistoricalOutput $_ })
$paths=@($helper,$controls,$controller,$PSCommandPath)+$originals
function Pins { @($paths | ForEach-Object { [ordered]@{path=$_;bytes=(Get-Item -LiteralPath $_).Length;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()} }) }
$before=Pins
$tokens=$null; $errors=$null
foreach ($path in @($controller,$PSCommandPath)) {
    [Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors) | Out-Null
    if ($errors.Count -ne 0) { throw 'Actual Windows PowerShell AST errors' }
}
Add-Type -Path @($helper,$controls) -ReferencedAssemblies @('System.dll','System.Core.dll','System.Web.Extensions.dll')
$method=[SystemRuntimeApprovedPayloadControls].GetMethod('Main',[Reflection.BindingFlags]::Static -bor [Reflection.BindingFlags]::Public)
$arguments=[object[]]::new(1); $arguments[0]=[string[]]$originals
if ([int]$method.Invoke($null,$arguments) -ne 0) { throw 'Actual pure payload controls failed' }
$checks=[SystemRuntimeApprovedPayloadControls]::CheckCount
$after=Pins
if (($before | ConvertTo-Json -Compress) -cne ($after | ConvertTo-Json -Compress)) { throw 'Selected sources/originals changed during metadata preflight' }
$result=[ordered]@{schema=1;verified=$true;actual_ps_ast_files=2;pure_metadata_checks=$checks;historical_icd_bytes=220;source_before=$before;source_after=$after;binding_controller_invoked=$false;module_calls=0;registry_calls=0;hardware_admission=$false;registration=$false;current_core_admitted=$false}
[IO.File]::WriteAllText($Output,($result | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Output "SYSTEM approved payload preflight PASS ast=2 checks=$checks module_calls=0 registry_calls=0 hardware_admission=0"
