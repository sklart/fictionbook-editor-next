[CmdletBinding()]
param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)

$ErrorActionPreference = 'Stop'
$root = Join-Path $RepositoryRoot 'runtime\EditorBackgrounds'
$manifestPath = Join-Path $root 'backgrounds.json'
if(-not (Test-Path -LiteralPath $root -PathType Container)) { throw 'Missing runtime\EditorBackgrounds.' }
if(-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Missing backgrounds.json.' }
$manifest = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
if($manifest.schemaVersion -ne 1 -or @($manifest.backgrounds).Count -ne 14) { throw 'Background manifest must be schema v1 with 14 entries.' }
$ids = @{}
$catalog = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'localization\app-ui\fbe-small-dialogs.json') -Encoding UTF8 | ConvertFrom-Json
for($index = 0; $index -lt $manifest.backgrounds.Count; ++$index) {
    $entry = $manifest.backgrounds[$index]
    if([string]::IsNullOrWhiteSpace($entry.id) -or $ids.ContainsKey($entry.id)) { throw "Invalid or duplicate id: $($entry.id)" }; $ids[$entry.id] = $true
    if($entry.file -notmatch '^[^\\/:?#%]+\.png$') { throw "Unsafe manifest file: $($entry.file)" }
	if($entry.localizationKey -notmatch '^fbe\.settings\.editor_background\.preset\.[a-z0-9_]+$' -or -not $catalog.strings.PSObject.Properties[$entry.localizationKey]) { throw "Missing localized preset: $($entry.id)" }
    if($entry.theme -notin @('light','dark')) { throw "Invalid theme: $($entry.theme)" }
	if($entry.fallbackColor -notmatch '^#[0-9A-Fa-f]{6}$' -or $entry.recommendedTextColor -notmatch '^#[0-9A-Fa-f]{6}$') { throw "Missing colour metadata: $($entry.id)" }
    if($index -lt 11 -and $entry.theme -ne 'light') { throw "Expected light background: $($entry.id)" }
    if($index -ge 11 -and $entry.theme -ne 'dark') { throw "Expected dark background: $($entry.id)" }
    $path = Join-Path $root $entry.file
    if(-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing image: $($entry.file)" }
    $image = [System.Drawing.Image]::FromFile($path)
    try { if($image.Width -ne 1024 -or $image.Height -ne 1024) { throw "Image must be 1024x1024: $($entry.file)" } } finally { $image.Dispose() }
    if($entry.sha256) { $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant(); if($actual -ne $entry.sha256.ToLowerInvariant()) { throw "SHA-256 mismatch: $($entry.file)" } }
}
if((Get-ChildItem -LiteralPath $root -Filter *.png -File).Count -ne 14) { throw 'Runtime folder must contain exactly 14 PNG backgrounds.' }
$packageManifest = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'packaging\package-manifest.json') -Encoding UTF8 | ConvertFrom-Json
if($packageManifest.core.runtimeDirectories -notcontains 'EditorBackgrounds') { throw 'Portable package manifest does not preserve EditorBackgrounds.' }
foreach($key in @('group','image','browse','layout','none','custom','tile','center','contain','cover','choose')) { if(-not $catalog.strings.PSObject.Properties["fbe.settings.editor_background.$key"]) { throw "Missing localized UI key: $key" } }
Write-Host 'Editor background assets verified.'
