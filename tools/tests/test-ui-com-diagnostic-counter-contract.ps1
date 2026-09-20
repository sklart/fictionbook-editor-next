$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$traceHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.h')
$traceSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('CountUiComCall()', 'UiComCallCount()')) {
    if($traceHeader.IndexOf($token, [StringComparison]::Ordinal) -lt 0 -or $traceSource.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Trace counter API is missing '$token'." }
}
if (([regex]::Matches($view, 'StartupTrace::CountUiComCall\(\)').Count) -ne 3) { throw 'Every bCall overload must count a JS/COM dispatch.' }
if($frame.IndexOf('js-com-calls=%llu', [StringComparison]::Ordinal) -lt 0) { throw 'Idle profile must report aggregate JS/COM calls.' }
Write-Host 'UI COM diagnostic counter contract passed.'
