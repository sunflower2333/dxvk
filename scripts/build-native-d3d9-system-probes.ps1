# Build separate real ARM64 and x64 executables from the frozen typed probe.
# This producer does not load the frontend/core or execute either view probe.
param([Parameter(Mandatory)][string]$Packet,
 [Parameter(Mandatory)][string]$ToolchainManifestPath,
 [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$ToolchainManifestSha256,
 [Parameter(Mandatory)][string]$OutputDirectory,
 [Parameter(Mandatory)][string]$AdditionalIncludeDirectory)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
function Require([bool]$Value,[string]$Message){if(!$Value){throw $Message}}
function Evidence([string]$Path){$f=Get-Item -LiteralPath $Path;[ordered]@{path=$f.FullName;bytes=$f.Length;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}}
function Pinned($Expected){$actual=Evidence $Expected.path;Require ($actual.bytes -eq $Expected.bytes -and $actual.sha256 -ceq $Expected.sha256) ('Original input differs: '+$Expected.path);return $actual}
function Machine([string]$Path,[bool]$PE){
 $stream=[IO.File]::OpenRead($Path);$reader=New-Object IO.BinaryReader($stream)
 try{
  if($PE){Require ($reader.ReadUInt16() -eq 0x5a4d) 'Actual PE DOS signature missing';$stream.Position=0x3c;$offset=$reader.ReadUInt32();Require ($offset -le $stream.Length-6) 'PE header bounds';$stream.Position=$offset;Require ($reader.ReadUInt32() -eq 0x4550) 'Actual PE signature missing'}
  return $reader.ReadUInt16()
 }finally{$reader.Dispose()}
}
Require ($PSVersionTable.PSVersion.Major -eq 5 -and $PSVersionTable.PSVersion.Minor -eq 1 -and
 [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture -eq 'Arm64') 'Actual native ARM64 PS5.1 producer required'
Require ((Evidence $ToolchainManifestPath).sha256 -ceq $ToolchainManifestSha256) 'Original view toolchain manifest changed'
$toolchainInfo=[IO.File]::ReadAllText($ToolchainManifestPath)|ConvertFrom-Json
Require ($toolchainInfo.schema -ceq 'native-modern-front-views-toolchain-v1' -and $toolchainInfo.ready -is [bool] -and
 $toolchainInfo.ready -and !$toolchainInfo.native_originals_pending -and $toolchainInfo.toolset -ceq '14.50.35717') 'Reviewed actual matching14.50 view toolchains required'
$views=[object[]]$toolchainInfo.views
Require ($views.Count -eq 2 -and $views[0].architecture -ceq 'arm64' -and $views[1].architecture -ceq 'x64') 'Actual ARM64 then x64 view profiles required'
$sourceManifest=Join-Path $Packet 'source-inputs.json'
$sourceInfo=[IO.File]::ReadAllText($sourceManifest)|ConvertFrom-Json
Require ($sourceInfo.schema -ceq 'native-d3d9-system-source-input-v1') 'Original view source manifest required'
Require ($sourceInfo.source_commit -cmatch '^[0-9a-f]{40}$') 'Exact frozen Git source commit required'
$requiredSource=@('tests/umd-d3d9-system-validation.cpp','tests/verify-d3d9-system-originals.py',
 'scripts/build-native-d3d9-system-probes.ps1','scripts/d3d9-system-sdk-prelude.h',
 'docs/native-system-d3d9-validation-20261008.md','src/umd/umd_runtime_identity.h','src/umd/umd_identity.h',
 'scripts/owned-raw-process-f4bf37f-02.cs')
Require (@($sourceInfo.files).Count -eq $requiredSource.Count) 'Exact ordinary probe source closure required'
foreach($relative in $requiredSource){Require (@($sourceInfo.files|Where-Object {$_.path -ceq $relative}).Count -eq 1) ('Missing or repeated source input: '+$relative)}
$probe=Join-Path $Packet 'source/tests/umd-d3d9-system-validation.cpp'
$runner=Join-Path $Packet 'source/scripts/owned-raw-process-f4bf37f-02.cs'
$prelude=Join-Path $Packet 'source/scripts/d3d9-system-sdk-prelude.h'
$allSourcePins=@()
foreach($inputRow in $sourceInfo.files){
 Require ($inputRow.path -notmatch '(^|[\/])\.\.([\/]|$)' -and ![IO.Path]::IsPathRooted($inputRow.path)) 'Bounded relative source input required'
 $path=Join-Path (Join-Path $Packet 'source') $inputRow.path
 $actual=Evidence $path
 Require ($actual.bytes -eq $inputRow.bytes -and $actual.sha256 -ceq $inputRow.sha256) ('Frozen source input differs: '+$inputRow.path)
 $allSourcePins+=@($actual)
}
$probeHash=(Evidence $probe).sha256;$runnerHash=(Evidence $runner).sha256
$probePins=@($sourceInfo.files|Where-Object {$_.path -ceq 'tests/umd-d3d9-system-validation.cpp'})
Require ($probePins.Count -eq 1 -and $probeHash -ceq $probePins[0].sha256 -and (Evidence $probe).bytes -eq $probePins[0].bytes) 'Exact frozen ordinary typed probe required'
Require ($runnerHash -ceq 'd8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad') 'Original owned child runner required'
Require (!(Test-Path -LiteralPath $OutputDirectory)) 'Fresh view producer output required'
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
Copy-Item -LiteralPath $ToolchainManifestPath -Destination (Join-Path $OutputDirectory 'toolchain-inputs-original.json')
Copy-Item -LiteralPath $sourceManifest -Destination (Join-Path $OutputDirectory 'view-source-inputs-original.json')
Copy-Item -LiteralPath $probe -Destination (Join-Path $OutputDirectory 'typed-probe-original.cpp.txt')
Copy-Item -LiteralPath $runner -Destination (Join-Path $OutputDirectory 'owned-process-original.cs.txt')
if(-not ('DxvkRawProcessF4_02' -as [type])){Add-Type -TypeDefinition ([IO.File]::ReadAllText($runner))}
$stages=New-Object 'System.Collections.Generic.List[object]'
function Tool([string]$Name,[string]$Executable,[string[]]$Arguments){
 $rsp=Join-Path $OutputDirectory ($Name+'.rsp');$Arguments|Set-Content -LiteralPath $rsp -Encoding ascii
 $stdout=Join-Path $OutputDirectory ($Name+'.stdout.raw');$stderr=Join-Path $OutputDirectory ($Name+'.stderr.raw')
 $argsText='@"'+$rsp+'"'
 $child=[DxvkRawProcessF4_02]::Run($Executable,$argsText,$Packet,$stdout,$stderr,60000)
 $row=[ordered]@{name=$Name;executable=(Evidence $Executable);arguments=$argsText;response=(Evidence $rsp);
  response_arguments=$Arguments;response_count=$Arguments.Count;pid=$child.Pid;retained_process_handle=$child.ProcessHandle;
  start_utc=$child.StartUtc;exited=$child.Exited;exit_code_available=$child.ExitCodeAvailable;
  exit_code=$(if($child.ExitCodeAvailable){$child.ExitCode}else{$null});timed_out=$child.TimedOut;
  child_still_running=$child.ChildStillRunning;pipes_drained=$child.PipesDrained;seconds=$child.Seconds;deadline_ms=60000;
  stdout_bytes=$child.StdoutBytes;stderr_bytes=$child.StderrBytes;stdout=(Evidence $stdout);stderr=(Evidence $stderr);failure=$child.Failure}
 $row|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $OutputDirectory ($Name+'.process-result.json')) -Encoding UTF8
 $stages.Add($row)
 Require ($child.ProcessHandle -and $child.Exited -and $child.ExitCodeAvailable -and $child.ExitCode -eq 0 -and
 !$child.TimedOut -and !$child.ChildStillRunning -and $child.PipesDrained -and !$child.Failure) ('Actual view build stage failed: '+$Name)
}
$originalEnvironment=[ordered]@{PATH=$env:PATH;INCLUDE=$env:INCLUDE;LIB=$env:LIB;VSLANG=$env:VSLANG}
$result=[ordered]@{schema='native-d3d9-system-probes-build-v1';passed=$false;failure=$null;source_commit=$sourceInfo.source_commit;
 probe_before=(Evidence $probe);probe_after=$null;source_manifest_before=(Evidence $sourceManifest);source_manifest_after=$null;
 selected_inputs_before=@($allSourcePins);selected_inputs_after=@();views=@();stages=@();finalization_errors=@();
 module_or_probe_executed=$false;registration_modified=$false;GPU_calls_executed=0;full_compiler_attestation=$false}
try{
 foreach($view in $views){
  $arch=[string]$view.architecture;$expectedMachine=if($arch -ceq 'arm64'){0xaa64}else{0x8664}
  Require ($view.expected_machine -eq $expectedMachine) 'Exact architecture profile machine required'
  $selected=[object[]]$view.selected_inputs
  Require ($selected.Count -ge 10) 'Actual compiler/backend/resource/static-library input pins required'
  foreach($inputInfo in $selected){$result.selected_inputs_before+=@(Pinned $inputInfo)}
  foreach($name in @('cl.exe','link.exe','dumpbin.exe','c1xx.dll','c2.dll','1033\clui.dll')){
   $path=Join-Path $view.binary_directory $name
   Require (@($selected|Where-Object {[IO.Path]::GetFullPath($_.path) -ieq [IO.Path]::GetFullPath($path)}).Count -eq 1) 'Selected matching compiler/resource input missing'
  }
  [string[]]$includes=@($toolchainInfo.include_directories)+@($AdditionalIncludeDirectory);[string[]]$libraries=$view.library_directories
  Require ($includes.Count -ge 3 -and $libraries.Count -eq 3) 'Actual view SDK and static-library directories required'
  foreach($directory in ($includes+$libraries+@($view.binary_directory))){Require ($directory -cmatch '^[A-Z]:\\' -and !$directory.Contains(';') -and (Test-Path -LiteralPath $directory -PathType Container)) 'Actual scalar view directory required'}
  Require (@($libraries|Where-Object {$_ -match ('(?i)[\\/]'+$arch+'[\\/]?$')}).Count -eq 3) 'Only matching architecture libraries permitted'
  $env:PATH=$view.binary_directory+';'+[Environment]::SystemDirectory+';'+$originalEnvironment.PATH
  $env:INCLUDE=$includes -join ';';$env:LIB=$libraries -join ';';$env:VSLANG='1033'
  # A bounded strict syntax pass discovers actual SDK/STL inputs. Those exact
  # selected bytes are then captured before object compilation, and rehashed
  # in finally. The discovery pass itself is not a pre-snapshot compile claim.
  Tool ('discover-headers-'+$arch) (Join-Path $view.binary_directory 'cl.exe') @('/nologo','/std:c++17','/Zc:preprocessor','/EHsc','/MT','/O1','/W4','/WX',
   '/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00',('/FI"'+$prelude+'"'),'/showIncludes','/Zs',('"'+$probe+'"'))
  $headerLog=[IO.File]::ReadAllText((Join-Path $OutputDirectory ('discover-headers-'+$arch+'.stdout.raw')))
  $headerNames=@([regex]::Matches($headerLog,'(?m)^Note: including file:\s+(.+?)\r?$') | ForEach-Object {$_.Groups[1].Value.Trim()} | Sort-Object -Unique)
  Require ($headerNames.Count -gt 50) 'Actual English /showIncludes header discovery required'
  $headerPins=@($headerNames | ForEach-Object {Evidence $_})
  $result.selected_inputs_before+=@($headerPins)
  $headerPins|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $OutputDirectory ('selected-precompile-headers-'+$arch+'.json')) -Encoding UTF8
  $object=Join-Path $OutputDirectory ('d3d9-system-probe-'+$arch+'.obj')
  $exe=Join-Path $OutputDirectory ('d3d9-system-probe-'+$arch+'.exe')
  $pdb=Join-Path $OutputDirectory ('d3d9-system-probe-'+$arch+'.pdb')
  Tool ('compile-'+$arch) (Join-Path $view.binary_directory 'cl.exe') @('/nologo','/std:c++17','/Zc:preprocessor','/EHsc','/MT','/O1','/W4','/WX',
   '/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00',('/FI"'+$prelude+'"'),'/showIncludes','/c',('"'+$probe+'"'),('/Fo"'+$object+'"'))
  Require ((Machine $object $false) -eq $expectedMachine) 'Actual target COFF machine differs; no architecture macro substitutions'
  # Explicit static CRT and OS imports are captured before link, including
  # newly required user32. Only the GDI screen and token APIs add OS imports; D3D factory calls remain exact dynamic SYSTEM exports.
  $linkInputs=@();$linkPins=@()
  foreach($name in @('libcmt.lib','libcpmt.lib','libvcruntime.lib','libucrt.lib','oldnames.lib','kernel32.lib','user32.lib','gdi32.lib','advapi32.lib')){
   $chosen=$null
   foreach($directory in $libraries){$candidate=Join-Path $directory $name;if(Test-Path -LiteralPath $candidate -PathType Leaf){$chosen=Evidence $candidate;break}}
   Require ($null -ne $chosen) ('Missing actual selected probe link input: '+$name)
   $result.selected_inputs_before+=@($chosen);$linkPins+=@($chosen);$linkInputs+=@('"'+$chosen.path+'"')
  }
  $linkPins|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $OutputDirectory ('selected-link-inputs-'+$arch+'.json')) -Encoding UTF8
  Tool ('link-'+$arch) (Join-Path $view.binary_directory 'link.exe') (@('/nologo','/WX',('/MACHINE:'+$arch.ToUpperInvariant()),'/SUBSYSTEM:CONSOLE','/DEBUG:FULL',
   ('"'+$object+'"'),('/OUT:"'+$exe+'"'),('/PDB:"'+$pdb+'"'),('/LINKREPROFULLPATHRSP:"'+(Join-Path $OutputDirectory ('actual-link-inputs-'+$arch+'.rsp'))+'"')) + $linkInputs)
  Require ((Machine $exe $true) -eq $expectedMachine) 'Actual view executable PE machine differs'
  Tool ('headers-'+$arch) (Join-Path $view.binary_directory 'dumpbin.exe') @('/headers',('"'+$exe+'"'))
  Tool ('imports-'+$arch) (Join-Path $view.binary_directory 'dumpbin.exe') @('/imports',('"'+$exe+'"'))
  $imports=[IO.File]::ReadAllText((Join-Path $OutputDirectory ('imports-'+$arch+'.stdout.raw')))
  Require ($imports -notmatch '(?i)\b(?:d3d8|d3d9|d3d10(?:_1)?|d3d11|dxgi|viogpudxvk(?:_x64)?|viogpudxvk_validate10|d3dcompiler_47|msvcp[0-9_]*|vcruntime[0-9_]*|ucrtbase)\.dll\b|Direct3DCreate|D3D10CreateDevice|D3D11CreateDevice') 'Ordinary probe must dynamically load exact SYSTEM paths; no D3D factory/runtime imports'
  $result.views+=@([ordered]@{architecture=$arch;machine=$expectedMachine;object=(Evidence $object);executable=(Evidence $exe);pdb=(Evidence $pdb);
   executed=$false;runtime_candidate_inputs_pending=$true})
 }
 Require ($stages.Count -eq 10 -and $result.views.Count -eq 2) 'Both actual view executables and ten owned build stages required'
 $result.passed=$true
}catch{$result.failure=[ordered]@{exception=$_.Exception.ToString();stack=$_.ScriptStackTrace;invocation=$_.InvocationInfo.PositionMessage}}
finally{
 foreach($key in $originalEnvironment.Keys){[Environment]::SetEnvironmentVariable($key,$originalEnvironment[$key],'Process')}
 try{
  foreach($inputInfo in $result.selected_inputs_before){$result.selected_inputs_after+=@(Pinned $inputInfo)}
  $result.probe_after=Evidence $probe;$result.source_manifest_after=Evidence $sourceManifest
  Require ($result.probe_before.sha256 -ceq $result.probe_after.sha256 -and $result.source_manifest_before.sha256 -ceq $result.source_manifest_after.sha256 -and
   (Evidence $ToolchainManifestPath).sha256 -ceq $ToolchainManifestSha256) 'Original view inputs changed during producer attempt'
 }catch{$result.passed=$false;$result.finalization_errors+=@($_.Exception.ToString())}
 $result.stages=$stages.ToArray()
 $result|ConvertTo-Json -Depth 16|Set-Content -LiteralPath (Join-Path $OutputDirectory 'view-build-original.json') -Encoding UTF8
}
$result|ConvertTo-Json -Depth 16
Require $result.passed 'Collect actual failed view build; no compiler or linker fallback/retry'
