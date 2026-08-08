<#
.SYNOPSIS
Собирает Scintilla и Lexilla и копирует их DLL в runtime.
#>

[CmdletBinding()]
param(
    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$PlatformToolset,

    [string]$VcVarsVersion,

    # Каталог для целевых DLL редактора. Пустое значение сохраняет
    # историческое поведение и использует out\editor-runtime\<вариант>.
    [string]$OutputDirectory = "",

    # Разрешает использовать предварительно проверенный runtime из CI-кэша.
    # Без ключа локальный запуск всегда сохраняет привычную полную сборку.
    [switch]$ReusePreparedRuntime
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$editorRuntimeDir = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Join-Path $repoRoot ("out\editor-runtime\{0}" -f $CompatibilityTarget)
} else {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
}
$runtimeDir = Join-Path $repoRoot "runtime"
$fingerprintPath = Join-Path $editorRuntimeDir "fbe-editor-runtime-fingerprint.json"

if ($CompatibilityTarget -eq "Win7" -and -not $VcVarsVersion) {
    $VcVarsVersion = "14.44"
}

if ($VcVarsVersion) {
    try {
        . (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") `
            -Arch x86 `
            -HostArch x64 `
            -PlatformToolset $PlatformToolset `
            -VcVarsVersion $VcVarsVersion
        Write-Host "Scintilla/Lexilla: используется vcvars_ver=$VcVarsVersion."
    }
    catch {
        if ($CompatibilityTarget -eq "Win7") {
            throw
        }

        Write-Warning "Не удалось включить vcvars_ver=$VcVarsVersion для Scintilla/Lexilla: $($_.Exception.Message)"
        Write-Warning "Продолжаю со стандартной средой Visual Studio."
        . (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
    }
}
else {
    . (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
}

function Test-PreparedRuntimeFingerprint {
    $prepared = @("Scintilla.dll", "Lexilla.dll") | ForEach-Object { Join-Path $editorRuntimeDir $_ }
    if (@($prepared | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -gt 0 -or
        -not (Test-Path -LiteralPath $fingerprintPath -PathType Leaf)) {
        return $false
    }
    try { $fingerprint = Get-Content -Raw -LiteralPath $fingerprintPath | ConvertFrom-Json } catch { return $false }
    if ($fingerprint.compatibilityTarget -ne $CompatibilityTarget -or $fingerprint.platformToolset -ne $PlatformToolset) { return $false }
    if ($CompatibilityTarget -eq "Win7" -and -not ([string]$fingerprint.vcToolsVersion).StartsWith("14.44")) { return $false }
    return $true
}

if ($ReusePreparedRuntime -and (Test-PreparedRuntimeFingerprint)) {
    foreach ($name in @("Scintilla.dll", "Lexilla.dll")) { Copy-Item -LiteralPath (Join-Path $editorRuntimeDir $name) -Destination $runtimeDir -Force }
    Write-Host "Scintilla/Lexilla: validated editor runtime cache hit ($CompatibilityTarget, toolset=$PlatformToolset)."
    return
}
if ($ReusePreparedRuntime) { Write-Host "Editor runtime cache fingerprint не соответствует текущему toolchain; выполняется rebuild." }

function Get-NmakePath {
    $fromPath = Get-Command nmake.exe -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Source -First 1
    if ($fromPath) {
        return $fromPath
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $vswhereArguments = @("-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64")
    if ($PlatformToolset -eq "v143") { $vswhereArguments += @("-version", "[17.0,18.0)") }
    $installationPath = & $vswhere @vswhereArguments -property installationPath | Select-Object -First 1
    if (-not $installationPath) {
        throw "Не удалось определить Visual Studio для nmake.exe."
    }

    $toolDirectories = Get-ChildItem -LiteralPath (Join-Path $installationPath "VC\Tools\MSVC") -Directory |
        Sort-Object Name -Descending
    if ($VcVarsVersion) {
        $toolDirectories = @($toolDirectories | Where-Object { $_.Name -like "$VcVarsVersion*" }) +
            @($toolDirectories | Where-Object { $_.Name -notlike "$VcVarsVersion*" })
    }
    foreach ($toolDirectory in $toolDirectories) {
        $candidate = Join-Path $toolDirectory.FullName "bin\HostX64\x86\nmake.exe"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    throw "Не найден nmake.exe в установленном наборе инструментов Visual Studio."
}

$nmake = Get-NmakePath
$compilerBinDirectory = Split-Path -Parent $nmake
$resourceCompiler = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter rc.exe -File `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x86\\rc\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $resourceCompiler) {
    throw "Не найден rc.exe из Windows SDK."
}
Write-Host "Scintilla/Lexilla toolchain: target=$CompatibilityTarget; PlatformToolset=$PlatformToolset; cl.exe=$((Get-Command cl.exe -ErrorAction Stop).Source); VCToolsVersion=$env:VCToolsVersion; nmake.exe=$nmake; rc.exe=$($resourceCompiler.FullName)"
$requiredToolDirectories = @($compilerBinDirectory, $resourceCompiler.Directory.FullName)
$env:Path = (($requiredToolDirectories + @($env:Path)) | Select-Object -Unique) -join ';'
[Environment]::SetEnvironmentVariable("Path", $env:Path, "Process")

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
        & $nmake /nologo /f $build.Makefile clean
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
        & $nmake @makeArguments
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    finally {
        Pop-Location
    }
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

[ordered]@{
    compatibilityTarget = $CompatibilityTarget
    platformToolset = $PlatformToolset
    vcToolsVersion = $env:VCToolsVersion
    scintillaVersion = $scintillaVersion
    lexillaVersion = $lexillaVersion
} | ConvertTo-Json | Set-Content -LiteralPath $fingerprintPath -Encoding UTF8

Write-Host "Scintilla $scintillaVersion и Lexilla $lexillaVersion подготовлены в $runtimeDir ($CompatibilityTarget)."
Write-Host "Целевые DLL редактора сохранены в $editorRuntimeDir."
