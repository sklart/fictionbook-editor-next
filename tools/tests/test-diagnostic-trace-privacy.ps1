$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$trace = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('failedStage = diagnosticFailureStage || diagnosticOperationStage || code', 'description-present=', 'message-present=', 'details=omitted')) {
    if($script.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing safe JavaScript diagnostic field: $required" }
}
if($script.IndexOf('description=" + error.description', [StringComparison]::Ordinal) -ge 0) { throw 'Raw JavaScript error descriptions must not enter diagnostic trace.' }
foreach($required in @('function SanitizeDiagnosticFileName(url)', 'if(value.length > 128)', 'character == "." || character == "-" || character == "_"', 'file-present=', 'file-name=')) {
    if($script.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing safe JavaScript filename field: $required" }
}
foreach($unsafe in @('file=" + fileName', '; line=" + lno')) {
    if($script.IndexOf($unsafe, [StringComparison]::Ordinal) -ge 0) { throw "Unsafe raw JavaScript error field remains: $unsafe" }
}
foreach($required in @('SanitizeScriptDetails', 'RedactPathFragments', 'details omitted', 'source-present=', 'description-present=')) {
    if($trace.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing trace privacy safeguard: $required" }
}
foreach($required in @('excep.source-length=', 'excep.description-length=', 'errorInfo.source-length=', 'errorInfo.description-length=', 'details=omitted')) {
    if($trace.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing COM exception privacy field: $required" }
}
foreach($unsafe in @('excep.source=%s', 'excep.description=%s', 'errorInfo.source=%s', 'errorInfo.description=%s', 'SanitizeExceptionText(exceptionInfo->bstrDescription)', 'SanitizeExceptionText(description)')) {
    if($trace.IndexOf($unsafe, [StringComparison]::Ordinal) -ge 0) { throw "COM exception text must not enter the trace: $unsafe" }
}
Write-Host 'Diagnostic trace privacy contract passed.'
