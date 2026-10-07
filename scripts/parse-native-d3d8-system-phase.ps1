param([Parameter(Mandatory)][string]$Manifest,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
if (Test-Path -LiteralPath $Output) { throw 'Use fresh native syntax evidence' }
$pins=[IO.File]::ReadAllText($Manifest)|ConvertFrom-Json
$rows=@()
foreach ($pin in @($pins.helpers | Where-Object { $_.name.EndsWith('.ps1') })) {
  $path=Join-Path $PSScriptRoot $pin.name
  $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($hash -cne $pin.sha256) { throw 'Reviewed original helper changed' }
  $tokens=$null;$errors=$null
  $null=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
  $rows+=@([ordered]@{name=$pin.name;sha256=$hash;parse_errors=@($errors | ForEach-Object { $_.ToString() })})
}
$raw=Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
$rawHash=(Get-FileHash -LiteralPath $raw -Algorithm SHA256).Hash.ToLowerInvariant()
if ($rawHash -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') { throw 'Raw owner source changed' }
Add-Type -Path $raw
[ordered]@{schema='system-d3d8-phase-native-parse-v1';scripts=$rows;raw_sha256=$rawHash;raw_type_compiled=$true;
  fixture_executions=0;KMT_queries=0;module_loads=0;runtime_factories=0;GPU_runs=0} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Output -Encoding UTF8
if (@($rows | Where-Object { $_.parse_errors.Count }).Count) { throw 'Original native syntax failure retained' }
'SYSTEM_D3D8_PHASE_NATIVE_PARSE_PASS scripts='+$rows.Count+'; runtime_factories=0'
