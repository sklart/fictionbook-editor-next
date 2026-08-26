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

    [string]$PlatformToolset = "v143",

    [switch]$SkipUpx,

    # Диагностический локальный режим. CI всегда полагается на корректный
    # граф MSBuild и не выполняет повторную полную пересборку проектов.
    [switch]$ForceRebuildRequiredProjects,

    [switch]$ReuseEditorRuntime,

    [switch]$ReusePreparedPcre2,

    [switch]$SkipVersionSync,

    [string]$BatchOutputDirectory,

    # Полная SemVer-identity tagged build (например, 3.2.0-rc.2).
    # Numeric VERSIONINFO остаётся основанным на src/version.h.
    [string]$ReleaseVersion,

    [switch]$WarningsAsErrors
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'UpdateVersion.ps1')

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
function Invoke-RequiredProjectBuild {
    param(
        [Parameter(Mandatory)]
        [string]$ProjectPath,

        # A few projects share generated inputs with the parallel solution
        # build.  Rebuild them serially when their release artifact must be
        # authoritative for the verifier.
        [switch]$Rebuild
    )

    $target = if ($ForceRebuildRequiredProjects -or $Rebuild) { "Rebuild" } else { "Build" }
    Write-Host "Сборка релизного бинарника ($target): $ProjectPath"
    $projectProperties = @($properties) + "/p:SolutionDir=$repoRoot\"
    & $msbuild $ProjectPath /m "/t:$target" $projectProperties /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

function Assert-PreparedDependencies {
    $requiredPaths = @(
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\include\pcre2.h"),
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\lib\pcre2-8-static.lib"),
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\lib\pcre2-posix-static.lib"),
        (Join-Path $repoRoot "build\hunspell\lib\$Configuration\libhunspell.lib"),
        (Join-Path $repoRoot "build\libwebp\install\$Configuration\lib\libwebp.lib"),
        (Join-Path $repoRoot "build\openjpeg\install\$Configuration\lib\openjp2.lib"),
        (Join-Path $repoRoot "build\libheif\install\$Configuration\include\libheif\heif.h"),
        (Join-Path $repoRoot "build\libheif\install\$Configuration\lib\heif.lib"),
        (Join-Path $repoRoot "build\libde265\install\$Configuration\lib\libde265.lib"),
        (Join-Path $repoRoot "build\aom\install\$Configuration\lib\aom.lib")
    )
    $missing = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
    if ($missing.Count -gt 0) {
        throw ("Нельзя пропустить подготовку зависимостей; отсутствуют: {0}" -f ($missing -join "; "))
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

# Use VS 2022 v143 and VC 14.44: newer CRTs import
# GetSystemTimePreciseAsFileTime, which is absent from Windows 7.
$editorRuntimeDirectory = Join-Path $repoRoot "out\editor-runtime"

if (-not $SkipVersionSync) {
    & (Join-Path $repoRoot "tools\version\sync-version.ps1")
}

# Universal release always uses the Win7-compatible VC 14.44 toolset.
$vsEnvironmentArguments = @{ Arch = "x86"; HostArch = "x64"; PlatformToolset = $PlatformToolset; VcVarsVersion = "14.44" }
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") @vsEnvironmentArguments

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
if ($ReusePreparedPcre2) {
    $pcre2BuildArgs += "-ReusePreparedPcre2"
}

function Export-RuntimeLanguageFiles {
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    & (Join-Path $repoRoot "tools\localization\export-runtime-lang.ps1") `
        -RepositoryRoot $repoRoot `
        -OutputDirectory (Join-Path $OutputDirectory "Lang") `
        -Clean

    Write-Host "Runtime-локализация подготовлена рядом с бинарниками: $(Join-Path $OutputDirectory "Lang")"
}

function Remove-ObsoleteRootLanguageDirectories {
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    foreach ($locale in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "pt-PT", "nl-NL", "cs-CZ", "bg-BG")) {
        $legacyDirectory = Join-Path $OutputDirectory $locale
        if (-not (Test-Path -LiteralPath $legacyDirectory -PathType Container)) {
            continue
        }

        if (@(Get-ChildItem -LiteralPath $legacyDirectory -Force).Count -eq 0) {
            Remove-Item -LiteralPath $legacyDirectory -Force
            Write-Host "Удалён пустой устаревший каталог языка: $legacyDirectory"
        }
        else {
            Write-Warning "Сохранён непустой устаревший каталог языка для ручной проверки: $legacyDirectory"
        }
    }
}

if ($ReusePreparedPcre2) {
    $pcreInputs = @(
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\include\pcre2.h"),
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\lib\pcre2-8-static.lib"),
        (Join-Path $repoRoot "build\pcre2\install\$Configuration\lib\pcre2-posix-static.lib")
    )
    if (@($pcreInputs | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -gt 0) {
        throw 'PCRE2 cache was reported as reusable, but its required files are missing.'
    }
    Write-Host "Используется только проверенный PCRE2 cache; остальные зависимости будут подготовлены отдельно."
}
else {
    Write-Host "Подготовка PCRE2..."
    & pwsh @pcre2BuildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
Write-Host "Подготовка generated Hunspell project/header..."
& (Join-Path $repoRoot "tools\build\build-hunspell.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset -PrepareOnly
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $repoRoot "tools\build\build-libwebp.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $repoRoot "tools\build\build-openjpeg.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $repoRoot "tools\build\build-libheif.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

# v143 is supplied by the VS 2022 toolchain. Do not let a newer Visual Studio
# instance win the generic -latest query: it can have the compiler but not the
# matching ATL/MFC headers for the requested toolset.
$vswhereArguments = @("-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild")
if ($PlatformToolset -eq "v143") {
    $vswhereArguments += @("-version", "[17.0,18.0)")
}
$vswhereArguments += @("-find", "MSBuild\Current\Bin\MSBuild.exe")
$msbuild = & $vswhere @vswhereArguments | Select-Object -First 1

if (-not $msbuild) {
    throw "Не найден MSBuild.exe."
}

$properties = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)
$buildCommit = (& git -C $repoRoot rev-parse --short=12 HEAD 2>$null | Select-Object -First 1)
if(-not $buildCommit) { $buildCommit = 'unknown' }
$properties += "/p:FbeBuildCommit=$buildCommit"
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
$baseMatch = [regex]::Match($versionHeader, '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')
if (-not $baseMatch.Success) { throw 'Не найден FBE_VERSION_STRING.' }
if (-not $ReleaseVersion) { $ReleaseVersion = $baseMatch.Groups['version'].Value }
if (-not (Test-FbeSemVer $ReleaseVersion)) {
    throw "Недопустимая release version: $ReleaseVersion"
}
$releaseBaseVersion = $ReleaseVersion -replace '[-+].*$', ''
if ($releaseBaseVersion -ne $baseMatch.Groups['version'].Value) {
    throw "Release version $ReleaseVersion не соответствует базовой версии $($baseMatch.Groups['version'].Value)."
}
$properties += "/p:FbeReleaseVersion=$ReleaseVersion"

if ($BatchOutputDirectory) {
    $BatchOutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
    New-Item -ItemType Directory -Path $BatchOutputDirectory -Force | Out-Null
    $properties += "/p:BatchOutputDirectory=$BatchOutputDirectory\"
}

if ($PlatformToolset) {
    $properties += "/p:PlatformToolset=$PlatformToolset"
}

if ($SkipUpx) {
    $properties += "/p:EnableUpx=false"
}

if ($WarningsAsErrors) {
    $properties += "/p:TreatWarningAsError=true"
}

. (Join-Path $repoRoot "tools\build\build-scintilla.ps1") `
    -PlatformToolset $PlatformToolset `
    -OutputDirectory $editorRuntimeDirectory `
    -ReusePreparedRuntime:$ReuseEditorRuntime

Export-RuntimeLanguageFiles -OutputDirectory (Join-Path $repoRoot "out\$Configuration")

# A partial or interrupted build must never leave the previous ImportEPUB
# output looking authoritative.
$commonOutput = Join-Path $repoRoot "out\$Configuration"
foreach ($name in @('ImportEPUB.dll', 'ImportEPUB.pdb', 'ImportEPUB.lib', 'ImportEPUB.exp')) {
    Remove-Item -LiteralPath (Join-Path $commonOutput $name) -Force -ErrorAction SilentlyContinue
}

& $msbuild (Join-Path $repoRoot "FBE.sln") /m /t:Build `
    $properties /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Assert-PreparedDependencies

# ImportEPUB is built by the solution, but its generated resource input can
# race with the parallel build graph.  A serial final rebuild makes the DLL
# consumed by the COM/version verifier deterministic.
Invoke-RequiredProjectBuild -ProjectPath (Join-Path $repoRoot "src\import-epub\ImportEPUB.vcxproj") -Rebuild

# Эти проекты не входят в FBE.sln. Остальные результаты даёт единственный
# solution Build; повторный Rebuild доступен только локально по явному ключу.
foreach ($requiredProject in @(
    "src\export-docx\ExportDOCXBatch.vcxproj",
    "src\export-epub\ExportEPUBBatch.vcxproj",
    "src\import-epub\ImportEPUBBatch.vcxproj",
    # The SVG adapter links these two libraries but does not declare MSBuild
    # project references to them.  Build them explicitly so a clean CI runner
    # never attempts to link ImportEPUBLunaSVG.dll before its libraries exist.
    "src\import-epub\thirdparty\lunasvg\plutovg.vcxproj",
    "src\import-epub\thirdparty\lunasvg\lunasvg.vcxproj",
    "src\import-epub\ImportEPUBLunaSVG.vcxproj"
)) {
    Invoke-RequiredProjectBuild -ProjectPath (Join-Path $repoRoot $requiredProject)
}

Remove-ObsoleteReleaseArtifacts -OutputDirectory (Join-Path $repoRoot "out\$Configuration")

Remove-ObsoleteRootLanguageDirectories -OutputDirectory (Join-Path $repoRoot "out\$Configuration")

if ($true) {
    & (Join-Path $repoRoot 'tools\build\build-provenance.ps1') -Action Write -Kind CommonCore `
        -Configuration $Configuration -CommonDirectory (Join-Path $repoRoot "out\$Configuration") -PlatformToolset $PlatformToolset
}
