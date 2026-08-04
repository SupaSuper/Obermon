[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$Installer,
  [string]$Destination = "reference\vivaldi-extracted"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Installer = [IO.Path]::GetFullPath($Installer)
$Destination = Join-Path $RepoRoot $Destination
if (-not (Test-Path $Installer)) { throw "Installer not found: $Installer" }
if (-not (Get-Command 7z -ErrorAction SilentlyContinue)) { throw "7-Zip command '7z' is required." }

Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
& 7z x $Installer "-o$Destination" -y | Out-Host
Get-ChildItem $Destination -File -Recurse | Get-FileHash -Algorithm SHA256 |
  Sort-Object Path | ConvertTo-Json | Set-Content (Join-Path $Destination "reference-hashes.json")
Write-Host "Reference extracted locally. It is excluded from Git and release packaging." -ForegroundColor Yellow
