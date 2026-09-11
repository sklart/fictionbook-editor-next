<#
.SYNOPSIS
Verifies that low-level visual DOM normalization remains independent from UI.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$normalizer = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\VisualDomNormalizer.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\VisualDomNormalizer.h')
$view = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')

foreach($forbidden in @('CFBEView', 'CMainFrame', 'FBEview.h', 'mainfrm.h', 'Settings', 'SourceEditor', 'PluginManager')) {
    if($normalizer -match [regex]::Escape($forbidden) -or $header -match [regex]::Escape($forbidden)) {
        throw "VisualDomNormalizer зависит от запрещённого компонента: $forbidden"
    }
}

foreach($required in @('NormalizeStructure', 'PackText', 'RelocateParagraphs', 'FixupParagraphs', 'KillDivs', 'KillStyles', 'RemoveEmptyNodes', 'SplitBRs', 'BubbleUp')) {
    if($normalizer -notmatch ("\b" + [regex]::Escape($required) + "\b")) { throw "VisualDomNormalizer не содержит $required" }
}

foreach($legacy in @('static void PackText', 'static void RelocateParagraphs', 'static void FixupParagraphs', 'static void KillDivs', 'static void KillStyles', 'static void RemoveEmptyNodes', 'static void SplitBRs', 'void BubbleUp')) {
    if($view -match [regex]::Escape($legacy)) { throw "FBEview.cpp всё ещё определяет $legacy" }
}

if(-not $view.Contains('FbeVisualDom::NormalizeStructure(Document(), el)')) { throw 'CFBEView::Normalize не вызывает normalizer.' }
Write-Host 'Visual DOM normalizer boundary passed.'
