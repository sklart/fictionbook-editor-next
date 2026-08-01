[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')

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
Write-Host 'JavaScript trace bridge contract passed.'