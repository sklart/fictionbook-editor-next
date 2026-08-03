$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ignore = Get-Content -Raw -LiteralPath (Join-Path $repoRoot '.gitignore')
foreach($rule in @('*.obj', '*.pdb', '*.ilk', '*.tlog', '*.lastbuildstate', 'hs_err_pid*.log', 'replay_pid*.log', 'PVS-Studio.stacktrace.txt', '/build/', '/out/', 'fbe-trace-*.log', 'FBE-Diagnostics-*.zip')) {
    if($ignore.IndexOf($rule, [StringComparison]::Ordinal) -lt 0) { throw "Missing .gitignore rule: $rule" }
}
Write-Host 'Diagnostic build artifact ignore contract passed.'