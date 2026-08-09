[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$smoke = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\scintilla-smoke.cpp')

if ($mainFrame -notmatch 'static\s+int\s+EstimateSourceLineCount\s*\(const\s+CString&\s+text\)') {
    throw 'Не найден подсчёт строк Source для SCI_ALLOCATELINES.'
}
if ($mainFrame -notmatch 'SCI_CLEARALL\s*\)\s*;\s*(?:\r?\n\s*phaseProfiler\.Mark\("SCI_CLEARALL"\);)?\s*\r?\n\s*// Source is filled by one bulk append, so reserve its line-index table once\.\s*\r?\n\s*m_source\.SendMessage\(SCI_ALLOCATELINES,\s*EstimateSourceLineCount\(srcText\)\)') {
    throw 'SCI_ALLOCATELINES должен вызываться после очистки и до bulk append Source.'
}
if ($smoke -notmatch 'allocate-lines-benchmark' -or $smoke -notmatch 'SCI_ALLOCATELINES') {
    throw 'Scintilla smoke не содержит benchmark SCI_ALLOCATELINES.'
}

Write-Host 'Source line-index preallocation contract passed.'
