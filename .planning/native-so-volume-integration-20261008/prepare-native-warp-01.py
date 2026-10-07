#!/usr/bin/env python3
"""Freeze current Git source and owned native tests; makes no target calls."""
from pathlib import Path
import ast
import gzip
import hashlib
import io
import json
import subprocess
import tarfile

WS = Path('/home/sunf/droidvm-repos')
REPO = WS / 'reference/codes/dxvk-umd-so-volume-integration-20261008'
DEP = WS / 'dxvk-umd-ci/subprojects/dxbc-spirv'
BASE = WS / 'artifacts/dxvk-native-dx10-dx11-20261007'
OUT = WS / 'artifacts/dxvk-so-volume-integration-20261008/native-warp-d821fc0-01'
TAG = 'd821fc0-01'
COMMIT = 'd821fc0ebdbe2ae30445af703b39fca309ec6cc2'
DEP_COMMIT = '213d2b859e83d91670ada15cc773e2c90c7c8b61'
OWNER_SHA = 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'


def evidence(path):
    data = path.read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def write(name, data):
    path = OUT / name
    with path.open('xb') as f:
        f.write(data.encode() if isinstance(data, str) else data)
    return path


assert git(REPO, 'rev-parse', 'HEAD').decode().strip() == COMMIT
assert not git(REPO, 'diff', '--name-only', COMMIT, '--', 'src', 'tests')
OUT.mkdir(parents=True, exist_ok=False)
prior = json.loads((BASE / 'guest-warp-634d781-01/native-warp-source-634d781-01.json').read_text())
sources = []
objects = {}
for old in prior['sources'] + [dict(path=name, submodule=None) for name in ['tests/umd-stream-output.cpp','src/umd/umd_texture3d.h','src/umd/umd_volume_policy.h','tests/umd-texture3d.cpp','tests/umd-volume-policy.cpp']]:
    path = old['path']
    dependency = old['submodule'] is not None
    git_path = old['git_path'] if dependency else path
    commit = DEP_COMMIT if dependency else COMMIT
    data = git(DEP if dependency else REPO, 'show', commit + ':' + git_path)
    assert path not in objects
    objects[path] = data
    sources.append(dict(path=path, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                        git_commit=commit, git_path=git_path, submodule=old['submodule']))
assert len(sources) == 93
buffer = io.BytesIO()
with gzip.GzipFile(fileobj=buffer, mode='wb', mtime=0, filename='') as zipped:
    with tarfile.open(fileobj=zipped, mode='w') as archive:
        for path, data in sorted(objects.items()):
            member = tarfile.TarInfo(path)
            member.size = len(data)
            member.mode = 0o644
            archive.addfile(member, io.BytesIO(data))
archive = write('native-warp-source-' + TAG + '.tar.gz', buffer.getvalue())
fixtures = [
    dict(name='stream-output', groups=['shader', 'parser', 'ddi'],
         extra=['/DVIOGPU_STREAM_OUTPUT_WARP', '/wd4100'],
         pattern=r'native stream output PASS checks=(\d+)', required_markers=[
             'STREAM_OUTPUT_BACKEND WARP fixture; actual native DDI, independent rasterizer',
             'STREAM_OUTPUT null-GS PASS draws=3 words=72 raw_uint=1 prior_stage_rebind=1 missing_output_rejected=1']),
    dict(name='d3d11-device', groups=['shader', 'parser', 'ddi'], extra=[],
         pattern=r'typed D3D10.1/D3D11 fixture PASS checks=(\d+)', required_markers=[
             'SM5 null-GS stream output PASS draws=3 words=72 raw_uint=1 public_control=1 missing_output_rejected=1',
             'SM5 null-GS domain stream output PASS public_control=1 words=1024'])]
fixtures += [
    dict(name='volume-policy', groups=[], extra=[], pattern=r'PASS volume policy: (\d+) checks', expected_checks=385547,
         required_markers=['BACKEND=none; GPU_ACCEPTANCE=NOT_RUN']),
    dict(name='texture3d', groups=['shader','parser','ddi'], extra=[],
         pattern=r'PASS Texture3D\r?\nprofiles=3\r?\ncases=27\r?\nchecks=(\d+)\r?\nvoxels=9138\r?\nsampled=945',
         required_markers=['BACKEND=WARP', 'production_volume_DDI=true'])]
manifest = dict(prior, source_commit=COMMIT, base_commit='d7e5c7d46b8ce889e993bfab66a3b78b076c49d1',
                archive_sha256=evidence(archive)['sha256'], input_count=93,
                adapter_units=[], sources=sources, fixtures=fixtures)
