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
    @{ Version = '3.2.0-rc.1'; Tag = 'v3.2.0-rc.1'; Type = 'stable'; Beta = 'false' },
    @{ Version = '3.2.0'; Tag = 'v3.2.0'; Type = 'prerelease'; Beta = 'true' },
    @{ Version = '3.2.0-rc.01'; Tag = 'v3.2.0-rc.01'; Type = 'prerelease'; Beta = 'true' },
    @{ Version = '3.2.0-rc.2'; Tag = 'v3.2.0-rc.3'; Type = 'prerelease'; Beta = 'true' }
)) {
    $content = $source -replace '<Version>[^<]+</Version>', "<Version>$($case.Version)</Version>" -replace '<ReleaseTag>[^<]+</ReleaseTag>', "<ReleaseTag>$($case.Tag)</ReleaseTag>" -replace '<ReleaseType>[^<]+</ReleaseType>', "<ReleaseType>$($case.Type)</ReleaseType>" -replace '<Beta>[^<]+</Beta>', "<Beta>$($case.Beta)</Beta>"
    [IO.File]::WriteAllText($fixture, $content, [Text.UTF8Encoding]::new($false))
    $accepted = $false
    try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $fixture -Feed Any; $accepted = $true } catch { }
    if ($accepted) { throw "Validator accepted invalid manifest case $($case.Version) / $($case.Type)." }
}

Write-Host "Проверка update.xml прошла успешно."
