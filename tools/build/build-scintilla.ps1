<#
.SYNOPSIS
Собирает Scintilla и Lexilla и копирует их DLL в runtime.
#>

[CmdletBinding()]
param(
    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$VcVarsVersion,

    # Каталог для целевых DLL редактора. Пустое значение сохраняет
    # историческое поведение и использует out\editor-runtime\<вариант>.
    [string]$OutputDirectory = ""
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
$editorRuntimeDir = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Join-Path $repoRoot ("out\editor-runtime\{0}" -f $CompatibilityTarget)
} else {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
}
$scintillaVersionCode = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot "third_party\scintilla\version.txt")).Trim()
$lexillaVersionCode = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot "third_party\lexilla\version.txt")).Trim()
if ($scintillaVersionCode -notmatch '^\d{3}$' -or $lexillaVersionCode -notmatch '^\d{3}$') {
    throw "Не удалось прочитать трёхзначные версии Scintilla/Lexilla из version.txt."
}
$scintillaVersion = "{0}.{1}.{2}" -f $scintillaVersionCode.Substring(0, 1), $scintillaVersionCode.Substring(1, 1), $scintillaVersionCode.Substring(2, 1)
$lexillaVersion = "{0}.{1}.{2}" -f $lexillaVersionCode.Substring(0, 1), $lexillaVersionCode.Substring(1, 1), $lexillaVersionCode.Substring(2, 1)
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\scintilla\bin\Scintilla.dll") `
    -Destination $runtimeDir -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\lexilla\bin\Lexilla.dll") `
    -Destination $runtimeDir -Force
New-Item -ItemType Directory -Path $editorRuntimeDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\scintilla\bin\Scintilla.dll") `
    -Destination (Join-Path $editorRuntimeDir "Scintilla.dll") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\lexilla\bin\Lexilla.dll") `
    -Destination (Join-Path $editorRuntimeDir "Lexilla.dll") -Force

Write-Host "Scintilla $scintillaVersion и Lexilla $lexillaVersion подготовлены в $runtimeDir ($CompatibilityTarget)."
Write-Host "Целевые DLL редактора сохранены в $editorRuntimeDir."
