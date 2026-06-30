[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32")]
    [string]$Platform = "Win32",

    [string]$PlatformToolset,

    [switch]$SkipUpx,
    [switch]$WarningsAsErrors,
    [switch]$SkipBuild,
    [switch]$SkipInstaller,
    [switch]$ValidateUpdateManifest,
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

if ($ReleaseTag -and $ReleaseTag -ne "v$version") {
    throw "Тег релиза '$ReleaseTag' не совпадает с версией исходников 'v$version'."
}
$architecture = $Platform.ToLowerInvariant()
$artifactsDir = Join-Path $repoRoot "out\artifacts"
$portableDir = Join-Path $repoRoot "out\package\FictionBookEditor"
$symbolsDir = Join-Path $repoRoot "out\package\symbols"

$syncArguments = @{}
if ($ValidateUpdateManifest) {
    $syncArguments.ValidateUpdateManifest = $true
}
& (Join-Path $repoRoot "tools\version\sync-version.ps1") @syncArguments

if (-not $SkipBuild) {
    $buildArguments = @{
        Configuration = $Configuration
        Platform = $Platform
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
    & (Join-Path $PSScriptRoot "build.ps1") @buildArguments
}

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

& (Join-Path $PSScriptRoot "verify-release.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-scintilla.ps1")
& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1") -Configuration $Configuration
& (Join-Path $PSScriptRoot "package-portable.ps1") `
    -Configuration $Configuration `
    -RequireWin32PropertyHandler `
    -RequireX64ShellExtension
& (Join-Path $PSScriptRoot "verify-package-stage.ps1")
& (Join-Path $PSScriptRoot "verify-nsis-layout.ps1")

if (Test-Path -LiteralPath $artifactsDir) {
    try {
        Remove-PathWithRetry -LiteralPath $artifactsDir
    }
    catch {
        $artifactsDir = "{0}-{1}" -f $artifactsDir, (Get-Date -Format "yyyyMMdd-HHmmss")
        Write-Warning "Предыдущие артефакты заблокированы; использую $artifactsDir"
    }
}
New-Item -ItemType Directory -Path $artifactsDir | Out-Null

$portableZip = Join-Path $artifactsDir "FictionBookEditorNext-$version-$architecture-portable.zip"
$symbolsZip = Join-Path $artifactsDir "FictionBookEditorNext-$version-$architecture-symbols.zip"
$setupArtifact = Join-Path $artifactsDir "FictionBookEditorNext-$version-$architecture-setup.exe"
$checksumsPath = Join-Path $artifactsDir "SHA256SUMS.txt"

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
Copy-Item -LiteralPath (Join-Path $repoRoot "out\$Configuration\$name") `
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
    & (Join-Path $PSScriptRoot "prepare-installer.ps1") -Configuration $Configuration -SkipPortablePackage

    $makensisCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "NSIS\Unicode\makensis.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "NSIS\makensis.exe")
    )
    $makensis = $makensisCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

    if (-not $makensis) {
        throw "Не найден makensis.exe. Установите NSIS для сборки setup-артефакта."
    }

    $installerDir = Join-Path $repoRoot "packaging\nsis\Installer"
    Get-ChildItem -LiteralPath $installerDir -Filter "FictionBook Editor Next Release $version*.exe" |
        ForEach-Object { Remove-PathWithRetry -LiteralPath $_.FullName }
    if (Test-Path -LiteralPath $setupArtifact) {
        Remove-PathWithRetry -LiteralPath $setupArtifact
    }

    Push-Location $installerDir
    try {
        & $makensis '/X!addincludedir ..\NSIS' '/X!addplugindir /x86-unicode ..\NSIS' ('/DINPUTDIR=' + $portableDir) ('/DOUTPUTFILE=' + $setupArtifact) 'MakeInstaller.nsi'
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

$artifactFiles = Get-ChildItem -LiteralPath $artifactsDir -File | Sort-Object Name
$checksumLines = foreach ($file in $artifactFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "{0}  {1}" -f $hash.Hash, $file.Name
}
[IO.File]::WriteAllLines($checksumsPath, $checksumLines, [Text.Encoding]::ASCII)
if (-not $SkipInstaller) {
    $setupHash = (Get-FileHash -LiteralPath $setupArtifact -Algorithm SHA256).Hash
    $updateManifestPath = Join-Path $repoRoot "update.xml"
    [xml]$manifest = Get-Content -Raw -LiteralPath $updateManifestPath
    if (-not $manifest.FBE) {
        throw "Файл update.xml должен содержать корневой элемент <FBE>."
    }

    Set-ManifestNodeValue -Document $manifest -NodeName "Name" `
        -Value "FictionBook Editor Next Release $version"
    Set-ManifestNodeValue -Document $manifest -NodeName "Date" `
        -Value (Get-Date -Format "dd-MM-yyyy")
    Set-ManifestNodeValue -Document $manifest -NodeName "Version" `
        -Value $version
    Set-ManifestNodeValue -Document $manifest -NodeName "Beta" `
        -Value "false"
    Set-ManifestNodeValue -Document $manifest -NodeName "DownloadUrl" `
        -Value "https://github.com/sklart/fictionbook-editor-next/releases/download/v$version/FictionBookEditorNext-$version-win32-setup.exe"
    Set-ManifestNodeValue -Document $manifest -NodeName "SHA256" `
        -Value $setupHash

    $settings = New-Object System.Xml.XmlWriterSettings
    $settings.Encoding = [Text.UTF8Encoding]::new($false)
    $settings.Indent = $true
    $settings.IndentChars = "`t"
    $settings.NewLineChars = "`r`n"
    $settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace

    $writer = [System.Xml.XmlWriter]::Create($updateManifestPath, $settings)
    try {
        $manifest.Save($writer)
    }
    finally {
        $writer.Dispose()
    }
}

if ($ValidateUpdateManifest) {
    & (Join-Path $repoRoot "tools\version\sync-version.ps1") -ValidateUpdateManifest
}

$verifyArtifactArguments = @{
    Platform = $Platform
    ArtifactsDirectory = $artifactsDir
}
if ($SkipInstaller) {
    $verifyArtifactArguments.SkipInstaller = $true
}
& (Join-Path $PSScriptRoot "verify-artifacts.ps1") @verifyArtifactArguments

Write-Host "Артефакты релиза для версии ${version}:"
Get-ChildItem -LiteralPath $artifactsDir -File | Select-Object Name, Length | Format-Table -AutoSize
