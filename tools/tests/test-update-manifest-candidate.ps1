<# Verifies schema-v2 candidate generation never changes tracked update.xml. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = ([regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), 'FBE_VERSION_STRING\s+"(?<v>\d+\.\d+\.\d+)"')).Groups['v'].Value
$fixture = Join-Path $root 'out\tests\update-manifest-candidate'
Remove-Item -LiteralPath $fixture -Force -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
foreach ($name in @("FictionBookEditorNext-$version-win32-setup.exe", "FictionBookEditorNext-$version-win32-portable.zip")) { Set-Content -LiteralPath (Join-Path $fixture $name) -Value $name -NoNewline }
Copy-Item -LiteralPath (Join-Path $fixture "FictionBookEditorNext-$version-win32-setup.exe") -Destination (Join-Path $fixture "FictionBookEditorNext-$version-win7-win32-setup.exe")
Copy-Item -LiteralPath (Join-Path $fixture "FictionBookEditorNext-$version-win32-portable.zip") -Destination (Join-Path $fixture "FictionBookEditorNext-$version-win7-win32-portable.zip")
$trackedManifest = Join-Path $root 'update.xml'; $before = [IO.File]::ReadAllBytes($trackedManifest)
$candidate = Join-Path $fixture 'update.xml'
& (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1') -ArtifactsRoot $fixture -OutputPath $candidate -ReleaseTag "v$version-rc.2"
if ([Convert]::ToBase64String($before) -ne [Convert]::ToBase64String([IO.File]::ReadAllBytes($trackedManifest))) { throw 'Candidate generation changed tracked update.xml.' }
[xml]$manifest = Get-Content -Raw -LiteralPath $candidate
if (-not $manifest.FBE.Artifacts -or [string]::IsNullOrWhiteSpace($manifest.FBE.Artifacts.SetupSHA256) -or [string]::IsNullOrWhiteSpace($manifest.FBE.Artifacts.PortableSHA256)) { throw 'Candidate lacks unified artifact metadata.' }
if ($manifest.FBE.Artifacts.Modern.SetupSHA256 -ne $manifest.FBE.Artifacts.SetupSHA256 -or $manifest.FBE.Artifacts.Win7.SetupSHA256 -ne $manifest.FBE.Artifacts.SetupSHA256 -or $manifest.FBE.Artifacts.Win7.PortableSHA256 -ne $manifest.FBE.Artifacts.PortableSHA256) { throw 'Prerelease migration aliases must be byte-identical to unified artifacts.' }
if ($manifest.FBE.Version -ne "$version-rc.2" -or $manifest.FBE.ReleaseTag -ne "v$version-rc.2" -or $manifest.FBE.ReleaseType -ne 'prerelease') { throw 'Candidate must preserve prerelease identity separately from artifact base version.' }
& (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $candidate -ExpectedReleaseTag "v$version-rc.2" -Feed PrereleaseFeed
$stableCandidate = Join-Path $fixture 'update-stable.xml'
& (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1') -ArtifactsRoot $fixture -OutputPath $stableCandidate -ReleaseTag "v$version"
& (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $stableCandidate -ExpectedReleaseTag "v$version" -Feed StableFeed
[xml]$stableManifest = Get-Content -Raw -LiteralPath $stableCandidate
if ([string]::IsNullOrWhiteSpace([string]$stableManifest.FBE.DownloadUrl)) { throw 'Stable candidate must retain legacy setup fields.' }
if ($null -eq $stableManifest.FBE.Artifacts.Modern -or $null -eq $stableManifest.FBE.Artifacts.Win7) { throw '3.0.8 stable candidate must retain the rc.1 migration nodes.' }
if ($stableManifest.FBE.Artifacts.Win7.SetupSHA256 -ne $stableManifest.FBE.Artifacts.SetupSHA256) { throw 'Stable Win7 alias must be byte-identical to universal setup.' }

function Assert-ValidatorRejects([string]$Name, [scriptblock]$Mutate) {
    [xml]$copy = Get-Content -Raw -LiteralPath $candidate
    & $Mutate $copy
    $path = Join-Path $fixture "negative-$Name.xml"
    $copy.Save($path)
    $accepted = $false
    try { & (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $path -Feed PrereleaseFeed; $accepted = $true } catch { }
    if ($accepted) { throw "Mixed-schema validator accepted invalid unified $Name." }
}

Assert-ValidatorRejects 'setup-url' { param($xml) $xml.FBE.Artifacts.SetupUrl = 'https://github.com/example/other/releases/download/v3.0.8-rc.2/FictionBookEditorNext-3.0.8-win32-setup.exe' }
Assert-ValidatorRejects 'portable-url' { param($xml) $xml.FBE.Artifacts.PortableUrl = 'https://github.com/example/other/releases/download/v3.0.8-rc.2/FictionBookEditorNext-3.0.8-win32-portable.zip' }
Assert-ValidatorRejects 'setup-hash' { param($xml) $xml.FBE.Artifacts.SetupSHA256 = 'invalid' }
Assert-ValidatorRejects 'portable-hash' { param($xml) $xml.FBE.Artifacts.PortableSHA256 = 'invalid' }

. (Join-Path $root 'tools\build\UpdateVersion.ps1')
if (-not (Test-FbeLegacy308MigrationRequired '3.0.8-rc.2') -or -not (Test-FbeLegacy308MigrationRequired '3.0.8') -or (Test-FbeLegacy308MigrationRequired '3.0.9') -or (Test-FbeLegacy308MigrationRequired '3.1.0-rc.1')) {
    throw 'Legacy migration policy must be limited to the 3.0.8 release line.'
}
Write-Host 'Update manifest candidate behavior passed.'
