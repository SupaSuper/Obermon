# Windows build

## Requirements

- Windows 11 x64 on an NTFS volume.
- Visual Studio 2022 with Desktop development with C++, Windows 11 SDK, ATL, and MFC.
- Git, Python 3, Node.js, PowerShell 7, and at least 180 GB free disk space.
- 32 GB RAM recommended.

The build uses a self-hosted Windows runner because standard hosted runners do not provide enough persistent disk or time for a Chromium build.

## Commands

```powershell
pwsh ./scripts/bootstrap.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/build.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/test.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/package.ps1 -WorkRoot C:\ObermonBuild
```

The final archive is written to `C:\ObermonBuild\dist\Obermon-Windows-x64.zip` only after the selected unit and browser tests pass.

## Visual reference installer

A Vivaldi installer can optionally be supplied to `scripts/extract-vivaldi-reference.ps1`. The script extracts a local reference tree and records file hashes. These files are ignored by Git and are never added to the product package.
