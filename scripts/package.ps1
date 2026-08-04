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
Remove-Item $Stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

$Files = @("chrome.exe", "chrome.dll", "chrome_elf.dll", "resources.pak", "icudtl.dat", "v8_context_snapshot.bin")
foreach ($File in $Files) {
  $Source = Join-Path $Out $File
  if (Test-Path $Source) { Copy-Item $Source $Stage }
}
Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^\d+\.\d+' -or $_.Name -in @('locales', 'MEIPreload') } | Copy-Item -Destination $Stage -Recurse

Rename-Item (Join-Path $Stage "chrome.exe") "obermon.exe"
Set-Content -Path (Join-Path $Stage "VERSION.txt") -Value (git -C (Join-Path $WorkRoot "chromium\src") rev-parse HEAD)
Compress-Archive -Path "$Stage\*" -DestinationPath $Archive -Force
Write-Host "Package created: $Archive" -ForegroundColor Green
