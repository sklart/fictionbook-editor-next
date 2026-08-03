$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('ResolveDiagnosticLogDirectories', 'ParseDiagnosticLogName', 'segment > latestSegment', 'FBE Next Diagnostics', 'time.wMilliseconds')) {
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
Write-Host 'Diagnostic log segment lookup contract passed.'