manifest_path = write('native-warp-source-' + TAG + '.json', json.dumps(manifest, indent=2) + '\n')
template = BASE / 'guest-warp-634d781-01/build-native-warp-634d781-01.ps1'
build = template.read_text().replace('634d781-01', TAG).replace('634D781_01', 'D821FC0_01')
build = build.replace(prior['source_commit'], COMMIT).replace(prior['archive_sha256'], evidence(archive)['sha256'])
build = build.replace("$ExpectedInputCount='88'", "$ExpectedInputCount='93'")
build = build.replace('a95ecbca2491a4a1380a8ea0a01cc293f57e933b90596602ab0d2b19495c9f29', evidence(manifest_path)['sha256'])
build = build.replace("source_base_commit='f4bf37f435540ef9086c99cb2f352fe1972058c4'", "source_base_commit='d7e5c7d46b8ce889e993bfab66a3b78b076c49d1'")
build = build.replace(",\n        [ordered]@{name='adapter';units=@('src\\umd\\umd_adapter.cpp','src\\umd\\umd_contract.cpp')}", '')
start = build.index('    $definitions=@(')
end = build.index('    foreach ($fixture in $definitions)', start)
definitions = '    $definitions=@(\n'
for f in fixtures:
    extra = ','.join("'" + x + "'" for x in f['extra'])
    markers = ','.join("'" + x + "'" for x in f['required_markers'])
    groups = ','.join("'" + x + "'" for x in f['groups'])
    expected = ";expected_checks=" + str(f['expected_checks']) if 'expected_checks' in f else ''
    definitions += "        [ordered]@{name='" + f['name'] + "';groups=@(" + groups + " );extra=@(" + extra + ");pattern='" + f['pattern'] + "'" + expected + ";required_markers=@(" + markers + ")},\n"
definitions = definitions.rstrip(',\n') + '\n    )\n'
build = build[:start] + definitions + build[end:]
build = build.replace("    & tar.exe -xzf $Archive -C $sourceRoot\n    if ($LASTEXITCODE) { throw 'Frozen caps source extraction failed' }", "    $extract=Invoke-Logged 'extract-source' (Join-Path ([Environment]::SystemDirectory) 'tar.exe') ('-xzf \"'+$Archive+'\" -C \"'+$sourceRoot+'\"') $out 60000\n    if ($extract.exit_code -ne 0) { throw 'Frozen source extraction failed' }")
# The original spelling also appears in older accepted helpers.
build = build.replace("    & tar.exe -xzf $Archive -C $sourceRoot\n    if ($LASTEXITCODE) { throw 'Frozen caps source extraction failed' }", '')
assert '& tar.exe' not in build
build = build.replace("        $fixtureChecks=[uint32]$Matches[1]", "        $fixtureChecks=[uint32]$Matches[1]\n        foreach ($marker in $fixture.required_markers) {\n            if (!$text.Contains($marker)) {throw ('Missing actual null-GS coverage marker: '+$marker)}\n        }")
build = build.replace("$exe '' $directory 60000", "$exe '' $directory 30000")
build = build.replace("checks=$fixtureChecks;pe=$pe", "checks=$fixtureChecks;required_markers=$fixture.required_markers;pe=$pe")
write('build-native-warp-' + TAG + '.ps1', build)
owner = BASE / 'guest-warp-f4bf37f-02/owned-raw-process-f4bf37f-02.cs'
assert evidence(owner)['sha256'] == OWNER_SHA
write(owner.name, owner.read_bytes())
for name in ['native-warp-sdk-headers-f4bf37f-01.json', 'native-warp-libraries-f4bf37f-01.json']:
    write(name, (BASE / 'guest-warp-f4bf37f-01' / name).read_bytes())
