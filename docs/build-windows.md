# Windows build

## Requirements

- Windows 11 x64 on an NTFS volume.
- Visual Studio 2022 with Desktop development with C++, Windows 11 SDK, ATL, and MFC.
- Git, Python 3, Node.js, Go, PowerShell 7, and at least 180 GB free disk space.
- 32 GB RAM recommended.

## Commands

```powershell
pwsh ./scripts/bootstrap.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/build.ps1 -WorkRoot C:\ObermonBuild
pwsh ./scripts/package.ps1 -WorkRoot C:\ObermonBuild
```

The final archive is written to:

```text
C:\ObermonBuild\dist\Obermon-Windows-x64.zip
```

## Visual reference installer

A Vivaldi installer can optionally be supplied to `scripts/extract-vivaldi-reference.ps1`. The script extracts a local reference tree and records file hashes. Those files are ignored by Git and are never added to the Obermon source repository or product package.
