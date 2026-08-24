<# Verifies schema-v2 candidate generation never changes tracked update.xml. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = ([regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), 'FBE_VERSION_STRING\s+"(?<v>\d+\.\d+\.\d+)"')).Groups['v'].Value
$fixture = Join-Path $root 'out\tests\update-manifest-candidate'
Remove-Item -LiteralPath $fixture -Force -Recurse -ErrorAction SilentlyContinue
foreach ($profile in @('Modern', 'Win7')) { New-Item -ItemType Directory -Path (Join-Path $fixture $profile) -Force | Out-Null }
foreach ($name in @("FictionBookEditorNext-$version-win32-setup.exe", "FictionBookEditorNext-$version-win32-portable.zip")) { Set-Content -LiteralPath (Join-Path $fixture "Modern\$name") -Value $name -NoNewline }
foreach ($name in @("FictionBookEditorNext-$version-win7-win32-setup.exe", "FictionBookEditorNext-$version-win7-win32-portable.zip")) { Set-Content -LiteralPath (Join-Path $fixture "Win7\$name") -Value $name -NoNewline }
$trackedManifest = Join-Path $root 'update.xml'; $before = [IO.File]::ReadAllBytes($trackedManifest)
$candidate = Join-Path $fixture 'update.xml'
& (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1') -ArtifactsRoot $fixture -OutputPath $candidate -ReleaseTag "v$version-rc.2"
if ([Convert]::ToBase64String($before) -ne [Convert]::ToBase64String([IO.File]::ReadAllBytes($trackedManifest))) { throw 'Candidate generation changed tracked update.xml.' }
[xml]$manifest = Get-Content -Raw -LiteralPath $candidate
foreach ($profile in @('Modern', 'Win7')) {
    $node = $manifest.FBE.Artifacts.$profile
    if (-not $node -or [string]::IsNullOrWhiteSpace($node.SetupSHA256) -or [string]::IsNullOrWhiteSpace($node.PortableSHA256)) { throw "Candidate lacks $profile metadata." }
}
if ($manifest.FBE.Version -ne "$version-rc.2" -or $manifest.FBE.ReleaseTag -ne "v$version-rc.2" -or $manifest.FBE.ReleaseType -ne 'prerelease') { throw 'Candidate must preserve prerelease identity separately from artifact base version.' }
& (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $candidate -ExpectedReleaseTag "v$version-rc.2" -Feed PrereleaseFeed
$stableCandidate = Join-Path $fixture 'update-stable.xml'
& (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1') -ArtifactsRoot $fixture -OutputPath $stableCandidate -ReleaseTag "v$version"
& (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $stableCandidate -ExpectedReleaseTag "v$version" -Feed StableFeed
[xml]$stableManifest = Get-Content -Raw -LiteralPath $stableCandidate
if ([string]::IsNullOrWhiteSpace([string]$stableManifest.FBE.DownloadUrl)) { throw 'Stable candidate must retain legacy Modern setup fields.' }
$tampered = Join-Path $fixture 'update-tampered.xml'
$content = Get-Content -Raw -LiteralPath $stableCandidate
[IO.File]::WriteAllText($tampered, $content.Replace('github.com/sklart/fictionbook-editor-next', 'github.com/example/other'), [Text.UTF8Encoding]::new($false))
$accepted = $false
try { & (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $tampered -Feed StableFeed; $accepted = $true } catch { }
if ($accepted) { throw 'Manifest validator accepted an untrusted artifact URL.' }
Write-Host 'Update manifest candidate behavior passed.'