collector = r'''$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$Root='C:\Users\Public\DxvkNativeWarp-d821fc0-01'
$Prefix='C:\Users\Public\native-warp-d821fc0-01-evidence'
$RunnerSource=Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
if ((Get-FileHash -LiteralPath $RunnerSource -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') {throw 'Original raw process runner changed'}
if (!(Test-Path -LiteralPath $Root -PathType Container)) {throw 'Require actual build root, successful or failed'}
foreach($suffix in @('.tar.gz','.stdout.raw','.stderr.raw','.process-result.json','.members.json')) {
    if(Test-Path -LiteralPath ($Prefix+$suffix)){throw 'Refusing previous collector output'}
}
Add-Type -Path $RunnerSource
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $Root 'helpers\collect-helper-original.ps1')
$members=@(Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($Root.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$members | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($Prefix+'.members.json') -Encoding UTF8
$run=[DxvkRawProcessF4_02]::Run((Join-Path ([Environment]::SystemDirectory) 'tar.exe'),('-czf "'+$Prefix+'.tar.gz" -C "'+(Split-Path $Root -Parent)+'" "'+(Split-Path $Root -Leaf)+'"'),(Split-Path $Root -Parent),($Prefix+'.stdout.raw'),($Prefix+'.stderr.raw'),60000)
$row=[ordered]@{pid=$run.Pid;retained_process_handle=$run.ProcessHandle;exited=$run.Exited;exit_code_available=$run.ExitCodeAvailable;exit_code=$run.ExitCode;seconds=$run.Seconds;deadline_ms=60000;timed_out=$run.TimedOut;child_still_running=$run.ChildStillRunning;pipes_drained=$run.PipesDrained;stdout_bytes=$run.StdoutBytes;stderr_bytes=$run.StderrBytes;failure=$run.Failure}
$row | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($Prefix+'.process-result.json') -Encoding UTF8
if($run.Failure -or $run.TimedOut -or !$run.Exited -or !$run.ExitCodeAvailable -or !$run.PipesDrained -or $run.ProcessHandle -eq 0 -or $run.ExitCode -ne 0){throw 'Original owned evidence collector failed; sidecars preserved'}
$after=@(Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($Root.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
if((ConvertTo-Json @($members) -Depth 5 -Compress) -cne (ConvertTo-Json @($after) -Depth 5 -Compress)){throw 'Original evidence changed during collection'}
$files=@(foreach($suffix in @('.tar.gz','.stdout.raw','.stderr.raw','.process-result.json','.members.json')) {
    $f=Get-Item -LiteralPath ($Prefix+$suffix)
    [ordered]@{name=$f.Name;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
[ordered]@{passed=$true;files=$files;included_files=$members.Count;originals_unchanged=$true;process=$row} | ConvertTo-Json -Depth 7
'''
write('collect-native-warp-' + TAG + '.ps1', collector)
scripts = ['build-native-warp-' + TAG + '.ps1', 'collect-native-warp-' + TAG + '.ps1']
rows = '\n'.join("    [ordered]@{name='" + n + "';sha256='" + evidence(OUT / n)['sha256'] + "'}," for n in scripts).rstrip(',')
parser = "$ErrorActionPreference='Stop'\nSet-StrictMode -Version Latest\n$rows=@(\n" + rows + r'''
)
if([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'Arm64' -or [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -ne 'Arm64' -or $PSVersionTable.PSVersion.Major -ne 5 -or $PSVersionTable.PSVersion.Minor -ne 1){throw 'Require actual native ARM64 PowerShell5.1'}
foreach($row in $rows){
    $path=Join-Path 'C:\Users\Public' $row.name
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.sha256){throw 'Frozen helper hash mismatch'}
    $tokens=$null;$errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
    if($errors.Count){$errors | Format-List | Out-String | Write-Output;throw 'Native PowerShell parser rejected helper'}
}
$owner=Join-Path $PSScriptRoot 'owned-raw-process-f4bf37f-02.cs'
if((Get-FileHash -LiteralPath $owner -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad'){throw 'Frozen process owner mismatch'}
Add-Type -Path $owner
[ordered]@{passed=$true;scripts=$rows.Count;parse_errors=0;process_owner_compiled=$true;os_architecture=[Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString();process_architecture=[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString();powershell=$PSVersionTable.PSVersion.ToString()} | ConvertTo-Json
'''
write('check-native-ps51-packet-' + TAG + '.ps1', parser)
pins = [evidence(p) for p in sorted(OUT.iterdir())]
prepared = dict(source_commit=COMMIT, input_count=93, fixtures=fixtures, files=pins,
                template=evidence(template), target_execution=False,
                actual_native_parse='pending', hardware_acceptance=False,
                deadlines_ms=dict(compile=60000, fixture=30000, collect=60000),
                warning_exception='stream-output WARP fixture only /wd4100 for inherited unused argv; production /W4 /WX unchanged')
write('prepared-native-null-gs-01.json', json.dumps(prepared, indent=2) + '\n')
print(json.dumps(dict(passed=True, packet=str(OUT), files=len(pins), sources=93,
                      archive=evidence(archive), prepared=evidence(OUT / 'prepared-native-null-gs-01.json'))))
