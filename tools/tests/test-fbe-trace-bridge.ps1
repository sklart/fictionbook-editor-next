[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$documentSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$externalHelperSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.cpp')
$startupSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.cpp')
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')

$required = @(
    'var diagnosticTraceBridgeState = 0;',
    'function apiGetDiagnosticTraceBridgeState()',
    'diagnosticTraceBridgeState == -1',
    'window.external.TraceScript(code, message);',
    'diagnosticTraceBridgeState = -1;'
)
foreach($pattern in $required) {
    if($script -notlike "*$pattern*") { throw "Missing trace bridge contract: $pattern" }
}
if($script -match 'window\.external\s*&&\s*window\.external\.TraceScript') {
    throw 'TraceScript must not probe the property before its direct call.'
}
foreach($pattern in @('apiGetDiagnosticTraceBridgeState', 'apiGetDiagnosticOperationStage', 'diagnosticBridgeUnavailable', 'diagnostic trace bridge=unknown')) {
    if($documentSource -notlike "*$pattern*") { throw "Missing C++ trace bridge diagnostic: $pattern" }
}
foreach($pattern in @('FBE_NEXT_TRACE_VERBOSE', 'success-count=', 'failure-count=', 'suppressed-count=', 'XH190', 'XH191')) {
    if($externalHelperSource -notlike "*$pattern*") { throw "Missing ExternalHelper trace aggregation contract: $pattern" }
}
if($startupSource -notlike '*ExternalHelper::FlushTraceSummary()*') {
    throw 'ExternalHelper trace summary is not flushed before trace shutdown.'
}foreach($pattern in @('DISPID_NAVIGATEERROR', 'OnNavigateError', 'code=WB135', 'StartupTrace::RedactPath')) {
    if($viewSource -notlike "*$pattern*") { throw "Missing NavigateError diagnostic contract: $pattern" }
}Write-Host 'JavaScript trace bridge contract passed.'