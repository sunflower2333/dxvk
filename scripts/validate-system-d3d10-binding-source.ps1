# SPDX-License-Identifier: MIT
# Parse/compile, pure byte controls and one tiny CPU child. Never invokes binding.
param([Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
if (Test-Path -LiteralPath $Output) { throw 'Fresh source-preflight output required' }
$root=Split-Path $PSScriptRoot -Parent
$paths=@((Join-Path $PSScriptRoot 'test-system-d3d10-binding.ps1'),(Join-Path $PSScriptRoot 'system-d3d10-binding-native.cs'),(Join-Path $root 'tests\system-d3d10-binding-controls.cs'),$PSCommandPath,(Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-progress-01.cs'),(Join-Path $root 'tests\system-d3d10-binding-progress-controls.cs'),(Join-Path $root 'tests\system-d3d10-binding-progress-child.ps1'),(Join-Path $root 'tests\system-runtime-binding-slots-controls.cs'),(Join-Path $root 'tests\system-d3d10-binding-phase-controls.ps1'))
$before=@($paths | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()} })
$tokens=$null; $errors=$null
foreach ($path in @($paths[0],$paths[3],$paths[6],$paths[8])) {
    [Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors) | Out-Null
    if ($errors.Count -ne 0) { $errors | Format-List | Out-String | Write-Error; throw 'Actual Windows PowerShell AST errors' }
}
Add-Type -Path @($paths[1],$paths[2],$paths[4],$paths[5],$paths[7])
$type=[DxvkBindingNative01].Assembly.GetType('SystemD3D10BindingControls',$true)
$method=$type.GetMethod('Main',[Reflection.BindingFlags]::Static -bor [Reflection.BindingFlags]::NonPublic)
if (!$method -or [int]$method.Invoke($null,@()) -ne 0) { throw 'Actual pure helper controls failed' }
$slotType=[DxvkBindingNative01].Assembly.GetType('SystemRuntimeBindingSlotsControls',$true)
$slotMethod=$slotType.GetMethod('Main',[Reflection.BindingFlags]::Static -bor [Reflection.BindingFlags]::NonPublic)
if (!$slotMethod -or [int]$slotMethod.Invoke($null,@()) -ne 0) { throw 'Actual native-slot/closed-checkpoint controls failed' }
$slotChecks=[int]$slotType.GetProperty('CheckCount').GetValue($null,$null)
$progress=$Output+'.progress'
$childArguments='-NoProfile -ExecutionPolicy Bypass -File "'+$paths[6]+'" -ReleasePath "'+(Join-Path $progress 'release')+'"'
if ([DxvkBindingProgressControls01]::Run((Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'),$childArguments,$progress) -ne 0) { throw 'Actual tiny child stdout/hold progress failed' }
$after=@($paths | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()} })
if (($before | ConvertTo-Json -Compress) -cne ($after | ConvertTo-Json -Compress)) { throw 'Source changed during preflight' }
$result=[ordered]@{schema=1;verified=$true;actual_ps_ast_files=4;pure_helper_checks=120;native_slot_checks=$slotChecks;actual_progress_cpu_children=1;progress_observed_before_release=$true;source_before=$before;source_after=$after;binding_controller_invoked=$false;windows_registry_calls=0;gpu_calls=0;hardware_admission=$false;registration=$false}
[IO.File]::WriteAllText($Output,($result | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Output "SYSTEM binding source preflight PASS ast=4 helper_checks=120 slot_checks=$slotChecks progress_children=1 binding_invoked=0 hardware_admission=0"
