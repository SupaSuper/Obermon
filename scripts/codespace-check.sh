#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "== Obermon Codespaces validation =="
echo "Repository: $repo_root"
echo "OS: $(uname -a)"
echo "Python: $(python3 --version)"
echo "Node: $(node --version)"
echo "Go: $(go version)"

python3 scripts/validate_source.py
node --check benchmarks/run-speedometer.mjs
python3 -m json.tool .devcontainer/devcontainer.json >/dev/null
python3 -m json.tool src/chromium/chrome/browser/resources/obermon_scramjet/manifest.json >/dev/null
python3 -m json.tool src/chromium/chrome/browser/resources/obermon_theme/manifest.json >/dev/null
shellcheck scripts/codespace-check.sh

echo
cat <<'NOTICE'
Codespaces validation passed.

This Linux environment validates Obermon-owned source, the local Go engine,
extension JavaScript, manifests, and benchmark automation. It does not build
the Windows Chromium product and does not produce a release-grade Speedometer
score. Use the self-hosted Windows workflow for those steps.
NOTICE
