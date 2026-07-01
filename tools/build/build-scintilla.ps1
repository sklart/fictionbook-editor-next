<#
.SYNOPSIS
Собирает Scintilla и Lexilla и копирует их DLL в runtime.
#>

[CmdletBinding()]
param(
    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$VcVarsVersion
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if ($CompatibilityTarget -eq "Win7" -and -not $VcVarsVersion) {
    $VcVarsVersion = "14.44"
}

if ($VcVarsVersion) {
    try {
        & (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") `
            -Arch x86 `
            -HostArch x64 `
            -VcVarsVersion $VcVarsVersion
        Write-Host "Scintilla/Lexilla: используется vcvars_ver=$VcVarsVersion."
    }
    catch {
        if ($CompatibilityTarget -eq "Win7") {
            throw
        }

        Write-Warning "Не удалось включить vcvars_ver=$VcVarsVersion для Scintilla/Lexilla: $($_.Exception.Message)"
        Write-Warning "Продолжаю со стандартной средой Visual Studio."
        & (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
    }
}
else {
    & (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
}

foreach ($build in @(
    @{ Directory = "third_party\scintilla\win32"; Makefile = "scintilla.mak"; Arguments = @() },
    @{ Directory = "third_party\lexilla\src"; Makefile = "lexilla.mak" }
)) {
    $makeArguments = @("/nologo", "/f", $build.Makefile, "QUIET=1")
    if ($CompatibilityTarget -eq "Win7" -and $build.Directory -eq "third_party\scintilla\win32") {
        $makeArguments += "ADD_DEFINE=-DFBE_SCINTILLA_WINVER=0x0601"
    }

    Push-Location (Join-Path $repoRoot $build.Directory)
    try {
        & nmake.exe /nologo /f $build.Makefile clean
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
        & nmake.exe @makeArguments
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

Write-Host "Scintilla 5.6.3 и Lexilla 5.5.0 подготовлены в $runtimeDir ($CompatibilityTarget)."
