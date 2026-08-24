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
$root = (Resolve-Path -LiteralPath $ArtifactsRoot).Path

function Get-ArtifactMetadata([string]$Profile, [string]$Name) {
    $path = Join-Path $root "$Profile\$Name"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден release artifact для update manifest: $path" }
    return @{ Url = "https://github.com/sklart/fictionbook-editor-next/releases/download/$ReleaseTag/$Name"; Hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
}

$modernSetup = Get-ArtifactMetadata Modern "FictionBookEditorNext-$version-win32-setup.exe"
$modernPortable = Get-ArtifactMetadata Modern "FictionBookEditorNext-$version-win32-portable.zip"
$win7Setup = Get-ArtifactMetadata Win7 "FictionBookEditorNext-$version-win7-win32-setup.exe"
$win7Portable = Get-ArtifactMetadata Win7 "FictionBookEditorNext-$version-win7-win32-portable.zip"

$document = New-Object Xml.XmlDocument
$declaration = $document.CreateXmlDeclaration('1.0', 'utf-8', $null); [void]$document.AppendChild($declaration)
$fbe = $document.CreateElement('FBE'); [void]$document.AppendChild($fbe)
foreach ($pair in @(@('Name', "FictionBook Editor Next Release $releaseVersion"), @('Date', (Get-Date -Format 'dd-MM-yyyy')), @('Version', $releaseVersion), @('ReleaseTag', $ReleaseTag), @('ReleaseType', $releaseType), @('Beta', ($releaseType -eq 'prerelease').ToString().ToLowerInvariant()))) {
    $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$fbe.AppendChild($node)
}
$artifacts = $document.CreateElement('Artifacts'); [void]$fbe.AppendChild($artifacts)
foreach ($profile in @(@('Modern', $modernSetup, $modernPortable), @('Win7', $win7Setup, $win7Portable))) {
    $profileNode = $document.CreateElement($profile[0]); [void]$artifacts.AppendChild($profileNode)
    foreach ($pair in @(@('SetupUrl', $profile[1].Url), @('SetupSHA256', $profile[1].Hash), @('PortableUrl', $profile[2].Url), @('PortableSHA256', $profile[2].Hash))) {
        $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$profileNode.AppendChild($node)
    }
}
# Keep the Modern setup fields for already released clients that predate
# schema-v2. Prerelease clients need SemVer support and therefore use Artifacts.
if ($releaseType -eq 'stable') {
    foreach ($pair in @(@('DownloadUrl', $modernSetup.Url), @('SHA256', $modernSetup.Hash))) {
        $node = $document.CreateElement($pair[0]); $node.InnerText = $pair[1]; [void]$fbe.AppendChild($node)
    }
}
$directory = Split-Path -Parent $OutputPath; New-Item -ItemType Directory -Path $directory -Force | Out-Null
$settings = New-Object Xml.XmlWriterSettings; $settings.Encoding = [Text.UTF8Encoding]::new($false); $settings.Indent = $true; $settings.IndentChars = "`t"; $settings.NewLineChars = "`r`n"
$writer = [Xml.XmlWriter]::Create($OutputPath, $settings)
try { $document.Save($writer) } finally { $writer.Dispose() }
Write-Host "Update manifest candidate written: $OutputPath"
