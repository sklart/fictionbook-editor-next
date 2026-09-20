$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$traceHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.h')
$traceSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('CountUiComCall()', 'UiComCallCount()', 'CountUiCheckCommand()', 'UiCheckCommandCount()', 'CountUiSelectionContainerQuery()', 'UiSelectionContainerQueryCount()', 'CountUiSelectionStructConQuery()', 'UiSelectionStructConQueryCount()', 'CountUiSelectionStructTableConQuery()', 'UiSelectionStructTableConQueryCount()')) {
    if($traceHeader.IndexOf($token, [StringComparison]::Ordinal) -lt 0 -or $traceSource.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Trace counter API is missing '$token'." }
}
if (([regex]::Matches($view, 'StartupTrace::CountUiComCall\(\)').Count) -ne 3) { throw 'Every bCall overload must count a JS/COM dispatch.' }
foreach($field in @('js-com-calls=%llu', 'check-command-calls=%llu', 'selection-container-queries=%llu', 'selection-struct-con-queries=%llu', 'selection-struct-table-con-queries=%llu')) {
    if($frame.IndexOf($field, [StringComparison]::Ordinal) -lt 0) { throw "Idle profile must report '$field'." }
}
Write-Host 'UI COM diagnostic counter contract passed.'
