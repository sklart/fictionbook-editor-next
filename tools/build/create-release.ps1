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
    [switch]$WarningsAsErrors,
    [switch]$SkipBuild,
    [switch]$SkipInstaller,
    [switch]$PreserveArtifacts,
    # Общие property handler и MUI уже подготовлены современным этапом.
    [switch]$SkipPropertyHandlerBuild,
    [switch]$SkipFbvVerbMuiBuild,
    [switch]$SkipCommonChecks,
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

if ($ReleaseTag) {
    if ($ReleaseTag -notmatch '^v(?<release>\d+\.\d+\.\d+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?)$') {
        throw "Недопустимый тег релиза: $ReleaseTag"
    }
    $releaseVersion = $Matches.release
    $releaseBaseVersion = $releaseVersion -replace '-.*$', ''
    if ($releaseBaseVersion -ne $version) { throw "Тег релиза '$ReleaseTag' не совпадает с базовой версией '$version'." }
    $tagIsPrerelease = $releaseVersion.Contains('-')
    if ($Prerelease -ne $tagIsPrerelease) { throw 'Параметр Prerelease должен соответствовать ReleaseTag.' }
}
elseif ($Prerelease) { throw "Для предварительного выпуска требуется тег с суффиксом, например v$version-rc.1." }
$architecture = $Platform.ToLowerInvariant()
$artifactCompatibility = if ($CompatibilityTarget -eq "Win7") { "win7-" } else { "" }
$artifactsDir = Join-Path $repoRoot ("out\artifacts\{0}" -f $CompatibilityTarget)
$portableDir = Join-Path $repoRoot ("out\package\{0}\FictionBookEditor" -f $CompatibilityTarget)
$coreDir = Join-Path $repoRoot ("out\stage\Core\{0}" -f $CompatibilityTarget)
$integrationDir = Join-Path $repoRoot ("out\stage\Integration\{0}" -f $CompatibilityTarget)
$installerInputDir = Join-Path $repoRoot ("out\package\{0}\InstallerInput" -f $CompatibilityTarget)
$symbolsDir = Join-Path $repoRoot ("out\package\{0}\symbols" -f $CompatibilityTarget)
$editorRuntimeDirectory = Join-Path $repoRoot ("out\editor-runtime\{0}" -f $CompatibilityTarget)
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
        CompatibilityTarget = $CompatibilityTarget
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
$archHandlerOutputDirectory = Join-Path $repoRoot "out\archhandler\$CompatibilityTarget\Win32\$Configuration"
if (-not $SkipBuild) {
    $archHandlerArguments = @{ OutputDirectory = $archHandlerOutputDirectory }
    if ($PlatformToolset) { $archHandlerArguments.PlatformToolset = $PlatformToolset }
    & (Join-Path $PSScriptRoot "build-archhandler.ps1") @archHandlerArguments
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build-provenance.ps1") -Action Write -Kind $CompatibilityTarget `
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

        & (Join-Path $PSScriptRoot "build-experimental-property-handler.ps1") @propertyHandlerBuildArguments
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
    CompatibilityTarget = $CompatibilityTarget
    SkipUpdateManifest = $true
    BatchOutputDirectory = $batchOutputDirectory
    ArchHandlerOutputDirectory = $archHandlerOutputDirectory
}
if ($PlatformToolset) {
    $verifyReleaseArguments.PlatformToolset = $PlatformToolset
}
if ($SkipCommonChecks) {
    $verifyReleaseArguments.SkipCommonChecks = $true
}
if ($FullValidation) {
    $verifyReleaseArguments.FullValidation = $true
}
if (-not $SkipReleaseVerification) {
    & (Join-Path $PSScriptRoot "verify-release.ps1") @verifyReleaseArguments
}
$stageCoreArguments = @{
    Configuration = $Configuration
    CompatibilityTarget = $CompatibilityTarget
    EditorRuntimeDirectory = $editorRuntimeDirectory
    BatchOutputDirectory = $batchOutputDirectory
    ArchHandlerOutputDirectory = $archHandlerOutputDirectory
    OutputDirectory = $coreDir
}
if ($CompatibilityTarget -eq "Win7") {
    $stageCoreArguments.CommonCoreDirectory = Join-Path $repoRoot "out\stage\Core\Modern"
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

$portableZip = Join-Path $artifactsDir "FictionBookEditorNext-$version-$artifactCompatibility$architecture-portable.zip"
$symbolsZip = Join-Path $artifactsDir "FictionBookEditorNext-$version-$artifactCompatibility$architecture-symbols.zip"
$setupArtifact = Join-Path $artifactsDir "FictionBookEditorNext-$version-$artifactCompatibility$architecture-setup.exe"
$checksumsPath = Join-Path $artifactsDir "SHA256SUMS.txt"

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
# payload and its freshly created ZIP. Win7 validation is intentionally kept
# in its dedicated compatibility contour.
if ($CompatibilityTarget -eq 'Modern') {
    & (Join-Path $repoRoot 'tools\tests\test-portable-package-smoke.ps1') `
        -PackageDirectory $portableDir `
        -PortableZip $portableZip
    & (Join-Path $repoRoot 'tools\tests\test-bundled-plugin-local-activation.ps1') `
        -Configuration $Configuration `
        -RuntimeDirectory $portableDir
    if ($FullValidation) {
        & (Join-Path $repoRoot 'tools\tests\test-portable-gui-state.ps1') -PackageDirectory $portableDir
    }
}

if (Test-Path -LiteralPath $symbolsDir) {
    Remove-PathWithRetry -LiteralPath $symbolsDir
}
New-Item -ItemType Directory -Path $symbolsDir | Out-Null

$symbolNames = @(
    "FBE.pdb",
    "ExportHTML.pdb",
    "ExportDOCX.pdb",
    "ExportEPUB.pdb",
    "ImportEPUB.pdb",
    "ImportEPUBLunaSVG.pdb",
    "ExportDOCXBatch.pdb",
    "ExportEPUBBatch.pdb",
    "ImportEPUBBatch.pdb",
    "FBShell.pdb",
    "res_rus.pdb",
    "res_ukr.pdb"
)
foreach ($name in $symbolNames) {
    $symbolSourcePath = switch ($name) {
        "res_rus.pdb" { Join-Path $repoRoot "out\$Configuration\Lang\ru-RU\$name"; break }
        "res_ukr.pdb" { Join-Path $repoRoot "out\$Configuration\Lang\uk-UA\$name"; break }
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
Отладочные символы FictionBook Editor Next $version

Эти PDB-файлы соответствуют бинарникам релиза $version $architecture.
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

if (-not $SkipInstaller) {
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
        if ($CompatibilityTarget -eq "Win7") {
            $makensisArguments += '/DFBE_WIN7_BUILD=1'
        }
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
}

$artifactFiles = Get-ChildItem -LiteralPath $artifactsDir -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | Sort-Object Name
$checksumLines = foreach ($file in $artifactFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "{0}  {1}" -f $hash.Hash, $file.Name
}
[IO.File]::WriteAllLines($checksumsPath, $checksumLines, [Text.Encoding]::ASCII)
if ($ValidateUpdateManifest) {
    throw "Проверка published update.xml не является частью materialization artifacts. Используйте tools\\build\\new-update-manifest-candidate.ps1 после Modern и Win7 artifacts."
}

$verifyArtifactArguments = @{
    Platform = $Platform
    ArtifactsDirectory = $artifactsDir
    CompatibilityTarget = $CompatibilityTarget
}
if ($SkipInstaller -and -not ($PreserveArtifacts -and $CompatibilityTarget -eq "Win7")) {
    $verifyArtifactArguments.SkipInstaller = $true
}
if (-not $SkipArtifactVerification) {
    & (Join-Path $PSScriptRoot "verify-artifacts.ps1") @verifyArtifactArguments
}

Write-Host "Артефакты релиза для версии ${version}:"
Get-ChildItem -LiteralPath $artifactsDir -File | Select-Object Name, Length | Format-Table -AutoSize
