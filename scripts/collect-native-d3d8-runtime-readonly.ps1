param(
  [Parameter(Mandatory)][string]$Root,
  [Parameter(Mandatory)][string]$ArchiveName
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
if (!$Root.StartsWith('C:\Users\Public\DxvkD3D8Runtime-', [StringComparison]::OrdinalIgnoreCase) -or
    $ArchiveName -notmatch '^[A-Za-z0-9_.-]+\.tar\.gz$') { throw 'Owned paths required' }
$archive = Join-Path 'C:\Users\Public' $ArchiveName
if ((Test-Path $archive) -or (Test-Path ($archive + '.json'))) { throw 'Fresh archive required' }
$result = [IO.File]::ReadAllText((Join-Path $Root 'result.json')) | ConvertFrom-Json
if ($result.schema -cne 'native-system-d3d8-readonly-x86-build-v1' -or $result.status -notin @('PASS','FAIL')) {
  throw 'Completed original build receipt required, including any failure'
}
Copy-Item $PSCommandPath (Join-Path $Root 'executed-collection-helper.ps1')
function Hash([string]$Path) { (Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$files = @(Get-ChildItem $Root -Recurse -File | Sort-Object FullName | ForEach-Object {
  [ordered]@{path=$_.FullName.Substring($Root.Length + 1).Replace('\','/');bytes=$_.Length;sha256=(Hash $_.FullName)}
})
$receipt = [ordered]@{schema='native-system-d3d8-readonly-x86-collection-v1';root=$Root;status=$result.status;
  source_commit=$result.source_commit;files=$files;installation=$false;gpu_runs=0;system_runtime_calls=0;selector_calls=0}
$receipt | ConvertTo-Json -Depth 24 | Set-Content (Join-Path $Root 'collection-original.json') -Encoding UTF8
$list = $archive + '.members.txt'
@($files | ForEach-Object { $_.path }) + @('collection-original.json') | Set-Content $list -Encoding ASCII
& (Join-Path $env:SystemRoot 'System32\tar.exe') -czf $archive -C $Root -T $list
if ($LASTEXITCODE -ne 0) { throw 'Original archive collection failed; preserve outputs' }
$summary = [ordered]@{root=$Root;archive=$archive;bytes=(Get-Item $archive).Length;sha256=(Hash $archive);
  original_files=$files.Count + 1;installation=$false;gpu_runs=0;system_runtime_calls=0;selector_calls=0}
$summary | ConvertTo-Json -Depth 6 | Set-Content ($archive + '.json') -Encoding UTF8
$summary | ConvertTo-Json -Depth 6
