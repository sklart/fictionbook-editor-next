[CmdletBinding()]
param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)

$ErrorActionPreference = 'Stop'
$settings = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\Settings.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\Settings.h')
$doc = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\FBDoc.cpp')
foreach($key in @('EditorBackgroundKind','EditorBackgroundId','EditorBackgroundCustomPath','EditorBackgroundLayout')) { if($settings -notmatch $key) { throw "Missing settings key: $key" } }
foreach($value in @('L"none"','L"tile"')) { if($settings -notmatch [regex]::Escape($value)) { throw "Missing legacy-safe default: $value" } }
foreach($method in @('GetEditorBackgroundKind','SetEditorBackgroundLayout')) { if($header -notmatch $method) { throw "Missing settings API: $method" } }
if($settings -notmatch 'value == L"builtin" \|\| value == L"custom"') { throw 'Unknown background kinds must fall back to none.' }
if($settings -notmatch 'value == L"center" \|\| value == L"contain" \|\| value == L"cover"') { throw 'Unknown layout must fall back to tile.' }
foreach($needle in @('ApplyEditorBackground','EditorBackgrounds::ResolveBuiltIn','EditorBackgrounds::IsSupportedLocalImage','UrlFromPath','SPI_GETHIGHCONTRAST','backgroundImage = L"none"')) { if($doc -notmatch [regex]::Escape($needle)) { throw "Missing safe DOM background behavior: $needle" } }
Write-Host 'Editor background settings contract verified.'
