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

    [ValidateSet("Modern", "Win7")]
    [string]$CompatibilityTarget = "Modern",

    [string]$PlatformToolset,

    [switch]$SkipUpx,

    # Собрать только Scintilla/Lexilla для указанного варианта Windows.
    # Используется release-конвейером после уже выполненной общей сборки.
    [switch]$EditorRuntimeOnly,

    # Повторно собрать только консольные пакетные конвертеры для выбранного
    # варианта Windows, сохранив остальные общие релизные бинарники.
    [switch]$BatchConvertersOnly,

    # Использовать уже подготовленные PCRE2/Hunspell. Режим предназначен для
    # второго (Win7) этапа одного release-конвейера.
    [switch]$SkipDependencies,

    # Диагностический локальный режим. CI всегда полагается на корректный
    # граф MSBuild и не выполняет повторную полную пересборку проектов.
    [switch]$ForceRebuildRequiredProjects,

    [switch]$ReuseEditorRuntime,

    [switch]$ReusePreparedPcre2,

    [switch]$SkipVersionSync,

    # Явный target-specific каталог для EXE/PDB пакетных конвертеров.
    # В CI обязателен, чтобы Modern и Win7 никогда не делили OutDir.
    [string]$BatchOutputDirectory,

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
function Invoke-RequiredProjectBuild {
    param(
        [Parameter(Mandatory)]
        [string]$ProjectPath
    )

    $target = if ($ForceRebuildRequiredProjects) { "Rebuild" } else { "Build" }
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
        (Join-Path $repoRoot "build\hunspell\lib\$Configuration\libhunspell.lib")
    )
    $missing = @($requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
    if ($missing.Count -gt 0) {
        throw ("Нельзя пропустить подготовку зависимостей; отсутствуют: {0}" -f ($missing -join "; "))
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$editorRuntimeDirectory = Join-Path $repoRoot ("out\editor-runtime\{0}" -f $CompatibilityTarget)

if ($EditorRuntimeOnly) {
    . (Join-Path $repoRoot "tools\build\build-scintilla.ps1") `
        -CompatibilityTarget $CompatibilityTarget `
        -PlatformToolset $PlatformToolset `
        -OutputDirectory $editorRuntimeDirectory `
        -ReusePreparedRuntime:$ReuseEditorRuntime
    Write-Host "Собраны только целевые DLL редактора для ${CompatibilityTarget}: $editorRuntimeDirectory"
    return
}

if (-not $SkipVersionSync) {
    & (Join-Path $repoRoot "tools\version\sync-version.ps1")
}

# Общая среда компилятора нужна и PCRE2/Hunspell, и прямым MSBuild-вызовам
# batch-проектов. Для Win7 фиксируем тот же toolset, что и runtime.
$vsEnvironmentArguments = @{ Arch = "x86"; HostArch = "x64"; PlatformToolset = $PlatformToolset }
if ($CompatibilityTarget -eq "Win7") {
    $vsEnvironmentArguments.VcVarsVersion = "14.44"
}
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

function Confirm-FbeLocalizedResourceLibraries {
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    $localizedLibraries = @{
        "ru-RU" = "res_rus.dll"
        "uk-UA" = "res_ukr.dll"
    }

    foreach ($locale in $localizedLibraries.Keys) {
        $libraryName = $localizedLibraries[$locale]
        $localizedPath = Join-Path $OutputDirectory "Lang\\$locale\\$libraryName"
        if (-not (Test-Path -LiteralPath $localizedPath -PathType Leaf)) {
            throw "Не найдена DLL локализованных ресурсов FBE: $localizedPath"
        }

        $legacyRootPath = Join-Path $OutputDirectory $libraryName
        if (Test-Path -LiteralPath $legacyRootPath -PathType Leaf) {
            Remove-Item -LiteralPath $legacyRootPath -Force
            Write-Host "Удалена устаревшая корневая копия DLL локализации: $legacyRootPath"
        }
    }

    Write-Host "Локализованные DLL FBE проверены в каталоге Lang."
}
if ($SkipDependencies) {
    Assert-PreparedDependencies
    Write-Host "Подготовка PCRE2 и Hunspell пропущена: используются проверенные общие библиотеки."
}
else {
    Write-Host "Подготовка PCRE2..."
    & pwsh @pcre2BuildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "Подготовка generated Hunspell project/header..."
    & (Join-Path $repoRoot "tools\build\build-hunspell.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset -PrepareOnly
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

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
    "/p:Platform=$Platform",
    "/p:CompatibilityTarget=$CompatibilityTarget"
)
$buildCommit = (& git -C $repoRoot rev-parse --short=12 HEAD 2>$null | Select-Object -First 1)
if(-not $buildCommit) { $buildCommit = 'unknown' }
$properties += "/p:FbeBuildCommit=$buildCommit"

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

if ($BatchConvertersOnly) {
    foreach ($batchProject in @(
        "src\\export-docx\\ExportDOCXBatch.vcxproj",
        "src\\export-epub\\ExportEPUBBatch.vcxproj",
        "src\\import-epub\\ImportEPUBBatch.vcxproj"
    )) {
        Invoke-RequiredProjectBuild -ProjectPath (Join-Path $repoRoot $batchProject)
    }

    $batchOutputText = if ($BatchOutputDirectory) { " в $BatchOutputDirectory" } else { " в стандартный out\\$Configuration" }
    Write-Host "Собраны только пакетные конвертеры для ${CompatibilityTarget}$batchOutputText."
    return
}

. (Join-Path $repoRoot "tools\build\build-scintilla.ps1") `
    -CompatibilityTarget $CompatibilityTarget `
    -PlatformToolset $PlatformToolset `
    -OutputDirectory $editorRuntimeDirectory `
    -ReusePreparedRuntime:$ReuseEditorRuntime

Export-RuntimeLanguageFiles -OutputDirectory (Join-Path $repoRoot "out\$Configuration")

& $msbuild (Join-Path $repoRoot "FBE.sln") /m /t:Build `
    $properties /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Assert-PreparedDependencies

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

Confirm-FbeLocalizedResourceLibraries -OutputDirectory (Join-Path $repoRoot "out\$Configuration")
Remove-ObsoleteRootLanguageDirectories -OutputDirectory (Join-Path $repoRoot "out\$Configuration")
