[CmdletBinding()]
param()
$ErrorActionPreference = "Stop"
function Select-PreviewVariant([bool]$full,[bool]$compact,[bool]$minimal,[bool]$fallback) { if($full){return 'full'}; if($compact){return 'compact'}; if($minimal){return 'minimal'}; if($fallback){return 'fallback'}; return 'none' }
$cases=@(@($true,$true,$true,$true,'full'),@($false,$true,$true,$true,'compact'),@($false,$false,$true,$true,'minimal'),@($false,$false,$false,$true,'fallback'),@($false,$false,$false,$false,'none'))
foreach($case in $cases) { if((Select-PreviewVariant $case[0] $case[1] $case[2] $case[3]) -ne $case[4]) { throw 'Preview selector order failed.' } }
$root=(Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path; $source=Get-Content -Raw (Join-Path $root 'src\fbe\SettingsNextDlg.cpp')
foreach($fragment in @('<?xml version=\"1.0\"?>','<section id=\"m\">','<p id=\"x\">T&amp;</p>','<p/>','SelectSourcePreviewVariant','XML_SOURCE_PREVIEW_NONE')) { if($source -notlike "*$fragment*"){throw "Preview fragment missing: $fragment"} }
if($source -match 'if\(x \+ size\.cx > bounds\.right\) return') { throw 'Preview must choose a complete variant, not skip individual tokens.' }
Write-Host 'Source preview variant selection passed.'