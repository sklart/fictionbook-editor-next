[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceViewSession = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\source\SourceViewSession.cpp')
$smoke = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\scintilla-smoke.cpp')

if ($sourceViewSession -notmatch 'SCI_CLEARALL\s*\)\s*;[\s\S]{0,500}int\s+lineCount\s*=\s*1;[\s\S]{0,500}SCI_ALLOCATELINES,\s*lineCount[\s\S]{0,1000}SCI_APPENDTEXT') {
    throw 'SCI_ALLOCATELINES должен вызываться после очистки и до bulk append Source.'
}
if ($smoke -notmatch 'allocate-lines-benchmark' -or $smoke -notmatch 'SCI_ALLOCATELINES') {
    throw 'Scintilla smoke не содержит benchmark SCI_ALLOCATELINES.'
}

Write-Host 'Source line-index preallocation contract passed.'
