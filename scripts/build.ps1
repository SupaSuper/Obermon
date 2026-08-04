[CmdletBinding()]
param(
  [string]$WorkRoot = "C:\ObermonBuild",
  [ValidateSet("Release", "Debug")][string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Src = Join-Path $WorkRoot "chromium\src"
$DepotTools = Join-Path $WorkRoot "depot_tools"
$Out = Join-Path $Src ("out\Obermon" + $Configuration)
$EngineSource = Join-Path $Src "chrome\browser\obermon\generated_engine"
$ProductDir = Join-Path $Out "obermon"
$env:PATH = "$DepotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

if (-not (Test-Path $Src)) { throw "Run scripts/bootstrap.ps1 first." }
$IsDebug = if ($Configuration -eq "Debug") { "true" } else { "false" }
$Args = @"
is_debug=$IsDebug
is_component_build=false
symbol_level=1
target_cpu="x64"
use_remoteexec=false
is_official_build=false
chrome_pgo_phase=0
enable_nacl=false
"@

Push-Location $Src
try {
  gn gen $Out --args=$Args
  autoninja -C $Out chrome obermon_tests
} finally { Pop-Location }

New-Item -ItemType Directory -Force -Path $ProductDir | Out-Null
Push-Location $EngineSource
try {
  go build -trimpath -ldflags "-s -w" -o (Join-Path $ProductDir "scramjet-engine.exe") .
} finally { Pop-Location }

$Resources = @(
  @{ Source = "chrome\browser\resources\obermon_scramjet"; Destination = "scramjet_extension" },
  @{ Source = "chrome\browser\resources\obermon_theme"; Destination = "theme_extension" }
)
foreach ($Resource in $Resources) {
  $Source = Join-Path $Src $Resource.Source
  $Destination = Join-Path $ProductDir $Resource.Destination
  Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
  Copy-Item $Source $Destination -Recurse
}
Write-Host "Build completed: $Out" -ForegroundColor Green
