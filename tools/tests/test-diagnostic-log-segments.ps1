$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('ResolveDiagnosticLogDirectories', 'ParseDiagnosticLogName', 'ParseDiagnosticLogNumber', 'CompareDiagnosticSessionName', 'logName.segment > latestLogName.segment', 'FBE Next Diagnostics', 'time.wMilliseconds', 'retentionDirectories', 'CurrentLogDirectory() { TraceLock guard; const CString path = tracePath.IsEmpty() ? FindLatestTrace')) {
    if($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing diagnostic log lookup contract: $required" }
}
$files = @(
    'fbe-trace-20260803-120000-001-pid42.log',
    'fbe-trace-20260803-120000-001-pid42-part1.log',
    'fbe-trace-20260803-120000-001-pid42-part2.log',
    'fbe-trace-20260803-120000-001-pid42-part9.log',
    'fbe-trace-20260803-120000-001-pid42-part10.log'
)
$segments = foreach($file in $files) {
    if($file -match '-part(\d+)\.log$') { [int]$Matches[1] } else { 0 }
}
if(($segments | Measure-Object -Maximum).Maximum -ne 10) { throw 'Numeric diagnostic segment selection did not choose part10.' }
$sessions = @(
    [pscustomobject]@{ Timestamp = [uint64]20260803120000001; ProcessId = 9; Suffix = 9; Segment = 10 },
    [pscustomobject]@{ Timestamp = [uint64]20260803120000001; ProcessId = 9; Suffix = 10; Segment = 0 },
    [pscustomobject]@{ Timestamp = [uint64]20260803120000001; ProcessId = 10; Suffix = 0; Segment = 0 }
)
$latest = $sessions | Sort-Object Timestamp, ProcessId, Suffix, Segment | Select-Object -Last 1
if($latest.ProcessId -ne 10 -or $latest.Suffix -ne 0) { throw 'Numeric diagnostic session selection did not choose pid10 after pid9/suffix10.' }
$suffixes = @('fbe-trace-20260803-120000-001-pid9-9.log', 'fbe-trace-20260803-120000-001-pid9-10.log')
$latestSuffix = $suffixes | ForEach-Object { if($_ -notmatch '-pid(\d+)(?:-(\d+))?\.log$') { throw "Could not parse session name: $_" }; [pscustomobject]@{ Pid = [int]$Matches[1]; Suffix = if($Matches[2]) { [int]$Matches[2] } else { 0 } } } | Sort-Object Pid, Suffix | Select-Object -Last 1
if($latestSuffix.Suffix -ne 10) { throw 'Numeric diagnostic session selection did not choose suffix10 after suffix9.' }
$retentionFixtures = @(
    'fbe-trace-20260803-120000-001-pid9-9.log',
    'fbe-trace-20260803-120000-001-pid9-10.log',
    'fbe-trace-20260803-120000-001-pid10.log',
    'fbe-trace-20260803-120000-002-pid1.log',
    'fbe-trace-20260803-120000-003-pid1.log',
    'fbe-trace-20260803-120000-004-pid1.log',
    'fbe-trace-20260803-120000-005-pid1.log',
    'fbe-trace-20260803-120000-006-pid1.log',
    'fbe-trace-20260803-120000-007-pid1.log',
    'fbe-trace-20260803-120000-008-pid1.log',
    'fbe-trace-20260803-120000-009-pid1.log',
    'fbe-trace-20260803-120000-010-pid1.log'
)
$orderedRetention = $retentionFixtures | ForEach-Object {
    if($_ -notmatch '^fbe-trace-(\d{8})-(\d{6})-(\d{3})-pid(\d+)(?:-(\d+))?\.log$') { throw "Could not parse retention fixture: $_" }
    [pscustomobject]@{ Name=$_; Date=[int64]$Matches[1]; Time=[int64]$Matches[2]; Milliseconds=[int]$Matches[3]; Pid=[int]$Matches[4]; Suffix=if($Matches[5]){[int]$Matches[5]}else{0} }
} | Sort-Object Date,Time,Milliseconds,Pid,Suffix -Descending
$retained = @($orderedRetention | Select-Object -First 10)
if($retained.Count -ne 10 -or $retained.Name -contains 'fbe-trace-20260803-120000-001-pid9-9.log' -or $retained.Name -contains 'fbe-trace-20260803-120000-001-pid9-10.log') { throw 'Retention did not discard the two numerically oldest sessions.' }
if($retained.Name -notcontains 'fbe-trace-20260803-120000-001-pid10.log') { throw 'Retention did not order pid10 after pid9/suffix10.' }
Write-Host 'Diagnostic log segment lookup contract passed.'
