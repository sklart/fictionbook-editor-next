<#
.SYNOPSIS
Собирает Scintilla и Lexilla и копирует их DLL в runtime.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

foreach ($build in @(
    @{ Directory = "third_party\scintilla\win32"; Makefile = "scintilla.mak" },
    @{ Directory = "third_party\lexilla\src"; Makefile = "lexilla.mak" }
)) {
    Push-Location (Join-Path $repoRoot $build.Directory)
    try {
        & nmake.exe /nologo /f $build.Makefile QUIET=1
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    finally {
        Pop-Location
    }
}

$runtimeDir = Join-Path $repoRoot "runtime"
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\scintilla\bin\Scintilla.dll") `
    -Destination $runtimeDir -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\lexilla\bin\Lexilla.dll") `
    -Destination $runtimeDir -Force

Write-Host "Scintilla 5.6.3 и Lexilla 5.5.0 подготовлены в $runtimeDir."
