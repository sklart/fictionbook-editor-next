<# Creates schema-v2 update metadata from already verified release artifacts. #>
[CmdletBinding()]
param(
    [string]$ArtifactsRoot,
    [string]$OutputPath,
    [Parameter(Mandatory)]
    [string]$ReleaseTag
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'UpdateVersion.ps1')
if (-not $ArtifactsRoot) { $ArtifactsRoot = Join-Path $repoRoot 'out\artifacts' }
if (-not $OutputPath) { $OutputPath = Join-Path $repoRoot 'out\release\update.xml' }
$versionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
$match = [regex]::Match($versionText, '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')
if (-not $match.Success) { throw 'Не найден FBE_VERSION_STRING.' }
$version = $match.Groups['version'].Value
if (-not (Test-FbeReleaseTag $ReleaseTag)) { throw "Недопустимый release tag: $ReleaseTag" }
$releaseVersion = $ReleaseTag.Substring(1)
if ((Get-FbeBaseVersion $releaseVersion) -ne $version) { throw "ReleaseTag $ReleaseTag не соответствует base version $version." }
$releaseType = if (Test-FbePrereleaseVersion $releaseVersion) { 'prerelease' } else { 'stable' }
$legacy308MigrationRequired = Test-FbeLegacy308MigrationRequired $releaseVersion
$root = (Resolve-Path -LiteralPath $ArtifactsRoot).Path

function Get-ArtifactMetadata([string]$Name) {
    $path = Join-Path $root $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден release artifact для update manifest: $path" }
    return @{ Url = "https://github.com/sklart/fictionbook-editor-next/releases/download/$ReleaseTag/$Name"; Hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
}

$setup = Get-ArtifactMetadata "FictionBookEditorNext-$version-win32-setup.exe"
$portable = Get-ArtifactMetadata "FictionBookEditorNext-$version-win32-portable.zip"
$legacyWin7Setup = $null
$legacyWin7Portable = $null
if ($legacy308MigrationRequired) {
    # Transitional aliases are byte-identical copies for the already released
    # 3.0.8-rc.1 Win7 updater. They are not a second build profile.
    $legacyWin7Setup = Get-ArtifactMetadata "FictionBookEditorNext-$version-win7-win32-setup.exe"
    $legacyWin7Portable = Get-ArtifactMetadata "FictionBookEditorNext-$version-win7-win32-portable.zip"
    if ($legacyWin7Setup.Hash -ne $setup.Hash -or $legacyWin7Portable.Hash -ne $portable.Hash) {
        throw 'Legacy Win7 migration aliases must be byte-identical to universal artifacts.'
    }
}

$document = New-Object Xml.XmlDocument
$declaration = $document.CreateXmlDeclaration('1.0', 'utf-8', $null); [void]$document.AppendChild($declaration)
$fbe = $document.CreateElement('FBE'); [void]$document.AppendChild($fbe)
foreach ($pair in @(@('Name', "FictionBook Editor Next Release $releaseVersion"), @('Date', (Get-Date -Format 'dd-MM-yyyy')), @('Version', $releaseVersion), @('ReleaseTag', $ReleaseTag), @('ReleaseType', $releaseType), @('Beta', ($releaseType -eq 'prerelease').ToString().ToLowerInvariant()))) {
    $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$fbe.AppendChild($node)
}
$artifacts = $document.CreateElement('Artifacts'); [void]$fbe.AppendChild($artifacts)
foreach ($pair in @(@('SetupUrl', $setup.Url), @('SetupSHA256', $setup.Hash), @('PortableUrl', $portable.Url), @('PortableSHA256', $portable.Hash))) {
    $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$artifacts.AppendChild($node)
}
if ($legacy308MigrationRequired) {
    $modern = $document.CreateElement('Modern'); [void]$artifacts.AppendChild($modern)
    $win7 = $document.CreateElement('Win7'); [void]$artifacts.AppendChild($win7)
    foreach ($pair in @(@('SetupUrl', $setup.Url), @('SetupSHA256', $setup.Hash), @('PortableUrl', $portable.Url), @('PortableSHA256', $portable.Hash))) {
        $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$modern.AppendChild($node)
    }
    foreach ($pair in @(@('SetupUrl', $legacyWin7Setup.Url), @('SetupSHA256', $legacyWin7Setup.Hash), @('PortableUrl', $legacyWin7Portable.Url), @('PortableSHA256', $legacyWin7Portable.Hash))) {
        $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$win7.AppendChild($node)
    }
}
# Keep legacy setup fields for v3.0.7 and other released clients.
if ($releaseType -eq 'stable') {
    foreach ($pair in @(@('DownloadUrl', $setup.Url), @('SHA256', $setup.Hash))) {
        $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$fbe.AppendChild($node)
    }
}
$directory = Split-Path -Parent $OutputPath; New-Item -ItemType Directory -Path $directory -Force | Out-Null
$settings = New-Object Xml.XmlWriterSettings; $settings.Encoding = [Text.UTF8Encoding]::new($false); $settings.Indent = $true; $settings.IndentChars = "`t"; $settings.NewLineChars = "`r`n"
$writer = [Xml.XmlWriter]::Create($OutputPath, $settings)
try { $document.Save($writer) } finally { $writer.Dispose() }
Write-Host "Update manifest candidate written: $OutputPath"
