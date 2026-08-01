[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$documentSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')

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
foreach($pattern in @('apiGetDiagnosticTraceBridgeState', 'diagnosticBridgeUnavailable', 'diagnostic trace bridge=unknown')) {
    if($documentSource -notlike "*$pattern*") { throw "Missing C++ trace bridge diagnostic: $pattern" }
}
Write-Host 'JavaScript trace bridge contract passed.'