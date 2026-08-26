[CmdletBinding()]
param(
   [Parameter(Mandatory = $true, Position = 0)]
   [string]$ApplicationPath,

   [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
   [string[]]$ApplicationArguments,

   [string]$DeviceLuid = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$bundleRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$manifest = Join-Path $bundleRoot 'freedreno_icd.arm64.json'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
   throw "The bundled Turnip manifest is missing: $manifest"
}

$application = [IO.Path]::GetFullPath($ApplicationPath)
if (-not (Test-Path -LiteralPath $application -PathType Leaf)) {
   throw "Application does not exist: $application"
}
if ([IO.Path]::GetDirectoryName($application) -ne $bundleRoot) {
   throw 'The application must be in the same directory as the DXVK/Turnip bundle.'
}

$env:VK_DRIVER_FILES = $manifest
# Keep compatibility with Vulkan loaders predating VK_DRIVER_FILES. Both
# variables point at the same app-local manifest and never install a system ICD.
$env:VK_ICD_FILENAMES = $manifest
if (-not [string]::IsNullOrWhiteSpace($DeviceLuid)) {
   if ($DeviceLuid -notmatch '^[0-9a-fA-F]{16}$') {
      throw 'DeviceLuid must contain exactly 16 hexadecimal digits.'
   }
   $env:DXVK_FILTER_DEVICE_LUID = $DeviceLuid.ToLowerInvariant()
}
else {
   Remove-Item Env:DXVK_FILTER_DEVICE_LUID -ErrorAction SilentlyContinue
}

Push-Location $bundleRoot
try {
   & $application @ApplicationArguments
   exit $LASTEXITCODE
}
finally {
   Pop-Location
}
