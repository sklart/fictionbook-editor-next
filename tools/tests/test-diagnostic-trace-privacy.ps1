$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$trace = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('failedStage = diagnosticFailureStage || diagnosticOperationStage || code', 'description-present=', 'message-present=', 'details=omitted')) {
    if($script.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing safe JavaScript diagnostic field: $required" }
}
if($script.IndexOf('description=" + error.description', [StringComparison]::Ordinal) -ge 0) { throw 'Raw JavaScript error descriptions must not enter diagnostic trace.' }
foreach($required in @('SanitizeScriptDetails', 'RedactPathFragments', 'details omitted')) {
    if($trace.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing trace privacy safeguard: $required" }
}
Write-Host 'Diagnostic trace privacy contract passed.'