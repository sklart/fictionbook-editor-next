[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$argsSource = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\apputils.cpp')
$mainFrame = Get-Content -Raw (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$fixtureGenerator = Get-Content -Raw (Join-Path $PSScriptRoot 'new-fb2-memory-fixture.ps1')
$benchmark = Get-Content -Raw (Join-Path $PSScriptRoot 'test-fbe-source-memory.ps1')

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) { throw "Missing $Description." }
}

Assert-Contains $argsSource 'xgetopt\(_ARGV,_T\("ducb:"\)' 'benchmark command-line switches'
Assert-Contains $argsSource "case _T\('b'\)" 'benchmark report path switch'
Assert-Contains $argsSource "case _T\('u'\)" 'undo selection history switch'
Assert-Contains $argsSource "case _T\('c'\)" 'Body-to-Source cycle switch'
Assert-Contains $mainFrame 'GetProcessMemoryInfo' 'full-process private/working-set measurement'
Assert-Contains $mainFrame 'VirtualQuery' 'full-process committed/reserved measurement'
Assert-Contains $mainFrame 'source-styled-wrap-word' 'styled Source checkpoint'
Assert-Contains $mainFrame 'undo-selection-history-10000-edits' 'Undo selection-history stress checkpoint'
Assert-Contains $mainFrame 'undo-all-10000-edits' 'Undo-all checkpoint'
Assert-Contains $mainFrame 'redo-all-10000-edits' 'Redo-all checkpoint'
Assert-Contains $mainFrame 'matched-tags-100000-positions' 'real matched-tags stress checkpoint'
Assert-Contains $mainFrame 'appendSnapshot\("fold-all"\)' 'Fold All profiling checkpoint'
Assert-Contains $mainFrame 'appendSnapshot\("expand-all"\)' 'Expand All profiling checkpoint'
Assert-Contains $mainFrame 'appendSnapshot\("find-section-1000"\)' 'large Source Find profiling checkpoint'
Assert-Contains $mainFrame 'appendSnapshot\("replace-section-same-text-100"\)' 'large Source Replace profiling checkpoint'
Assert-Contains $mainFrame 'appendSnapshot\("navigate-source-lines-1000"\)' 'Source navigation profiling checkpoint'
Assert-Contains $fixtureGenerator 'ValidateRange\(1, 100\)' 'fixture size bounds'
Assert-Contains $fixtureGenerator 'body name="notes"' 'FB2 notes fixture content'
Assert-Contains $fixtureGenerator 'binary id="cover" content-type="image/png"' 'FB2 image metadata fixture content'
Assert-Contains $benchmark 'UndoSelectionHistory' 'on/off benchmark comparison'
Assert-Contains $benchmark 'RunViewCycles' 'optional Body-to-Source cycle benchmark'

Write-Host 'Full-process FBE Source benchmark contract passed.'
