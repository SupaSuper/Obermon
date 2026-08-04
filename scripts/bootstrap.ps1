[CmdletBinding()]
param(
  [string]$WorkRoot = "C:\ObermonBuild",
  [switch]$SkipChromiumSync,
  [switch]$SkipScramjetBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Config = Get-Content (Join-Path $RepoRoot "config\upstream.json") -Raw | ConvertFrom-Json
$WorkRoot = [IO.Path]::GetFullPath($WorkRoot)
$ChromiumRoot = Join-Path $WorkRoot "chromium"
$ChromiumSrc = Join-Path $ChromiumRoot "src"
$DepotTools = Join-Path $WorkRoot "depot_tools"
$ScramjetSrc = Join-Path $WorkRoot "scramjet-src"

function Require-Command([string]$Name) {
  if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
    throw "Required command '$Name' was not found in PATH."
  }
}

if (-not $IsWindows) { throw "Obermon's supported release build currently requires Windows." }
Require-Command git
Require-Command python
Require-Command node
Require-Command corepack

New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null
$Drive = Get-PSDrive -Name ([IO.Path]::GetPathRoot($WorkRoot).Substring(0,1))
if ($Drive.Free -lt 180GB) {
  throw "At least 180 GB free is required. Available: $([math]::Round($Drive.Free / 1GB, 1)) GB."
}

if (-not (Test-Path (Join-Path $DepotTools ".git"))) {
  git clone --depth 1 $Config.depotTools.repository $DepotTools
}
$env:PATH = "$DepotTools;$env:PATH"
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"

if (-not $SkipChromiumSync) {
  if (-not (Test-Path (Join-Path $ChromiumSrc ".git"))) {
    New-Item -ItemType Directory -Force -Path $ChromiumRoot | Out-Null
    Push-Location $ChromiumRoot
    try { fetch --nohooks chromium } finally { Pop-Location }
  }

  git -C $ChromiumSrc fetch origin $Config.chromium.ref --depth 1
  git -C $ChromiumSrc checkout --detach FETCH_HEAD
  Push-Location $ChromiumRoot
  try { gclient sync -D --force --with_branch_heads --with_tags } finally { Pop-Location }
}

if (-not $SkipScramjetBuild) {
  if (-not (Test-Path (Join-Path $ScramjetSrc ".git"))) {
    git clone $Config.scramjet.repository $ScramjetSrc
  }
  git -C $ScramjetSrc fetch origin $Config.scramjet.ref --depth 1
  git -C $ScramjetSrc checkout --detach FETCH_HEAD
  corepack enable
  corepack prepare $Config.scramjet.packageManager --activate
  Push-Location $ScramjetSrc
  try {
    pnpm install --frozen-lockfile
    pnpm --filter @mercuryworkshop/scramjet run rewriter:build
    pnpm --filter @mercuryworkshop/scramjet run build
  } finally { Pop-Location }
}

python (Join-Path $RepoRoot "scripts\materialize.py") `
  --repo $RepoRoot `
  --chromium $ChromiumSrc `
  --scramjet $ScramjetSrc

python (Join-Path $RepoRoot "scripts\apply_source_edits.py") --chromium $ChromiumSrc
Write-Host "Obermon source materialized at $ChromiumSrc" -ForegroundColor Green
