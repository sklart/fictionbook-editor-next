[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\version\sync-version.ps1')
& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update.xml') -Feed StableFeed
& (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath (Join-Path $repoRoot 'update-prerelease.xml') -Feed PrereleaseFeed

$fixture = Join-Path $repoRoot 'out\tests\update-manifest-negative.xml'
New-Item -ItemType Directory -Path (Split-Path -Parent $fixture) -Force | Out-Null
$hash = [string]::new('A', 64)
@"
<?xml version="1.0" encoding="utf-8"?>
<FBE><Name>FBE</Name><Date>22-08-2026</Date><Version>3.2.0-rc.2</Version><ReleaseTag>v3.2.0-rc.2</ReleaseTag><ReleaseType>prerelease</ReleaseType><Beta>true</Beta><Artifacts><SetupUrl>https://github.com/example/other/releases/download/v3.2.0-rc.2/FictionBookEditorNext-3.2.0-win32-setup.exe</SetupUrl><SetupSHA256>$hash</SetupSHA256><PortableUrl>https://github.com/sklart/fictionbook-editor-next/releases/download/v3.2.0-rc.2/FictionBookEditorNext-3.2.0-win32-portable.zip</PortableUrl><PortableSHA256>$hash</PortableSHA256></Artifacts></FBE>
"@ | Set-Content -LiteralPath $fixture -Encoding UTF8
$accepted = $false
try { & (Join-Path $repoRoot 'tools\build\validate-update-manifest.ps1') -ManifestPath $fixture -Feed PrereleaseFeed; $accepted = $true } catch { }
if ($accepted) { throw 'Validator accepted an untrusted unified artifact URL.' }
Write-Host 'Unified update manifest validation passed.'
