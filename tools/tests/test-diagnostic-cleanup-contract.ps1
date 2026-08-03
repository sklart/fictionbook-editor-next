$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$localization = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-idr-mainframe-menu.json') | ConvertFrom-Json

foreach($field in @('sessionsFound','sessionsDeleted','filesDeleted','filesFailed','lastError')) {
    if($header.IndexOf($field, [StringComparison]::Ordinal) -lt 0) { throw "Cleanup result is missing $field." }
}
foreach($pattern in @('StartupTrace::DiagnosticLogCleanupResult StartupTrace::ClearOldLogSessions()', 'ResolveDiagnosticLogDirectories(directories)', 'ERROR_PATH_NOT_FOUND', 'filesFailed', 'filesDeleted', 'sessionsDeleted')) {
    if($source.IndexOf($pattern, [StringComparison]::Ordinal) -lt 0) { throw "Cleanup implementation is missing $pattern." }
}
foreach($key in @('fbe.trace.clear_completed_details','fbe.trace.clear_partial','fbe.trace.clear_empty','fbe.trace.clear_delete_failed')) {
    $entry = $localization.strings.PSObject.Properties | Where-Object Name -eq $key
    if($null -eq $entry) { throw "Cleanup localization key is missing: $key" }
    if($key -ne 'fbe.trace.clear_empty' -and $entry.Value.translations.'en-US' -notmatch '%u') { throw "Cleanup localization key must retain numeric result data: $key" }
}
foreach($key in @('fbe.trace.clear_empty','fbe.trace.clear_completed_details','fbe.trace.clear_partial','fbe.trace.clear_delete_failed')) {
    if($frame.IndexOf($key, [StringComparison]::Ordinal) -lt 0) { throw "Cleanup UI does not use $key." }
}
Write-Host 'Diagnostic cleanup contract passed.'
