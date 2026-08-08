$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('lastHResultFailure = failure', 'lastDispatchFailure = failure', 'lastScriptFailureStage')) {
    if($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing diagnostic snapshot update: $required" }
}
$comException = [regex]::Match($source, '(?s)void StartupTrace::ComException\(.*?\n\}')
if(-not $comException.Success -or $comException.Value.IndexOf('lastHResultFailure = failure', [StringComparison]::Ordinal) -lt 0) { throw 'ComException must update lastHResultFailure.' }
$fbdDoc = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$externalHelper = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.cpp')
foreach($route in @('DispatchResult(L"script", L"C120"', 'DispatchResult(L"script", FAILED(hr) ? L"C140"')) {
    if($fbdDoc.IndexOf($route, [StringComparison]::Ordinal) -lt 0) { throw "Missing script dispatch snapshot route: $route" }
}
foreach($route in @('DispatchResult(L"external", L"XH120"', 'DispatchResult(L"external", FAILED(result) ? L"XH140"')) {
    if($externalHelper.IndexOf($route, [StringComparison]::Ordinal) -lt 0) { throw "Missing external dispatch snapshot route: $route" }
}
$crashHandler = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\CrashHandler.cpp')
foreach($required in @('const bool snapshotAvailable = StartupTrace::TryGetCrashTraceSnapshot(traceSnapshot);', 'Trace snapshot available: %s', 'TEMP fallback: %s', 'CString text;', 'text.GetLength() * sizeof(wchar_t)')) {
    if($crashHandler.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing length-safe crash report contract: $required" }
}
if($crashHandler.IndexOf('wchar_t text[2048]', [StringComparison]::Ordinal) -ge 0) { throw 'Crash report must not use a fixed 2048-character buffer.' }

Write-Host 'Diagnostic snapshot contract passed.'
