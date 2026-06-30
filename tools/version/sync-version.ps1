[CmdletBinding()]
param(
    [switch]$ValidateUpdateManifest
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionHeader = Join-Path $repoRoot "src\version.h"
$versionNsh = Join-Path $repoRoot "packaging\nsis\Installer\version.nsh"
$updateManifest = Join-Path $repoRoot "update.xml"

$header = Get-Content -Raw -LiteralPath $versionHeader
$match = [regex]::Match($header, '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')

if (-not $match.Success) {
    throw "Не удалось найти FBE_VERSION_STRING в $versionHeader"
}

$version = $match.Groups["version"].Value
$expectedManifestName = "FictionBook Editor Next Release $version"
$expectedDownloadUrl =
    "https://github.com/sklart/fictionbook-editor-next/releases/download/" +
    "v$version/FictionBookEditorNext-$version-win32-setup.exe"

function Test-ManifestDateFormat {
    param(
        [Parameter(Mandatory)]
        [string]$Value
    )

    try {
        [void][DateTime]::ParseExact(
            $Value,
            "dd-MM-yyyy",
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::None)
        return $true
    }
    catch {
        return $false
    }
}
$nsh = @"
; Сгенерировано tools/version/sync-version.ps1 на основе src/version.h.
!define PRODUCT_VER_NUM "$version"
"@

[IO.File]::WriteAllText($versionNsh, $nsh, [Text.UTF8Encoding]::new($false))

if ($ValidateUpdateManifest) {
    [xml]$manifest = Get-Content -Raw -LiteralPath $updateManifest
    if (-not $manifest.FBE) {
        throw "update.xml должен содержать корневой элемент <FBE>"
    }

    $publishedName = [string]$manifest.FBE.Name
    if ($publishedName -ne $expectedManifestName) {
        throw "Поле Name в update.xml должно быть равно '$expectedManifestName'"
    }

    $publishedDate = [string]$manifest.FBE.Date
    if (-not (Test-ManifestDateFormat -Value $publishedDate)) {
        throw "Поле Date в update.xml должно использовать формат dd-MM-yyyy"
    }

    $publishedVersion = [string]$manifest.FBE.Version

    if ($publishedVersion -ne $version) {
        throw "В update.xml опубликована версия $publishedVersion, а в src/version.h указана $version"
    }

    $beta = [string]$manifest.FBE.Beta
    if ($beta -notin @("false", "true")) {
        throw "Поле Beta в update.xml должно быть либо 'false', либо 'true'"
    }
    if ($beta -ne "false") {
        throw "Для текущего релизного процесса поле Beta в update.xml должно оставаться 'false'"
    }

    $downloadUrl = [string]$manifest.FBE.DownloadUrl
    if (-not $downloadUrl.Equals($expectedDownloadUrl, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Поле DownloadUrl в update.xml должно быть равно '$expectedDownloadUrl'"
    }

    if (-not $downloadUrl.EndsWith(".exe", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Поле DownloadUrl в update.xml должно указывать на исполняемый установщик"
    }

    $sha256 = [string]$manifest.FBE.SHA256
    if ($sha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Поле SHA256 в update.xml должно содержать ровно 64 шестнадцатеричных символа"
    }
}

Write-Host "Версия FictionBook Editor Next: $version"
