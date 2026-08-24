[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

& (Join-Path $repoRoot "tools\version\sync-version.ps1")

& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update.xml') -Feed StableFeed
& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update-prerelease.xml') -Feed PrereleaseFeed

$fixture = Join-Path $repoRoot 'out\tests\update-manifest-negative.xml'
function Get-ArtifactUrl([string]$Tag, [string]$FileName, [string]$AssetHost = 'github.com/sklart/fictionbook-editor-next') {
    return "https://$AssetHost/releases/download/$Tag/$FileName"
}

function New-SchemaV2Manifest([hashtable]$Case) {
    $base = $Case.Base
    $tag = $Case.Tag
    $assetHost = 'github.com/sklart/fictionbook-editor-next'
    $modernSetup = Get-ArtifactUrl $tag "FictionBookEditorNext-$base-win32-setup.exe" $assetHost
    $modernPortable = Get-ArtifactUrl $tag "FictionBookEditorNext-$base-win32-portable.zip" $assetHost
    $win7Setup = Get-ArtifactUrl $tag "FictionBookEditorNext-$base-win7-win32-setup.exe" $assetHost
    $win7Portable = Get-ArtifactUrl $tag "FictionBookEditorNext-$base-win7-win32-portable.zip" $assetHost
    if ($Case.ContainsKey('Artifact')) { $modernSetup = Get-ArtifactUrl $tag $Case.Artifact $assetHost }
    if ($Case.ContainsKey('UrlTag')) { $modernSetup = Get-ArtifactUrl $Case.UrlTag "FictionBookEditorNext-$base-win32-setup.exe" $assetHost }
    if ($Case.ContainsKey('Host')) { $modernSetup = Get-ArtifactUrl $tag "FictionBookEditorNext-$base-win32-setup.exe" $Case.Host }
    $hash = [string]::new('A', 64)
@"
<?xml version="1.0" encoding="utf-8"?>
<FBE>
    <Name>FictionBook Editor Next Release $($Case.Version)</Name>
    <Date>22-08-2026</Date>
    <Version>$($Case.Version)</Version>
    <ReleaseTag>$tag</ReleaseTag>
    <ReleaseType>$($Case.Type)</ReleaseType>
    <Beta>$($Case.Beta)</Beta>
    <Artifacts>
        <Modern><SetupUrl>$modernSetup</SetupUrl><SetupSHA256>$hash</SetupSHA256><PortableUrl>$modernPortable</PortableUrl><PortableSHA256>$hash</PortableSHA256></Modern>
        <Win7><SetupUrl>$win7Setup</SetupUrl><SetupSHA256>$hash</SetupSHA256><PortableUrl>$win7Portable</PortableUrl><PortableSHA256>$hash</PortableSHA256></Win7>
    </Artifacts>
</FBE>
"@
}

$baseline = @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.2'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0' }
[IO.File]::WriteAllText($fixture, (New-SchemaV2Manifest $baseline), [Text.UTF8Encoding]::new($false))
& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $fixture -Feed PrereleaseFeed

foreach ($case in @(
    @{ Version = '3.2.0-rc.1'; Tag = 'v3.2.0-rc.1'; Type = 'stable'; Beta = 'false'; Base = '3.2.0'; ExpectedError = [regex]::Escape('ReleaseType не согласован с prerelease suffix Version.') },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; ExpectedError = [regex]::Escape('ReleaseType не согласован с prerelease suffix Version.') },
    @{ Version = '3.2.0-rc.01'; Tag = 'v3.2.0-rc.01'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; ExpectedError = [regex]::Escape('Version не является допустимым SemVer.') },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.3'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; ExpectedError = [regex]::Escape('Version должен совпадать с ReleaseTag без v.') },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'stable'; Beta = 'true'; Base = '3.2.0'; ExpectedError = [regex]::Escape('Beta не согласован с ReleaseType.') },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.2'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; Artifact = 'wrong.exe'; ExpectedError = [regex]::Escape('Modern/Setup URL не является доверенным URL ожидаемого артефакта.') },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.2'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; UrlTag = 'v3.2.0-rc.3'; ExpectedError = [regex]::Escape('Modern/Setup URL не является доверенным URL ожидаемого артефакта.') },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'stable'; Beta = 'false'; Base = '3.2.0'; Host = 'github.com/example/other'; ExpectedError = [regex]::Escape('Modern/Setup URL не является доверенным URL ожидаемого артефакта.') }
)) {
    [IO.File]::WriteAllText($fixture, (New-SchemaV2Manifest $case), [Text.UTF8Encoding]::new($false))
    $accepted = $false
    $message = $null
    try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $fixture -Feed Any; $accepted = $true } catch { $message = $_.Exception.Message }
    if ($accepted) { throw "Validator accepted invalid manifest case $($case.Version) / $($case.Type)." }
    if ($message -notmatch $case.ExpectedError) { throw "Validator rejected $($case.Version) / $($case.Type) for an unexpected reason: $message" }
}

$legacyPrerelease = Join-Path $repoRoot 'out\tests\update-manifest-legacy-prerelease.xml'
$legacyPrereleaseContent = @"
<?xml version="1.0" encoding="utf-8"?>
<FBE>
    <Name>FictionBook Editor Next Release 3.2.0-rc.2</Name>
    <Date>22-08-2026</Date>
    <Version>3.2.0-rc.2</Version>
    <ReleaseTag>v3.2.0-rc.2</ReleaseTag>
    <ReleaseType>prerelease</ReleaseType>
    <Beta>true</Beta>
    <DownloadUrl>https://github.com/sklart/fictionbook-editor-next/releases/download/v3.2.0-rc.2/FictionBookEditorNext-3.2.0-win32-setup.exe</DownloadUrl>
    <SHA256>$([string]::new('A', 64))</SHA256>
</FBE>
"@
[IO.File]::WriteAllText($legacyPrerelease, $legacyPrereleaseContent, [Text.UTF8Encoding]::new($false))
$accepted = $false
$message = $null
try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $legacyPrerelease -Feed PrereleaseFeed; $accepted = $true } catch { $message = $_.Exception.Message }
if ($accepted) { throw 'Validator accepted a prerelease manifest without <Artifacts>.' }
if ($message -notmatch [regex]::Escape('Prerelease manifest обязан содержать <Artifacts>.')) { throw "Validator rejected legacy prerelease for an unexpected reason: $message" }

Write-Host "Проверка update.xml прошла успешно."
