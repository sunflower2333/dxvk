# SPDX-License-Identifier: MIT
# Execute actual source settlement functions with an in-memory registry double.
# No controller roles, driver registration, KMT, task, device or GPU operation.
param([Parameter(Mandatory)][string]$Source,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
if (Test-Path -LiteralPath $Output) { throw 'Fresh phase-control output required' }
$before=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($Source,[ref]$tokens,[ref]$errors)
if ($errors.Count -ne 0) { throw 'Actual controller AST failed' }
$names=@('Hash','Write-Json','Write-MutationIntent','Raw-Rows','Restore-Tuple')
foreach ($name in $names) {
    $functions=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name},$true))
    if ($functions.Count -ne 1) { throw 'Exact production function required' }
    Invoke-Expression $functions[0].Extent.Text
}
Add-Type -TypeDefinition @'
using System;
public sealed class DxvkBindingRawValue01 {
    public uint View; public string Name; public bool Exists; public uint Type; public byte[] Data;
}
public static class DxvkBindingNative01 {
    public static DxvkBindingRawValue01[] Current;
    public static int RestoreCalls;
    public static DxvkBindingRawValue01[] Snapshot(string unused) { return Current; }
    public static void Restore(string unused,DxvkBindingRawValue01[] rows) { ++RestoreCalls; Current=rows; }
    public static void RequireSnapshot(DxvkBindingRawValue01[] expected,DxvkBindingRawValue01[] actual) {
        if (expected.Length!=6 || actual.Length!=6) throw new InvalidOperationException();
        for (int i=0;i<6;++i) {
            var a=expected[i]; var b=actual[i];
            if (a.View!=b.View || a.Name!=b.Name || a.Exists!=b.Exists || a.Type!=b.Type || a.Data.Length!=b.Data.Length) throw new InvalidOperationException();
            for (int j=0;j<a.Data.Length;++j) if (a.Data[j]!=b.Data[j]) throw new InvalidOperationException();
        }
    }
}
'@
$checks=0; $rows=[Collections.Generic.List[object]]::new()
function Check([bool]$Passed) { $script:checks++; if (!$Passed) { throw "Actual-source phase control $script:checks failed" } }
$text=$ast.Extent.Text
$lease=$text.IndexOf('$leaseHeld=$lease.WaitOne(0)')
$exclusion=$text.IndexOf("'A previous restoration task must finish or be independently reviewed first'")
$backup=$text.IndexOf('$original=[DxvkBindingNative01]::Snapshot($subkey)')
$config=$text.IndexOf('$configFile=Join-Path $control')
$recheck=$text.IndexOf('[DxvkBindingNative01]::RequireSnapshot($original,[DxvkBindingNative01]::Snapshot($subkey))')
$intent=$text.IndexOf("Write-MutationIntent (Join-Path `$control 'mutation-intent.json')")
$write=$text.IndexOf('[DxvkBindingNative01]::Write($subkey,$replacement)')
Check ($lease -ge 0 -and $lease -lt $exclusion -and $exclusion -lt $backup -and $backup -lt $config)
Check ($recheck -ge 0 -and $recheck -lt $intent -and $intent -lt $write)
Check ($text.Contains('} finally { if ($leaseHeld) { $lease.ReleaseMutex() }; $lease.Dispose() }'))
function Original([byte]$Sentinel) {
    @(0..5 | ForEach-Object { $value=[DxvkBindingRawValue01]::new(); $value.View=256; $value.Name='value-'+$_; $value.Exists=$true; $value.Type=7; $value.Data=[byte[]]@($Sentinel,$_); $value })
}
$work=$Output+'.cases'; New-Item -ItemType Directory -Path $work | Out-Null
foreach ($nativeSlot in @(0,1,2)) {
foreach ($case in @('unused-stale-backup','unused-matching-backup','partial-write','intent-before-write','foreign-intent','foreign-slot')) {
    $control=Join-Path $work ('slot'+$nativeSlot+'-'+$case); New-Item -ItemType Directory -Path $control | Out-Null
    $original=Original 11
    [DxvkBindingNative01]::Current=Original 22
    [DxvkBindingNative01]::RestoreCalls=0
    $value=[pscustomobject]@{control=$control;original=$original;owner_pid=$PID;registry_subkey='MEMORY-ONLY';native_slot=$nativeSlot;mutex=('Local\VioGpuBindingPhaseControl-'+[Guid]::NewGuid().ToString('N'))}
    if ($case -in @('unused-matching-backup','intent-before-write','foreign-intent','foreign-slot')) { [DxvkBindingNative01]::Current=Original 11 }
    if ($case -in @('partial-write','intent-before-write','foreign-intent','foreign-slot')) {
        $owner=$PID; if ($case -eq 'foreign-intent') { $owner++ }
        $intentSlot=$nativeSlot; if ($case -eq 'foreign-slot') { $intentSlot=($nativeSlot+1)%3 }
        Write-MutationIntent (Join-Path $control 'mutation-intent.json') ([ordered]@{schema=2;owner_pid=$owner;registry_subkey='MEMORY-ONLY';selected_native_slot=$intentSlot;only_native_tuple_slot=$true})
    }
    if ($case -in @('foreign-intent','foreign-slot')) {
        $rejected=$false; try { Restore-Tuple $value 'control' | Out-Null } catch { $rejected=$true }
        Check $rejected; Check ([DxvkBindingNative01]::RestoreCalls -eq 0)
        Check (!(Test-Path -LiteralPath (Join-Path $control 'release.json')))
        $rows.Add([ordered]@{native_slot=$nativeSlot;case=$case;rejected=$rejected;backup_replayed=$false}); continue
    }
    $settled=Restore-Tuple $value 'control'
    $proof=Get-Content -LiteralPath (Join-Path $control 'restored.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $permission=Get-Content -LiteralPath (Join-Path $control 'release.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    Check ($permission.restored_sha256 -ceq (Hash (Join-Path $control 'restored.json')))
    if ($case -like 'unused-*') {
        Check ([DxvkBindingNative01]::RestoreCalls -eq 0)
        Check ($settled.cancelled_without_registry_write -and !$settled.backup_replayed -and !$settled.raw_tuple_restored)
        Check ($proof.cancelled_without_registry_write -and !$permission.raw_tuple_restored)
        Check ($settled.original_readback_matches -eq ($case -eq 'unused-matching-backup'))
        Check ([DxvkBindingNative01]::Current[0].Data[0] -eq $(if ($case -eq 'unused-stale-backup') {22} else {11}))
    } else {
        Check ([DxvkBindingNative01]::RestoreCalls -eq 1)
        Check ($settled.backup_replayed -and $settled.raw_tuple_restored -and !$settled.cancelled_without_registry_write)
        Check ($proof.raw_tuple_restored -and $permission.raw_tuple_restored)
        [DxvkBindingNative01]::RequireSnapshot($original,[DxvkBindingNative01]::Current)
        Check ($settled.original_readback_matches)
    }
    $rows.Add([ordered]@{native_slot=$nativeSlot;case=$case;settlement=$settled;release=$permission})
}
}
$after=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant(); Check ($before -ceq $after)
$result=[ordered]@{schema=1;verified=$true;checks=$checks;scenarios=$rows.Count;source_before=$before;source_after=$after;actual_source_functions=$names;cases=$rows;registry_double='In-memory only';controller_invoked=$false;windows_registry_calls=0;kmt_calls=0;gpu_calls=0;hardware_admission=$false;registration=$false}
[IO.File]::WriteAllText($Output,($result | ConvertTo-Json -Depth 16),[Text.UTF8Encoding]::new($false))
Write-Output "SYSTEM binding actual-source phase controls PASS checks=$checks scenarios=$($rows.Count) native_slots=3 windows_registry=0 controller_invoked=0 hardware_admission=0"
