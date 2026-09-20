$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @(
    'struct IdleProfile',
    'StartupTrace::Enabled()',
    'idle-count=%llu',
    'total-ms=%llu',
    'max-ms=%llu',
    'command-state-updates=%llu',
    'selection-context-updates=%llu',
    'file-fingerprint-checks=%llu',
    'toolbar-updates=%llu',
    'tree-updates=%llu',
    'P410')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) {
        throw "OnIdle profiler contract is missing '$token'."
    }
}

if($source.IndexOf('const bool profileIdle = StartupTrace::Enabled();', [StringComparison]::Ordinal) -lt 0) {
    throw 'OnIdle profiler must leave the normal path gated by the existing diagnostic trace.'
}

Write-Host 'Main-frame idle profiler contract passed.'
