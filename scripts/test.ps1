[CmdletBinding()]
param(
  [string]$WorkRoot = "C:\ObermonBuild",
  [ValidateSet("Release", "Debug")][string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Out = Join-Path $WorkRoot ("chromium\src\out\Obermon" + $Configuration)
$Unit = Join-Path $Out "obermon_tests.exe"
if (-not (Test-Path $Unit)) { throw "Missing test binary: $Unit" }

& $Unit --gtest_color=yes
if ($LASTEXITCODE -ne 0) { throw "Obermon unit tests failed." }

$BrowserTests = Join-Path $Out "browser_tests.exe"
if (Test-Path $BrowserTests) {
  & $BrowserTests --gtest_filter="ObermonScramjet*" --test-launcher-jobs=1
  if ($LASTEXITCODE -ne 0) { throw "Obermon browser tests failed." }
}
