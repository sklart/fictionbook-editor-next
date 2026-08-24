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

function Write-Utf8FileIfChanged {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Content
    )

    $current = if (Test-Path -LiteralPath $Path -PathType Leaf) {
        [IO.File]::ReadAllText($Path, [Text.UTF8Encoding]::new($false))
    } else {
        $null
    }
    if ($current -ceq $Content) {
        Write-Host "Generated-файл уже синхронизирован: $Path"
        return
    }

    [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
    Write-Host "Generated-файл обновлён: $Path"
}

Write-Utf8FileIfChanged -Path $versionNsh -Content $nsh

if ($ValidateUpdateManifest) {
    & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $updateManifest -Feed StableFeed
}

Write-Host "Версия FictionBook Editor Next: $version"
