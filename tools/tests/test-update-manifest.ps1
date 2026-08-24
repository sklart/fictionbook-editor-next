[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

& (Join-Path $repoRoot "tools\version\sync-version.ps1")

& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update.xml') -Feed StableFeed
& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update-prerelease.xml') -Feed PrereleaseFeed

$fixture = Join-Path $repoRoot 'out\tests\update-manifest-negative.xml'
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'update.xml')
foreach ($case in @(
    @{ Version = '3.2.0-rc.1'; Tag = 'v3.2.0-rc.1'; Type = 'stable'; Beta = 'false'; Base = '3.2.0' },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0' },
    @{ Version = '3.2.0-rc.01'; Tag = 'v3.2.0-rc.01'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0' },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.3'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0' },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'stable'; Beta = 'true'; Base = '3.2.0' },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.2'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; Artifact = 'wrong.exe' },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.2'; Type = 'prerelease'; Beta = 'true'; Base = '3.2.0'; UrlTag = 'v3.2.0-rc.3' },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'stable'; Beta = 'false'; Base = '3.2.0'; Host = 'github.com/example/other' }
)) {
    $urlTag = if ($case.UrlTag) { $case.UrlTag } else { $case.Tag }
    $artifact = if ($case.Artifact) { $case.Artifact } else { "FictionBookEditorNext-$($case.Base)-win32-setup.exe" }
    $assetHost = if ($case.Host) { $case.Host } else { 'github.com/sklart/fictionbook-editor-next' }
    $url = "https://$assetHost/releases/download/$urlTag/$artifact"
    $content = $source -replace '<Version>[^<]+</Version>', "<Version>$($case.Version)</Version>" -replace '<ReleaseTag>[^<]+</ReleaseTag>', "<ReleaseTag>$($case.Tag)</ReleaseTag>" -replace '<ReleaseType>[^<]+</ReleaseType>', "<ReleaseType>$($case.Type)</ReleaseType>" -replace '<Beta>[^<]+</Beta>', "<Beta>$($case.Beta)</Beta>" -replace '<DownloadUrl>[^<]+</DownloadUrl>', "<DownloadUrl>$url</DownloadUrl>"
    [IO.File]::WriteAllText($fixture, $content, [Text.UTF8Encoding]::new($false))
    $accepted = $false
    try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $fixture -Feed Any; $accepted = $true } catch { }
    if ($accepted) { throw "Validator accepted invalid manifest case $($case.Version) / $($case.Type)." }
}

$legacyPrerelease = Join-Path $repoRoot 'out\tests\update-manifest-legacy-prerelease.xml'
$legacyPrereleaseContent = $source -replace '(?s)\s*<Artifacts>.*?</Artifacts>', '' -replace '<Version>[^<]+</Version>', '<Version>3.2.0-rc.2</Version>' -replace '<ReleaseTag>[^<]+</ReleaseTag>', '<ReleaseTag>v3.2.0-rc.2</ReleaseTag>' -replace '<ReleaseType>[^<]+</ReleaseType>', '<ReleaseType>prerelease</ReleaseType>' -replace '<Beta>[^<]+</Beta>', '<Beta>true</Beta>' -replace '<DownloadUrl>[^<]+</DownloadUrl>', '<DownloadUrl>https://github.com/sklart/fictionbook-editor-next/releases/download/v3.2.0-rc.2/FictionBookEditorNext-3.2.0-win32-setup.exe</DownloadUrl>'
[IO.File]::WriteAllText($legacyPrerelease, $legacyPrereleaseContent, [Text.UTF8Encoding]::new($false))
$accepted = $false
try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $legacyPrerelease -Feed PrereleaseFeed; $accepted = $true } catch { }
if ($accepted) { throw 'Validator accepted a prerelease manifest without <Artifacts>.' }

Write-Host "Проверка update.xml прошла успешно."
