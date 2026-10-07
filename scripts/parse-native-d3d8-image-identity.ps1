param(
  [Parameter(Mandatory)][string]$Inputs,
  [Parameter(Mandatory)][string]$Receipt,
  [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
  [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ManifestSHA256
)
# Supplemental native parser only. The selected original frozen build packet is
# byte-identical; this helper does not launch an executable or require a core.
$ErrorActionPreference = 'Stop'
$Inputs = [IO.Path]::GetFullPath($Inputs)
$Receipt = [IO.Path]::GetFullPath($Receipt)
if (!$Inputs.StartsWith('C:\Users\Public\DxvkD3D8RuntimeInputs-', [StringComparison]::OrdinalIgnoreCase) -or
    !$Receipt.StartsWith($Inputs + '\', [StringComparison]::OrdinalIgnoreCase) -or (Test-Path $Receipt)) {
  throw 'Fresh receipt in the owned input directory required'
}
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$manifestPath = Join-Path $Inputs 'native-system-d3d8-device-x86-source-01.json'
$manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
if ((Hash $manifestPath) -cne $ManifestSHA256 -or
    $manifest.source_commit -cne $SourceCommit -or $manifest.inputs.Count -ne 15 -or $manifest.schema -cne 'native-system-d3d8-image-identity-x86-v1' -or
    $manifest.raw_process_helper_sha256 -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') {
  throw 'Exact selected frozen manifest required'
}
$expected = [ordered]@{
  'native-system-d3d8-device-x86-source-01.tar.gz'=$manifest.archive_sha256
  'native-system-d3d8-device-x86-source-01.json'=$ManifestSHA256
  'build-native-d3d8-image-identity.ps1'=$manifest.build_helper_sha256
  'collect-native-d3d8-runtime-device.ps1'=$manifest.collector_helper_sha256
  'owned-raw-process-f4bf37f-02.cs'=$manifest.raw_process_helper_sha256
}
$parserRows = @($manifest.inputs | Where-Object { $_.path -ceq 'scripts/parse-native-d3d8-image-identity.ps1' })
if ($parserRows.Count -ne 1 -or (Hash $PSCommandPath) -cne $parserRows[0].sha256) {
  throw 'Frozen parser helper differs'
}
$expected['parse-native-d3d8-image-identity.ps1'] = $parserRows[0].sha256
$before = @($expected.Keys | ForEach-Object {
  $path = Join-Path $Inputs $_
  $digest = Hash $path
  if ($digest -cne $expected[$_]) { throw "Frozen input differs: $_" }
  [ordered]@{name=$_;bytes=(Get-Item -LiteralPath $path).Length;sha256=$digest}
})
$result = [ordered]@{schema='native-d3d8-image-identity-parser-v1';status='FAIL';source_commit=$manifest.source_commit;
  inputs_before=$before;script_parse=@();core_required=$false;core_loaded=$false;
  executable_launches=0;system_runtime_calls=0;selector_calls=0;gpu_runs=0;installation=$false}
try {
  foreach ($name in @('build-native-d3d8-image-identity.ps1','collect-native-d3d8-runtime-device.ps1')) {
    $tokens=$null; $errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $Inputs $name), [ref]$tokens, [ref]$errors)
    $row=[ordered]@{name=$name;sha256=(Hash (Join-Path $Inputs $name));parse_errors=$errors.Count;
      errors=@($errors | ForEach-Object { $_.Message })}
    $result.script_parse += $row
    if ($errors.Count) { throw "Native parse failed: $name" }
  }
  $runner = Join-Path $Inputs 'owned-raw-process-f4bf37f-02.cs'
  Add-Type -TypeDefinition ([IO.File]::ReadAllText($runner)) -Language CSharp
  $result.raw_process_source_sha256=Hash $runner
  $result.raw_process_type_compiled=$true
  $after = @($before | ForEach-Object {
    $path = Join-Path $Inputs $_.name
    if ((Hash $path) -cne $_.sha256 -or (Get-Item -LiteralPath $path).Length -ne $_.bytes) {
      throw "Parser input changed: $($_.name)"
    }
    [ordered]@{name=$_.name;bytes=$_.bytes;sha256=$_.sha256}
  })
  $result.inputs_after=$after
  $result.status='PASS'
} catch {
  $result.error=$_.Exception.Message
} finally {
  $result.parser_helper=[ordered]@{path=$PSCommandPath;sha256=(Hash $PSCommandPath)}
  $result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Receipt -Encoding UTF8
}
$result | ConvertTo-Json -Depth 12
if ($result.status -cne 'PASS') { exit 1 }
