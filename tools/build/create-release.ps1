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

if ($ReleaseTag -and $ReleaseTag -ne "v$version" -and
    (-not $Prerelease -or -not $ReleaseTag.StartsWith("v$version-", [StringComparison]::OrdinalIgnoreCase))) {
    throw "Тег релиза '$ReleaseTag' не совпадает с версией исходников 'v$version'."
}
if ($Prerelease -and -not $ReleaseTag) {
    throw "Для предварительного выпуска требуется тег с суффиксом, например v$version-rc.1."
}
$architecture = $Platform.ToLowerInvariant()
$artifactCompatibility = if ($CompatibilityTarget -eq "Win7") { "win7-" } else { "" }
$artifactsDir = Join-Path $repoRoot ("out\artifacts\{0}" -f $CompatibilityTarget)
$portableDir = Join-Path $repoRoot ("out\package\{0}\FictionBookEditor" -f $CompatibilityTarget)
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
    & (Join-Path $PSScriptRoot "build.ps1") @buildArguments
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

$verifyReleaseArguments = @{
    Configuration = $Configuration
    CompatibilityTarget = $CompatibilityTarget
    SkipUpdateManifest = $true
    BatchOutputDirectory = $batchOutputDirectory
}
if ($PlatformToolset) {
    $verifyReleaseArguments.PlatformToolset = $PlatformToolset
}
if ($SkipCommonChecks) {
    $verifyReleaseArguments.SkipCommonChecks = $true
}
if (-not $SkipReleaseVerification) {
    & (Join-Path $PSScriptRoot "verify-release.ps1") @verifyReleaseArguments
}
& (Join-Path $PSScriptRoot "package-portable.ps1") `
    -Configuration $Configuration `
    -EditorRuntimeDirectory $editorRuntimeDirectory `
    -BatchOutputDirectory $batchOutputDirectory `
    -PackageDirectory $portableDir `
    -RequireWin32PropertyHandler `
    -RequireX64ShellExtension `
    -SkipFbvVerbMuiBuild:$SkipFbvVerbMuiBuild
& (Join-Path $PSScriptRoot "verify-package-stage.ps1") -StageDirectory $portableDir

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
        -Configuration $Configuration `
        -SkipPortablePackage `
        -SkipPackageVerification `
        -PortableDirectory $portableDir

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
        $makensisArguments = @(
            '/X!addincludedir ..\NSIS',
            '/X!addplugindir /x86-unicode ..\NSIS',
            ('/DINPUTDIR=' + $portableDir),
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
if (-not $SkipInstaller -and $CompatibilityTarget -eq "Modern" -and -not $Prerelease) {
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
    if ($Prerelease) {
        throw "Предварительный выпуск не должен проверять или изменять update.xml."
    }
    & (Join-Path $repoRoot "tools\version\sync-version.ps1") -ValidateUpdateManifest
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
