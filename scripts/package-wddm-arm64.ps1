[CmdletBinding()]
param(
   [Parameter(Mandatory = $true)]
   [string]$DxvkRoot,

   [Parameter(Mandatory = $true)]
   [string]$TurnipBundleRoot,

   [Parameter(Mandatory = $true)]
   [string]$OutputRoot,

   [string]$DxvkRevision = 'unknown',

   [string]$MesaRepository = 'unknown',

   [string]$MesaRunId = 'unknown'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$dxvkDlls = @('d3d8.dll', 'd3d9.dll', 'd3d10core.dll', 'd3d11.dll', 'dxgi.dll')
$turnipFiles = @('freedreno_icd.arm64.json', 'vulkan_freedreno.dll', 'z-1.dll')

function Get-PeMachine {
   param([Parameter(Mandatory = $true)][string]$Path)

   $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
                              [IO.FileShare]::Read)
   try {
      $reader = [IO.BinaryReader]::new($stream)
      if ($stream.Length -lt 0x40 -or $reader.ReadUInt16() -ne 0x5a4d) {
         throw "$Path is not a PE image."
      }
      $stream.Position = 0x3c
      $peOffset = $reader.ReadUInt32()
      if ($peOffset -gt $stream.Length - 6) {
         throw "$Path has an invalid PE header offset."
      }
      $stream.Position = $peOffset
      if ($reader.ReadUInt32() -ne 0x00004550) {
         throw "$Path does not have a PE signature."
      }
      return $reader.ReadUInt16()
   }
   finally {
      $stream.Dispose()
   }
}

function Get-Sha256 {
   param([Parameter(Mandatory = $true)][string]$Path)
   return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-Arm64Pe {
   param([Parameter(Mandatory = $true)][string]$Path)
   if ((Get-PeMachine -Path $Path) -ne 0xaa64) {
      throw "$Path is not an ARM64 PE image."
   }
}

function Assert-TurnipBundle {
   param([Parameter(Mandatory = $true)][string]$Root)

   $hashPath = Join-Path $Root 'SHA256SUMS.txt'
   if (-not (Test-Path -LiteralPath $hashPath -PathType Leaf)) {
      throw "Turnip bundle is missing SHA256SUMS.txt: $Root"
   }

   $covered = @{}
   foreach ($line in Get-Content -LiteralPath $hashPath) {
      if ($line -notmatch '^([0-9a-f]{64})  ([^\\/]+)$') {
         throw "Malformed Turnip hash entry: $line"
      }
      $name = $Matches[2]
      if ($covered.ContainsKey($name)) {
         throw "Duplicate Turnip hash entry: $name"
      }
      $path = Join-Path $Root $name
      if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
          (Get-Sha256 -Path $path) -ne $Matches[1]) {
         throw "Turnip bundle hash mismatch: $name"
      }
      $covered[$name] = $true
   }

   foreach ($name in $turnipFiles) {
      if (-not $covered.ContainsKey($name)) {
         throw "Turnip hash list does not cover required file: $name"
      }
   }

   $manifest = Get-Content -LiteralPath (Join-Path $Root $turnipFiles[0]) -Raw |
      ConvertFrom-Json
   if ($manifest.file_format_version -ne '1.0.1' -or
       $manifest.ICD.library_path -ne '.\vulkan_freedreno.dll' -or
       $manifest.ICD.library_arch -ne '64' -or
       $manifest.ICD.api_version -notmatch '^1\.4\.[0-9]+$') {
      throw 'Turnip manifest is not the expected Windows ARM64 ICD manifest.'
   }
   Assert-Arm64Pe -Path (Join-Path $Root 'vulkan_freedreno.dll')
   Assert-Arm64Pe -Path (Join-Path $Root 'z-1.dll')
}

$DxvkRoot = [IO.Path]::GetFullPath($DxvkRoot)
$TurnipBundleRoot = [IO.Path]::GetFullPath($TurnipBundleRoot)
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

if (-not (Test-Path -LiteralPath $DxvkRoot -PathType Container)) {
   throw "DXVK build root does not exist: $DxvkRoot"
}
if (-not (Test-Path -LiteralPath $TurnipBundleRoot -PathType Container)) {
   throw "Turnip bundle root does not exist: $TurnipBundleRoot"
}

Assert-TurnipBundle -Root $TurnipBundleRoot
if ((Test-Path -LiteralPath $OutputRoot -PathType Container) -and
    (@(Get-ChildItem -LiteralPath $OutputRoot -Force).Count -ne 0)) {
   throw "Output directory must be empty: $OutputRoot"
}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

foreach ($name in $dxvkDlls) {
   $matches = @(Get-ChildItem -LiteralPath $DxvkRoot -Recurse -File -Filter $name)
   if ($matches.Count -eq 0) {
      throw "DXVK build is missing required DLL: $name"
   }
   if ($matches.Count -ne 1) {
      throw "DXVK build contains multiple candidates for required DLL: $name"
   }
   $source = $matches[0].FullName
   Assert-Arm64Pe -Path $source
   Copy-Item -LiteralPath $source -Destination (Join-Path $OutputRoot $name) -Force
}

foreach ($name in $turnipFiles) {
   Copy-Item -LiteralPath (Join-Path $TurnipBundleRoot $name) `
      -Destination (Join-Path $OutputRoot $name) -Force
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'launch-wddm-arm64.ps1') `
   -Destination (Join-Path $OutputRoot 'launch-wddm-arm64.ps1') -Force
if (Test-Path -LiteralPath (Join-Path $DxvkRoot 'dxvk.conf') -PathType Leaf) {
   Copy-Item -LiteralPath (Join-Path $DxvkRoot 'dxvk.conf') `
      -Destination (Join-Path $OutputRoot 'dxvk.conf') -Force
}

@"
DXVK_WDDM_BUNDLE_FORMAT=1
DXVK_REPOSITORY=sunflower2333/dxvk
DXVK_REVISION=$DxvkRevision
MESA_REPOSITORY=$MesaRepository
MESA_RUN_ID=$MesaRunId
TURNIP_MANIFEST=freedreno_icd.arm64.json
KMD_REQUIRED=viogpuwddm.sys
RUNTIME_STATUS=compile-and-package-evidence-only
"@ | Set-Content -LiteralPath (Join-Path $OutputRoot 'BUILD-PROVENANCE.txt') -Encoding ascii

@'
DXVK ARM64 plus Mesa Turnip WDDM app-local bundle.

Copy the DLLs and ICD files beside a native ARM64 D3D8/9/10/11 application.
Run launch-wddm-arm64.ps1 from that directory. The launcher selects the bundled
Turnip manifest through VK_DRIVER_FILES and never installs a system ICD or KMD.
This package requires the matching viogpuwddm.sys Native Context miniport.
'@ | Set-Content -LiteralPath (Join-Path $OutputRoot 'README-wddm-arm64.txt') -Encoding ascii

$hashLines = Get-ChildItem -LiteralPath $OutputRoot -File |
   Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
   Sort-Object Name |
   ForEach-Object { "$(Get-Sha256 -Path $_.FullName)  $($_.Name)" }
$hashLines | Set-Content -LiteralPath (Join-Path $OutputRoot 'SHA256SUMS.txt') -Encoding ascii

Write-Host "Created verified DXVK/Turnip bundle: $OutputRoot"
Get-ChildItem -LiteralPath $OutputRoot -File | Sort-Object Name |
   Format-Table Name, Length
