[CmdletBinding()]
param(
  [string]$WorkRoot = "C:\ObermonBuild",
  [ValidateSet("Release", "Debug")][string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Out = Join-Path $WorkRoot ("chromium\src\out\Obermon" + $Configuration)
$Dist = Join-Path $WorkRoot "dist"
$Stage = Join-Path $Dist "Obermon-Windows-x64"
$Archive = Join-Path $Dist "Obermon-Windows-x64.zip"

if (-not (Test-Path (Join-Path $Out "chrome.exe"))) { throw "Browser binary is missing. Build and test first." }
if (-not (Test-Path (Join-Path $Out "obermon\scramjet-engine.exe"))) { throw "Scramjet engine binary is missing." }
Remove-Item $Stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

$Required = @("chrome.exe", "chrome.dll", "chrome_elf.dll", "resources.pak", "icudtl.dat", "v8_context_snapshot.bin")
foreach ($File in $Required) {
  $Source = Join-Path $Out $File
  if (-not (Test-Path $Source)) { throw "Required runtime file is missing: $Source" }
  Copy-Item $Source $Stage
}

$Optional = @(
  "chrome_100_percent.pak", "chrome_200_percent.pak", "D3DCompiler_47.dll",
  "d3dcompiler_47.dll", "libEGL.dll", "libGLESv2.dll", "vk_swiftshader.dll",
  "vk_swiftshader_icd.json", "vulkan-1.dll", "dxcompiler.dll", "dxil.dll",
  "snapshot_blob.bin"
)
foreach ($File in $Optional) {
  $Source = Join-Path $Out $File
  if (Test-Path $Source) { Copy-Item $Source $Stage }
}

foreach ($Directory in @("locales", "MEIPreload", "obermon")) {
  $Source = Join-Path $Out $Directory
  if (Test-Path $Source) { Copy-Item $Source $Stage -Recurse }
}
Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^\d+\.\d+' } | Copy-Item -Destination $Stage -Recurse

Rename-Item (Join-Path $Stage "chrome.exe") "obermon.exe"
Set-Content -Path (Join-Path $Stage "VERSION.txt") -Value (git -C (Join-Path $WorkRoot "chromium\src") rev-parse HEAD)
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Compress-Archive -Path "$Stage\*" -DestinationPath $Archive -Force
Write-Host "Package created: $Archive" -ForegroundColor Green
