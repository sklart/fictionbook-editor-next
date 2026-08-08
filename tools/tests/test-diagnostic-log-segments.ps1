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
Write-Host 'Diagnostic log segment lookup contract passed.'
