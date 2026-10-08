param(
  [Parameter(Mandatory)][string]$Root,
  [Parameter(Mandatory)][string]$ArchiveName,
  [string]$RunnerSource = (Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs')
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
if (!$Root.StartsWith('C:\Users\Public\DxvkD3D8Runtime-', [StringComparison]::OrdinalIgnoreCase) -or
    $ArchiveName -notmatch '^[A-Za-z0-9_.-]+\.tar\.gz$') { throw 'Owned paths required' }
$archive = Join-Path 'C:\Users\Public' $ArchiveName
if ((Test-Path $archive) -or (Test-Path ($archive + '.json'))) { throw 'Fresh archive required' }
$result = [IO.File]::ReadAllText((Join-Path $Root 'result.json')) | ConvertFrom-Json
if ($result.schema -cne 'native-ordinary-d3d8-probe-x86-build-v1' -or $result.status -notin @('PASS','FAIL')) {
  throw 'Completed original build receipt required, including any failure'
}
function Hash([string]$Path) { (Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$manifest = [IO.File]::ReadAllText((Join-Path $Root 'source-manifest.json')) | ConvertFrom-Json
if ((Hash $PSCommandPath) -cne $manifest.collector_helper_sha256) { throw 'Frozen collector differs' }
$runner = $RunnerSource
if ((Hash $runner) -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') {
  throw 'Original bounded runner differs'
}
Add-Type -Path $runner
Copy-Item $PSCommandPath (Join-Path $Root 'executed-collection-helper.ps1')
Copy-Item $runner (Join-Path $Root 'executed-collection-raw-source.cs')
$files = @(Get-ChildItem $Root -Recurse -File | Sort-Object FullName | ForEach-Object {
  [ordered]@{path=$_.FullName.Substring($Root.Length + 1).Replace('\','/');bytes=$_.Length;sha256=(Hash $_.FullName)}
})
$receipt = [ordered]@{schema='native-ordinary-d3d8-probe-x86-collection-v1';root=$Root;status=$result.status;
  source_commit=$result.source_commit;files=$files;installation=$false;gpu_runs=0;system_runtime_calls=0;selector_calls=0}
$receipt | ConvertTo-Json -Depth 24 | Set-Content (Join-Path $Root 'collection-original.json') -Encoding UTF8
$list = $archive + '.members.txt'
@($files | ForEach-Object { $_.path }) + @('collection-original.json') | Set-Content $list -Encoding ASCII
$exe = Join-Path $env:SystemRoot 'System32\tar.exe'
$arguments = '-czf "' + $archive + '" -C "' + $Root + '" -T "' + $list + '"'
$stdout = $archive + '.collection.stdout.txt'; $stderr = $archive + '.collection.stderr.txt'
$run = [DxvkRawProcessF4_02]::Run($exe, $arguments, $Root, $stdout, $stderr, 60000)
$process = [ordered]@{exe=$exe;arguments=$arguments;cwd=$Root;deadline_ms=60000;
  runner_sha256=(Hash $runner);pid=$run.Pid;start_utc=$run.StartUtc;retained_process_handle=$run.ProcessHandle;
  exited=$run.Exited;exit_code_available=$run.ExitCodeAvailable;exit=$null;timeout=$run.TimedOut;
  child_still_running=$run.ChildStillRunning;pipes_drained=$run.PipesDrained;seconds=$run.Seconds;
  stdout=$stdout;stderr=$stderr;stdout_bytes=$run.StdoutBytes;stderr_bytes=$run.StderrBytes;capture_failure=$run.Failure}
if ($run.ExitCodeAvailable) { $process.exit = [int]$run.ExitCode }
$process | ConvertTo-Json -Depth 12 | Set-Content ($archive + '.collection-process.json') -Encoding UTF8
if ($run.Failure -or $run.TimedOut -or !$run.Exited -or !$run.ExitCodeAvailable -or
    !$run.PipesDrained -or $run.ChildStillRunning -or $run.ProcessHandle -eq 0 -or $process.exit -ne 0) {
  throw 'Original archive collection failed; preserve actual owned process and raw outputs'
}
$summary = [ordered]@{root=$Root;archive=$archive;bytes=(Get-Item $archive).Length;sha256=(Hash $archive);
  original_files=$files.Count + 1;collection_process=($archive + '.collection-process.json');
  collection_process_sha256=(Hash ($archive + '.collection-process.json'));
  collection_stdout_sha256=(Hash $stdout);collection_stderr_sha256=(Hash $stderr);
  installation=$false;gpu_runs=0;system_runtime_calls=0;selector_calls=0}
$summary | ConvertTo-Json -Depth 6 | Set-Content ($archive + '.json') -Encoding UTF8
$summary | ConvertTo-Json -Depth 6
