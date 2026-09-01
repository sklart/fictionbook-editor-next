<# Validates an update manifest without tying a checked-in feed to src/version.h. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$ManifestPath,
    [string]$ExpectedReleaseTag,
    [ValidateSet('StableFeed', 'PrereleaseFeed', 'Any')] [string]$Feed = 'Any'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'UpdateVersion.ps1')
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Get-One([xml]$Document, [string]$Name) {
    $nodes = @($Document.FBE.SelectNodes($Name))
    Require ($nodes.Count -eq 1) "Ожидался один <$Name>."
    return [string]$nodes[0].InnerText
}

[xml]$document = Get-Content -Raw -LiteralPath $ManifestPath
Require ($null -ne $document.SelectSingleNode('/FBE')) 'Корневой элемент должен быть <FBE>.'
Require ($null -eq $document.FBE.ReleaseNotes -and $null -eq $document.FBE.ReleaseNotesUrl) 'Manifest не должен содержать Release Notes или управляемый URL.'
$date = Get-One $document 'Date'
try { [void][DateTime]::ParseExact($date, 'dd-MM-yyyy', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None) } catch { throw 'Date должен иметь формат dd-MM-yyyy.' }
$version = Get-One $document 'Version'; Require (Test-FbeSemVer $version) 'Version не является допустимым SemVer.'
$tag = Get-One $document 'ReleaseTag'; Require (Test-FbeReleaseTag $tag) 'ReleaseTag недопустим.'
Require ($tag.Substring(1) -eq $version) 'Version должен совпадать с ReleaseTag без v.'
if ($ExpectedReleaseTag) { Require ($tag -eq $ExpectedReleaseTag) "ReleaseTag должен быть $ExpectedReleaseTag." }
$type = Get-One $document 'ReleaseType'; Require ($type -in @('stable', 'prerelease')) 'ReleaseType должен быть stable или prerelease.'
$beta = Get-One $document 'Beta'; Require (($type -eq 'stable' -and $beta -eq 'false') -or ($type -eq 'prerelease' -and $beta -eq 'true')) 'Beta не согласован с ReleaseType.'
Require (($type -eq 'stable' -and -not (Test-FbePrereleaseVersion $version)) -or ($type -eq 'prerelease' -and (Test-FbePrereleaseVersion $version))) 'ReleaseType не согласован с prerelease suffix Version.'
if ($Feed -eq 'StableFeed') { Require ($type -eq 'stable') 'Стабильный feed не может содержать prerelease.' }
$assetVersion = if ($version -eq '3.0.8-rc.1') {
    # This already published prerelease used the former base-version names.
    # New prereleases always retain their suffix in the asset filename.
    Get-FbeBaseVersion $version
} else { Get-FbeAssetVersion $version }
$artifacts = @{ Setup = "FictionBookEditorNext-$assetVersion-win32-setup.exe"; Portable = "FictionBookEditorNext-$assetVersion-win32-portable.zip" }
$hasArtifacts = $null -ne $document.FBE.Artifacts
if (-not $hasArtifacts) {
    Require ($type -eq 'stable') 'Prerelease manifest обязан содержать <Artifacts>.'
    $legacyUrl = Get-One $document 'DownloadUrl'; $legacyHash = Get-One $document 'SHA256'
    Require ($legacyHash -match '^[0-9A-Fa-f]{64}$') 'Legacy SHA256 недопустим.'
    $legacyExpected = "https://github.com/sklart/fictionbook-editor-next/releases/download/$tag/FictionBookEditorNext-$assetVersion-win32-setup.exe"
    Require ($legacyUrl -ceq $legacyExpected) 'Legacy DownloadUrl не является доверенным URL ожидаемого артефакта.'
    Write-Host "Проверен legacy manifest: $ManifestPath"
    return
}
function Test-ArtifactPair {
    param(
        [Parameter(Mandatory)] $Node,
        [Parameter(Mandatory)][hashtable]$Names,
        [Parameter(Mandatory)][string]$Label
    )
    foreach ($kind in @('Setup', 'Portable')) {
        $url = [string]$Node.($kind + 'Url'); $hash = [string]$Node.($kind + 'SHA256')
        Require ($hash -match '^[0-9A-Fa-f]{64}$') "$Label $kind SHA256 недопустим."
        $expected = "https://github.com/sklart/fictionbook-editor-next/releases/download/$tag/$($Names[$kind])"
        Require ($url -ceq $expected) "$Label $kind URL не является доверенным URL ожидаемого артефакта."
    }
}

$hasUnifiedNodes = $null -ne $document.FBE.Artifacts.SetupUrl -or
    $null -ne $document.FBE.Artifacts.SetupSHA256 -or
    $null -ne $document.FBE.Artifacts.PortableUrl -or
    $null -ne $document.FBE.Artifacts.PortableSHA256
$hasLegacyNodes = $null -ne $document.FBE.Artifacts.Modern -or $null -ne $document.FBE.Artifacts.Win7
Require ($hasUnifiedNodes -or $hasLegacyNodes) 'Artifacts должен содержать unified или legacy migration nodes.'

if ($hasUnifiedNodes) {
    Test-ArtifactPair -Node $document.FBE.Artifacts -Names $artifacts -Label 'Unified artifact'
}

if ($hasLegacyNodes) {
    # Published 3.0.8-rc.1 uses the former profile schema. It is also emitted
    # only as a temporary migration bridge alongside validated unified nodes.
    Require (Test-FbeLegacy308MigrationRequired $version) 'Legacy Modern/Win7 nodes допустимы только для migration-линии 3.0.8.'
    $legacyProfiles = @{
        Modern = @{ Setup = "FictionBookEditorNext-$assetVersion-win32-setup.exe"; Portable = "FictionBookEditorNext-$assetVersion-win32-portable.zip" }
        Win7 = @{ Setup = "FictionBookEditorNext-$assetVersion-win7-win32-setup.exe"; Portable = "FictionBookEditorNext-$assetVersion-win7-win32-portable.zip" }
    }
    foreach ($profile in $legacyProfiles.Keys) {
        $node = $document.FBE.Artifacts.$profile
        Require ($null -ne $node) "Отсутствует legacy Artifacts/$profile."
        Test-ArtifactPair -Node $node -Names $legacyProfiles[$profile] -Label "Legacy $profile"
    }
}
if ($type -eq 'prerelease') { Require ($null -eq $document.FBE.DownloadUrl -and $null -eq $document.FBE.SHA256) 'Prerelease manifest не должен выдавать legacy setup как универсальный artifact.' }
if ($type -eq 'stable' -and $document.FBE.DownloadUrl) {
    $legacyUrl = [string]$document.FBE.DownloadUrl; $legacyHash = [string]$document.FBE.SHA256
    Require ($legacyHash -match '^[0-9A-Fa-f]{64}$') 'Legacy SHA256 недопустим.'
    $legacyExpected = "https://github.com/sklart/fictionbook-editor-next/releases/download/$tag/FictionBookEditorNext-$assetVersion-win32-setup.exe"
    Require ($legacyUrl -ceq $legacyExpected) 'Legacy DownloadUrl не является доверенным URL ожидаемого артефакта.'
}
Write-Host "Манифест обновления проверен: $ManifestPath"
