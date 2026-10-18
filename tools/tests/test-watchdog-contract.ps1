# Architectural guards complement the real UI-stall / trace-lock runtime test.
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$worker=Get-Content -Raw (Join-Path $root 'src/fbe/diagnostics/UiWatchdog.cpp')
$header=Get-Content -Raw (Join-Path $root 'src/fbe/diagnostics/UiWatchdog.h')
$telemetry=Get-Content -Raw (Join-Path $root 'src/fbe/diagnostics/HangTelemetry.inl')
foreach($token in 'PostMessage','WaitForSingleObject','QueryUnbiasedInterruptTime','IsDebuggerPresent','HandlingCrash','ValidateHangReport','CREATE_NEW','FlushFileBuffers','TryGetHangTelemetry') {
 if(!$worker.Contains($token)){throw "Watchdog boundary missing: $token"}
}
foreach($forbidden in 'SendMessage','SuspendThread','GetThreadContext','MiniDumpWriteDump','StartupTrace::Event','StartupTrace::Flush','StartupTrace::OperationStatisticsReport','TryGetCrashTraceSnapshot','TraceLock') {
 if($worker.Contains($forbidden)){throw "Watchdog must not wait for UI/trace or capture a dump: $forbidden"}
}
if($header -notmatch 'suspectMs=15000, confirmMs=30000' -or $header -notmatch 'std::atomic'){throw 'Production thresholds or atomic heartbeat missing'}
if($telemetry -notmatch 'TryAcquireSRWLockExclusive' -or $telemetry -match 'WriteFile|EnterCriticalSection|(?<!Try)AcquireSRWLockExclusive\('){throw 'Telemetry publication must remain independent of trace I/O'}
$main=Get-Content -Raw (Join-Path $root 'src/fbe/mainfrm.cpp')
foreach($token in 'm_ui_watchdog.Start(m_hWnd)','m_ui_watchdog.Stop()','m_ui_watchdog.Pause()','m_ui_watchdog.Resume()') {if(!$main.Contains($token)){throw "Missing lifecycle hook: $token"}}
Write-Host 'PASS: watchdog 15/30-second thresholds, bounded posted heartbeat, event waits, independent telemetry/report I/O and lifecycle boundaries (source contract).'
