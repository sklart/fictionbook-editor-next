[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32")]
    [string]$Platform = "Win32",

    [string]$PlatformToolset = 'v143',

    [switch]$SkipUpx,
    [switch]$WarningsAsErrors,
    [switch]$SkipBuild,
    [switch]$PreserveArtifacts,
    # Общие property handler и MUI уже подготовлены современным этапом.
    [switch]$SkipPropertyHandlerBuild,
    [switch]$SkipFbvVerbMuiBuild,
    [string]$BatchOutputDirectory,
    [switch]$SkipArtifactVerification,
    [switch]$SkipReleaseVerification,
    [switch]$SkipVersionSync,
    [switch]$FullValidation,
    [switch]$ValidateUpdateManifest,
    [switch]$Prerelease,
    [string]$ReleaseTag
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'UpdateVersion.ps1')

function Remove-PathWithRetry {
    param(
        [Parameter(Mandatory)]
        [string]$LiteralPath,
        [int]$Attempts = 10,
        [int]$DelayMilliseconds = 500
    )

    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        try {
            Remove-Item -Recurse -Force -LiteralPath $LiteralPath -ErrorAction Stop
            return
        }
        catch {
            if ($attempt -eq $Attempts) {
                throw
            }
            Start-Sleep -Milliseconds $DelayMilliseconds
        }
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionHeader = Join-Path $repoRoot "src\version.h"
$versionText = Get-Content -Raw -LiteralPath $versionHeader
$versionMatch = [regex]::Match(
    $versionText,
    '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"'
)

if (-not $versionMatch.Success) {
    throw "В $versionHeader не найден FBE_VERSION_STRING."
}

$version = $versionMatch.Groups["version"].Value
$releaseVersion = $version
$legacy308MigrationRequired = $false

if ($ReleaseTag) {
    if (-not (Test-FbeReleaseTag $ReleaseTag)) {
        throw "Недопустимый тег релиза: $ReleaseTag"
    }
    $releaseVersion = $ReleaseTag.Substring(1)
    $releaseBaseVersion = Get-FbeBaseVersion $releaseVersion
    if ($releaseBaseVersion -ne $version) { throw "Тег релиза '$ReleaseTag' не совпадает с базовой версией '$version'." }
    $tagIsPrerelease = Test-FbePrereleaseVersion $releaseVersion
    if ($Prerelease -ne $tagIsPrerelease) { throw 'Параметр Prerelease должен соответствовать ReleaseTag.' }
    $legacy308MigrationRequired = Test-FbeLegacy308MigrationRequired $releaseVersion
}
elseif ($Prerelease) { throw "Для предварительного выпуска требуется тег с суффиксом, например v$version-rc.1." }
$architecture = $Platform.ToLowerInvariant()
$assetVersion = Get-FbeAssetVersion $releaseVersion
$artifactsDir = Join-Path $repoRoot "out\artifacts"
$portableDir = Join-Path $repoRoot "out\package\FictionBookEditor"
$coreDir = Join-Path $repoRoot "out\stage\Core"
$integrationDir = Join-Path $repoRoot "out\stage\Integration"
$installerInputDir = Join-Path $repoRoot "out\package\InstallerInput"
$symbolsDir = Join-Path $repoRoot "out\package\symbols"
$editorRuntimeDirectory = Join-Path $repoRoot "out\editor-runtime"
$batchOutputDirectory = if ($BatchOutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
} else {
    Join-Path $repoRoot ("out\{0}" -f $Configuration)
}

# До упаковки синхронизируется только version.nsh. Проверка update.xml должна
# выполняться в конце: SHA-256 нового setup появляется лишь после его сборки.
if (-not $SkipVersionSync) {
    & (Join-Path $repoRoot "tools\version\sync-version.ps1")
}

if (-not $SkipBuild) {
    $buildArguments = @{
        Configuration = $Configuration
        Platform = $Platform
        BatchOutputDirectory = $batchOutputDirectory
    }
    if ($PlatformToolset) {
        $buildArguments.PlatformToolset = $PlatformToolset
    }
    if ($SkipUpx) {
        $buildArguments.SkipUpx = $true
    }
    if ($WarningsAsErrors) {
        $buildArguments.WarningsAsErrors = $true
    }
	if ($ReleaseTag) {
		$buildArguments.ReleaseVersion = $releaseVersion
	}
    & (Join-Path $PSScriptRoot "build.ps1") @buildArguments
}
$archHandlerOutputDirectory = Join-Path $repoRoot "out\archhandler\Win32\$Configuration"
if (-not $SkipBuild) {
    $archHandlerArguments = @{ OutputDirectory = $archHandlerOutputDirectory }
    if ($PlatformToolset) { $archHandlerArguments.PlatformToolset = $PlatformToolset }
    & (Join-Path $PSScriptRoot "build-archhandler.ps1") @archHandlerArguments
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build-provenance.ps1") -Action Write -Kind Runtime `
        -Configuration $Configuration -ProfileDirectory $editorRuntimeDirectory `
        -BatchDirectory $batchOutputDirectory -ArchHandlerDirectory $archHandlerOutputDirectory `
        -PlatformToolset $PlatformToolset
}

if ($SkipBuild -and -not $SkipPropertyHandlerBuild) {
    throw "-SkipBuild запрещает native-компиляцию. Подготовьте property handler заранее и укажите -SkipPropertyHandlerBuild."
}
if ($SkipBuild -and -not $SkipFbvVerbMuiBuild) {
    throw "-SkipBuild запрещает native-компиляцию. Подготовьте FBV Verb MUI заранее и укажите -SkipFbvVerbMuiBuild."
}

if ($SkipPropertyHandlerBuild) {
    foreach ($propertyHandlerPlatform in @("Win32", "x64")) {
        $propertyHandlerOutput = Join-Path $repoRoot "out\package\shell-build\$propertyHandlerPlatform\$Configuration\FBShell.dll"
        if (-not (Test-Path -LiteralPath $propertyHandlerOutput -PathType Leaf)) {
            throw "Нельзя пропустить сборку property handler: отсутствует $propertyHandlerOutput"
        }
        $propertyHandlerSymbols = Join-Path $repoRoot "out\package\shell-build\$propertyHandlerPlatform\$Configuration\FBShell.pdb"
        if (-not (Test-Path -LiteralPath $propertyHandlerSymbols -PathType Leaf)) {
            throw "Нельзя пропустить сборку property handler: отсутствуют символы $propertyHandlerSymbols"
        }
    }
    Write-Host "Повторная сборка property handler пропущена: используются общие подготовленные DLL."
}
else {
    foreach ($propertyHandlerPlatform in @("Win32", "x64")) {
        $propertyHandlerBuildArguments = @{
            Configuration = $Configuration
            Platform = $propertyHandlerPlatform
            SkipUpx = $SkipUpx
        }
        if ($PlatformToolset) {
            $propertyHandlerBuildArguments.PlatformToolset = $PlatformToolset
        }

        & (Join-Path $PSScriptRoot "build-shell-integration.ps1") @propertyHandlerBuildArguments
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

# Shell-команда FBV использует отдельный MUI-host. Он не входит в FBE.sln,
# поэтому обязан собираться здесь до verify-release и stage-integration.
if (-not $SkipFbvVerbMuiBuild) {
    $fbvVerbMuiBuildArguments = @{ Configuration = $Configuration }
    if ($PlatformToolset) {
        $fbvVerbMuiBuildArguments.PlatformToolset = $PlatformToolset
    }
    & (Join-Path $PSScriptRoot "build-fbv-verb-mui.ps1") @fbvVerbMuiBuildArguments
}
else {
    $fbvVerbMuiHost = Join-Path $repoRoot "out\$Configuration\Lang\Shell\FBVVerbResources.dll"
    if (-not (Test-Path -LiteralPath $fbvVerbMuiHost -PathType Leaf)) {
        throw "Нельзя пропустить сборку FBV Verb MUI: отсутствует $fbvVerbMuiHost"
    }
    Write-Host "Повторная сборка FBV Verb MUI пропущена: используется подготовленный ресурс."
}

$verifyReleaseArguments = @{
    Configuration = $Configuration
    SkipUpdateManifest = $true
    BatchOutputDirectory = $batchOutputDirectory
    ArchHandlerOutputDirectory = $archHandlerOutputDirectory
}
if ($PlatformToolset) {
    $verifyReleaseArguments.PlatformToolset = $PlatformToolset
}
if ($FullValidation) {
    $verifyReleaseArguments.FullValidation = $true
}
if (-not $SkipReleaseVerification) {
    & (Join-Path $PSScriptRoot "verify-release.ps1") @verifyReleaseArguments
}
$stageCoreArguments = @{
    Configuration = $Configuration
    EditorRuntimeDirectory = $editorRuntimeDirectory
    BatchOutputDirectory = $batchOutputDirectory
    ArchHandlerOutputDirectory = $archHandlerOutputDirectory
    OutputDirectory = $coreDir
}
& (Join-Path $PSScriptRoot "stage-core.ps1") @stageCoreArguments
& (Join-Path $PSScriptRoot "stage-integration.ps1") `
    -Configuration $Configuration `
    -OutputDirectory $integrationDir
& (Join-Path $PSScriptRoot "package-portable.ps1") `
    -CoreDirectory $coreDir `
    -OutputDirectory $portableDir
foreach ($name in @('ZipHandler.exe', 'RarHandler.exe')) {
    $builtArtifact = Join-Path $archHandlerOutputDirectory $name
    $packagedArtifact = Join-Path $portableDir "Utilities\ArchHandler\$name"
    if ((Get-FileHash -LiteralPath $builtArtifact -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $packagedArtifact -Algorithm SHA256).Hash) {
        throw "Упакованный ArchHandler не совпадает с проверенным artifact: $name"
    }
}

if ((Test-Path -LiteralPath $artifactsDir) -and -not $PreserveArtifacts) {
    try {
        Remove-PathWithRetry -LiteralPath $artifactsDir
    }
    catch {
        $artifactsDir = "{0}-{1}" -f $artifactsDir, (Get-Date -Format "yyyyMMdd-HHmmss")
        Write-Warning "Предыдущие артефакты заблокированы; использую $artifactsDir"
    }
}
New-Item -ItemType Directory -Path $artifactsDir -Force | Out-Null

$portableZip = Join-Path $artifactsDir "FictionBookEditorNext-$assetVersion-$architecture-portable.zip"
$symbolsZip = Join-Path $artifactsDir "FictionBookEditorNext-$assetVersion-$architecture-symbols.zip"
$setupArtifact = Join-Path $artifactsDir "FictionBookEditorNext-$assetVersion-$architecture-setup.exe"
$checksumsPath = Join-Path $artifactsDir "SHA256SUMS.txt"
$legacyWin7Setup = Join-Path $artifactsDir "FictionBookEditorNext-$assetVersion-win7-$architecture-setup.exe"
$legacyWin7Portable = Join-Path $artifactsDir "FictionBookEditorNext-$assetVersion-win7-$architecture-portable.zip"

foreach ($artifactPath in @($portableZip, $symbolsZip)) {
    if (Test-Path -LiteralPath $artifactPath) {
        Remove-PathWithRetry -LiteralPath $artifactPath
    }
}

function Set-ManifestNodeValue {
    param(
        [Parameter(Mandatory)]
        [xml]$Document,

        [Parameter(Mandatory)]
        [string]$NodeName,

        [Parameter(Mandatory)]
        [string]$Value
    )

    $nodes = @($Document.FBE.SelectNodes($NodeName))
    if ($nodes.Count -ne 1) {
        throw "Файл update.xml должен содержать ровно один элемент <$NodeName>."
    }

    $nodes[0].InnerText = $Value
}

Compress-Archive -Path (Join-Path $portableDir "*") -DestinationPath $portableZip -CompressionLevel Optimal

# The portable acceptance scenario is exercised against the materialised
# payload and its freshly created ZIP.
& (Join-Path $repoRoot 'tools\tests\test-portable-package-smoke.ps1') `
    -PackageDirectory $portableDir `
    -PortableZip $portableZip
& (Join-Path $repoRoot 'tools\tests\test-bundled-plugin-local-activation.ps1') `
    -Configuration $Configuration `
    -RuntimeDirectory $portableDir
if ($FullValidation) {
    & (Join-Path $repoRoot 'tools\tests\test-portable-gui-state.ps1') -PackageDirectory $portableDir
}

if (Test-Path -LiteralPath $symbolsDir) {
    Remove-PathWithRetry -LiteralPath $symbolsDir
}
New-Item -ItemType Directory -Path $symbolsDir | Out-Null

$symbolNames = @(
    "FBE.pdb",
    "FBV.pdb",
    "ExportHTML.pdb",
    "ExportDOCX.pdb",
    "ExportEPUB.pdb",
    "ImportEPUB.pdb",
    "ImportEPUBLunaSVG.pdb",
    "ExportDOCXBatch.pdb",
    "ExportEPUBBatch.pdb",
    "ImportEPUBBatch.pdb",
    "FBShell.pdb"
)
foreach ($name in $symbolNames) {
    $symbolSourcePath = switch ($name) {
        default {
            $symbolSourceDirectory = if ($name -in @("ExportDOCXBatch.pdb", "ExportEPUBBatch.pdb", "ImportEPUBBatch.pdb")) { $batchOutputDirectory } else { Join-Path $repoRoot "out\$Configuration" }
            Join-Path $symbolSourceDirectory $name
        }
    }
    Copy-Item -LiteralPath $symbolSourcePath `
        -Destination $symbolsDir -Force
}
Copy-Item -LiteralPath (Join-Path $repoRoot "out\package\shell-build\Win32\$Configuration\FBShell.pdb") `
    -Destination (Join-Path $symbolsDir "FBShell.propertyhandler.win32.pdb") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "out\package\shell-build\x64\$Configuration\FBShell.pdb") `
    -Destination (Join-Path $symbolsDir "FBShell.propertyhandler.x64.pdb") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "out\package\shell-build\x64\$Configuration\FBShell.pdb") `
    -Destination (Join-Path $symbolsDir "FBShell.x64.pdb") -Force

$symbolReadme = @"
Отладочные символы FictionBook Editor Next $releaseVersion

Эти PDB-файлы соответствуют бинарникам релиза $releaseVersion $architecture.
FBShell.propertyhandler.win32.pdb соответствует поставляемому modern property
handler для Win32 Explorer на x86.
FBShell.propertyhandler.x64.pdb соответствует поставляемому modern property
handler для 64-bit Explorer.
При разборе minidump-файлов из %LOCALAPPDATA%\FBE\Crashes храните этот пакет
рядом с подходящим установщиком или portable-архивом того же релиза.
"@
[IO.File]::WriteAllText(
    (Join-Path $symbolsDir "README.txt"),
    $symbolReadme,
    [Text.UTF8Encoding]::new($false)
)
Compress-Archive -Path (Join-Path $symbolsDir "*") -DestinationPath $symbolsZip -CompressionLevel Optimal

& (Join-Path $PSScriptRoot "prepare-installer.ps1") `
        -CoreDirectory $coreDir `
        -IntegrationDirectory $integrationDir `
        -OutputDirectory $installerInputDir

    $makensis = & (Join-Path $PSScriptRoot 'resolve-nsis.ps1')

    $installerDir = Join-Path $repoRoot "packaging\nsis\Installer"
    Get-ChildItem -LiteralPath $installerDir -Filter "FictionBook Editor Next Release $version*.exe" |
        ForEach-Object { Remove-PathWithRetry -LiteralPath $_.FullName }
    if (Test-Path -LiteralPath $setupArtifact) {
        Remove-PathWithRetry -LiteralPath $setupArtifact
    }

    Push-Location $installerDir
    try {
        $makensisArguments = @(
            '/X!addincludedir ..\NSIS',
            '/X!addplugindir /x86-unicode ..\NSIS',
            ('/DINPUTDIR=' + $installerInputDir),
            ('/DOUTPUTFILE=' + $setupArtifact)
        )
        $makensisArguments += 'MakeInstaller.nsi'
        & $makensis @makensisArguments
        if ($LASTEXITCODE -ne 0) {
            throw "NSIS завершился с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }

if (-not (Test-Path -LiteralPath $setupArtifact)) {
    throw "NSIS завершился без создания инсталлятора по пути $setupArtifact."
}

if ($legacy308MigrationRequired) {
    # Compatibility bridge for the published v3.0.8-rc.1 Win7 updater.
    # These are copies, never separately built artifacts.
    Copy-Item -LiteralPath $setupArtifact -Destination $legacyWin7Setup -Force
    Copy-Item -LiteralPath $portableZip -Destination $legacyWin7Portable -Force
    foreach ($pair in @(@($setupArtifact, $legacyWin7Setup), @($portableZip, $legacyWin7Portable))) {
        if ((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash) {
            throw 'Legacy Win7 migration alias does not match the universal artifact.'
        }
    }
}

$artifactFiles = Get-ChildItem -LiteralPath $artifactsDir -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | Sort-Object Name
$checksumLines = foreach ($file in $artifactFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "{0}  {1}" -f $hash.Hash, $file.Name
}
[IO.File]::WriteAllLines($checksumsPath, $checksumLines, [Text.Encoding]::ASCII)
if ($ValidateUpdateManifest) {
    throw "Проверка published update.xml не является частью materialization artifacts. Используйте tools\\build\\new-update-manifest-candidate.ps1 после создания единого набора artifacts."
}

$verifyArtifactArguments = @{
    Platform = $Platform
    ArtifactsDirectory = $artifactsDir
}
if ($ReleaseTag) { $verifyArtifactArguments.ReleaseTag = $ReleaseTag }
if ($legacy308MigrationRequired) { $verifyArtifactArguments.AllowLegacyWin7Aliases = $true }
if (-not $SkipArtifactVerification) {
    & (Join-Path $PSScriptRoot "verify-artifacts.ps1") @verifyArtifactArguments
}

Write-Host "Артефакты релиза для версии ${releaseVersion}:"
Get-ChildItem -LiteralPath $artifactsDir -File | Select-Object Name, Length | Format-Table -AutoSize
