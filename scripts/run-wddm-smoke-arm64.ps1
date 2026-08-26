[CmdletBinding()]
param(
   [string]$BundleRoot = $PSScriptRoot,
   [string]$DeviceLuid = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$BundleRoot = [IO.Path]::GetFullPath($BundleRoot)
$manifest = Join-Path $BundleRoot 'freedreno_icd.arm64.json'
$smokes = @(
   'dxvk_wddm_d3d9_smoke_arm64.exe',
   'dxvk_wddm_d3d10_smoke_arm64.exe',
   'dxvk_wddm_d3d11_smoke_arm64.exe'
)

if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
   throw "The bundled Turnip manifest is missing: $manifest"
}
if (-not [string]::IsNullOrWhiteSpace($DeviceLuid) -and
    $DeviceLuid -notmatch '^[0-9a-fA-F]{16}$') {
   throw 'DeviceLuid must contain exactly 16 hexadecimal digits.'
}

$env:VK_DRIVER_FILES = $manifest
$env:VK_ICD_FILENAMES = $manifest
$env:DXVK_HUD = 'devinfo,version,submissions'
$env:DXVK_LOG_LEVEL = 'info'
$logRoot = Join-Path $BundleRoot 'logs'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
$env:DXVK_LOG_PATH = $logRoot
if (-not [string]::IsNullOrWhiteSpace($DeviceLuid)) {
   $env:DXVK_FILTER_DEVICE_LUID = $DeviceLuid.ToLowerInvariant()
}

Push-Location $BundleRoot
try {
   foreach ($name in $smokes) {
      $path = Join-Path $BundleRoot $name
      if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
         throw "Smoke workload is missing from the bundle: $name"
      }
      Write-Host "Running $name with app-local Turnip ICD"
      & $path
      if ($LASTEXITCODE -ne 0) {
         throw "$name failed with exit code $LASTEXITCODE"
      }
   }
}
finally {
   Pop-Location
}

Write-Host 'DXVK ARM64 WDDM D3D9/D3D10/D3D11 smoke workloads passed.'
Get-ChildItem -LiteralPath $logRoot -File -ErrorAction SilentlyContinue |
   Sort-Object Name | Format-Table Name, Length
