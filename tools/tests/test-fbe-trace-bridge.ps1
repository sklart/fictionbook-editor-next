[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$documentSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
$externalHelperSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ExternalHelper.cpp')
$startupSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.cpp')
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$viewHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')

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
foreach($pattern in @('FBE_NEXT_TRACE_VERBOSE', 'success-count=', 'failure-count=', 'suppressed-count=', 'XH190', 'XH191', 'GetBinarySize', 'GetImageDimsByData', 'GetImageDimsByPath', 'DescShowElement', 'UINT_MAX')) {
    if($externalHelperSource -notlike "*$pattern*") { throw "Missing ExternalHelper trace aggregation contract: $pattern" }
}
if($documentSource -notlike '*TraceOptionalDiagnosticWarning*' -or $documentSource -notlike '*TraceScriptStageSnapshot(L"D115", L"failure-stage"*' -or $documentSource -notlike '*TraceScriptStageSnapshot(L"D116", L"operation-stage"*') {
    throw 'Optional diagnostic API and stage snapshots are not separated from script operation stages.'
}
$traceSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
if(-not $traceSource.Contains('safeCode[0] == L''J''') -or -not $traceSource.Contains('safeCode == L"J900"') -or -not $traceSource.Contains('operation=CSS restore')) {
    throw 'StartupTrace does not preserve the actual JavaScript operation stage.'
}

if($startupSource -notlike '*ExternalHelper::FlushTraceSummary()*') {
    throw 'ExternalHelper trace summary is not flushed before trace shutdown.'
}
foreach($pattern in @('OnNavigateError', 'L"WB135"', 'StartupTrace::RedactPath')) {
    if($viewSource -notlike "*$pattern*") { throw "Missing NavigateError diagnostic contract: $pattern" }
}
if($viewHeader -notlike '*DISPID_NAVIGATEERROR*') { throw 'Missing NavigateError sink contract.' }
foreach($pattern in @('L"WB135"', 'L"WB136"', 'm_navigation_status')) {
    if($viewSource -notlike "*$pattern*") { throw "Incomplete NavigateError handling: $pattern" }
}
if($documentSource -notlike '*m_body.NavigationFailed()*') { throw 'DocumentComplete wait does not stop after NavigateError.' }
foreach($pattern in @('GetErrorInfo(0, &errorInfo)', 'SetErrorInfo(0, errorInfo)', 'pfnDeferredFillIn = NULL', 'InvokeFunc(L"apiSetDiagnosticTraceEnabled", &diagnosticTrace, 1, diagnosticResult, true)', 'InvokeFunc(L"apiGetDiagnosticTraceBridgeState", NULL, 0, bridgeState, true)')) {
    if($documentSource -notlike "*$pattern*") { throw "Missing quiet optional API or COM error preservation contract: $pattern" }
}
foreach($pattern in @('SetErrorInfo(0, errorInfo)', 'pfnDeferredFillIn = NULL', 'RecordLogged', 'SafeMethodHash')) {
    if($externalHelperSource -notlike "*$pattern*") { throw "Missing ExternalHelper dispatch preservation contract: $pattern" }
}
Write-Host 'JavaScript trace bridge contract passed.'
$fbeSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.cpp')
$traceHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.h')
$traceImplementation = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($pattern in @('PARAMFLAG_FIN', 'PARAMFLAG_FOUT', 'PARAMFLAG_FRETVAL', 'core-compatible=%d; diagnostic-compatible=%d')) {
    if($fbeSource -notlike "*$pattern*") { throw "Missing typelib signature validation: $pattern" }
}
foreach($pattern in @('bool ClearOldLogSessions()', 'TrySnapshot', 'TryEnterCriticalSection', 'FindLatestTrace', 'ResolveDiagnosticLogDirectory')) {
    if(($traceHeader + $traceImplementation) -notlike "*$pattern*") { throw "Missing trace fallback or crash snapshot contract: $pattern" }
}foreach($pattern in @('TraceDiagnosticEvent("J210", "operation=CSS restore begin")', 'TraceDiagnosticEvent("J211", "operation=CSS restore success")', 'TraceDiagnosticEvent("J212", "level=error; operation=CSS restore failure; load-result=success")')) {
    if($script -notlike "*$pattern*") { throw "Missing CSS restore diagnostic: $pattern" }
}$mainFrameSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($pattern in @('TracePluginDiagnostic', 'type=%s; clsid=%s; operation=%s; dom-returned=%d', 'L"CreateInstance"', 'L"QueryInterface"', 'L"Import"', 'L"Export"', 'L"completed"', 'L"exception"')) {
    if($mainFrameSource -notlike "*$pattern*") { throw "Missing safe plugin diagnostic: $pattern" }
}
$fastModeSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBDoc.cpp')
foreach($pattern in @('void Doc::FastMode()', 'm_body.HasDoc()', 'L"D230"', 'L"D231"', 'L"D233"')) {
    if($fastModeSource -notlike "*$pattern*") { throw "Missing safe FastMode startup guard: $pattern" }
}
$viewCommandGuard = '(?s)CheckCommand\(WORD wID\).*?if \(!HasDoc\(\)\)\s*return false;'
if($viewSource -notmatch $viewCommandGuard) {
    throw 'Command availability must not dereference a document before it is attached.'
}
$mainFrameIdleGuard = '(?s)BOOL CMainFrame::OnIdle\(\).*?if \(!m_doc \|\| !m_doc->m_body\.HasDoc\(\)\)\s*return false;'
if($mainFrameSource -notmatch $mainFrameIdleGuard) {
    throw 'Idle command updates must wait for the HTML document.'
}
$viewSelectionGuard = '(?s)void CMainFrame::SaveSelection\(VIEW_TYPE vt\).*?m_body\.HasDoc\(\).*?return;'
if($mainFrameSource -notmatch $viewSelectionGuard) {
    throw 'View selection must tolerate an unavailable HTML document.'
}

if($documentSource -notlike '*documentCompleteTimeoutMs = 120000*') {
    throw 'DocumentComplete timeout must tolerate a very slow debugger-started MSHTML instance.'
}
foreach($pattern in @('messageWaitSliceMs = 50', '::PeekMessage(&msg', 'delay MSHTML''s DocumentComplete callback')) {
    if($documentSource -notlike "*$pattern*") { throw "Missing short-slice DocumentComplete wait: $pattern" }
}