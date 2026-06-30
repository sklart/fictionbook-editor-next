<#
.SYNOPSIS
Собирает основной solution FBE и предварительно подготавливает ключевые зависимости.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32")]
    [string]$Platform = "Win32",

    [string]$PlatformToolset,

    [switch]$SkipUpx,

    [switch]$WarningsAsErrors
)

$ErrorActionPreference = "Stop"

function Remove-ObsoleteReleaseArtifacts {
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    foreach ($name in @(
        "pcre.dll",
        "ExportHTML.exp",
        "ExportHTML.lib",
        "ExportDOCX.exp",
        "ExportDOCX.lib",
        "ExportEPUB.exp",
        "ExportEPUB.lib",
        "ImportEPUB.exp",
        "ImportEPUB.lib",
        "FBShell.exp",
        "FBShell.lib",
        "FBE.exe.manifest"
    )) {
        $path = Join-Path $OutputDirectory $name
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
            Write-Host "Удалён лишний релизный артефакт: $path"
        }
    }
}
function Invoke-RequiredProjectRebuild {
    param(
        [Parameter(Mandatory)]
        [string]$ProjectPath
    )

    Write-Host "Пересборка релизного бинарника: $ProjectPath"
    $projectProperties = @($properties) + "/p:SolutionDir=$repoRoot\"
    & $msbuild $ProjectPath /m /t:Rebuild $projectProperties /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

& (Join-Path $repoRoot "tools\version\sync-version.ps1")
& (Join-Path $repoRoot "tools\build\build-scintilla.ps1")
Write-Host "Подготовка PCRE2..."
$pcre2BuildScript = Join-Path $repoRoot "tools\build\build-pcre2.ps1"
$pcre2BuildArgs = @(
    "-NoProfile",
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    $pcre2BuildScript,
    "-Configuration",
    $Configuration
)
if ($PlatformToolset) {
    $pcre2BuildArgs += @("-PlatformToolset", $PlatformToolset)
}
& pwsh @pcre2BuildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "build\pcre2\install\$Configuration\include\pcre2.h"))) {
    throw "PCRE2 не подготовлен: отсутствует build\pcre2\install\$Configuration\include\pcre2.h"
}
Write-Host "Подготовка Hunspell..."
& (Join-Path $repoRoot "tools\build\build-hunspell.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find "MSBuild\Current\Bin\MSBuild.exe" | Select-Object -First 1

if (-not $msbuild) {
    throw "Не найден MSBuild.exe."
}

$properties = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)

if ($PlatformToolset) {
    $properties += "/p:PlatformToolset=$PlatformToolset"
}

if ($SkipUpx) {
    $properties += "/p:EnableUpx=false"
}

if ($WarningsAsErrors) {
    $properties += "/p:TreatWarningAsError=true"
}

& $msbuild (Join-Path $repoRoot "FBE.sln") /m /t:Build `
    $properties /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# Эти проекты дают локальные релизные бинарники. Явный Rebuild защищает
# release-gate от устаревших файлов, которые общий solution build может оставить
# из-за некорректного incremental-состояния или старой runtime-копии.
foreach ($requiredProject in @(
    "src\export-docx\ExportDOCX.vcxproj",
    "src\export-epub\ExportEPUB.vcxproj",
    "src\fbv\FBV.vcxproj",
    "src\import-epub\ImportEPUB.vcxproj",
    "src\export-docx\ExportDOCXBatch.vcxproj",
    "src\export-epub\ExportEPUBBatch.vcxproj",
    "src\import-epub\ImportEPUBBatch.vcxproj",
    "src\import-epub\ImportEPUBLunaSVG.vcxproj"
)) {
    Invoke-RequiredProjectRebuild -ProjectPath (Join-Path $repoRoot $requiredProject)
}

Remove-ObsoleteReleaseArtifacts -OutputDirectory (Join-Path $repoRoot "out\$Configuration")

