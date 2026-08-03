$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ignore = Get-Content -Raw -LiteralPath (Join-Path $repoRoot '.gitignore')
$rules = @('*.obj', '*.pdb', '*.ilk', '*.tlog', '*.lastbuildstate', 'hs_err_pid*.log', 'replay_pid*.log', 'PVS-Studio.stacktrace.txt', '/build/', '/out/', 'fbe-trace-*.log', 'FBE-Diagnostics-*.zip')
foreach($rule in $rules) {
    if($ignore.IndexOf($rule, [StringComparison]::Ordinal) -lt 0) { throw "Missing .gitignore rule: $rule" }
}

$artifactPattern = '(?i)(^|/)([^/]+\.(obj|pdb|ilk|tlog|lastbuildstate)|hs_err_pid[^/]*\.log|replay_pid[^/]*\.log|PVS-Studio\.stacktrace\.txt|fbe-trace-[^/]*\.log|FBE-Diagnostics-[^/]*\.zip)$'
$tracked = & git -C $repoRoot ls-files
$trackedArtifacts = @($tracked | Where-Object { $_ -match $artifactPattern })
if($trackedArtifacts.Count) { throw "Tracked diagnostic build artifacts found: $($trackedArtifacts -join ', ')" }

$status = & git -C $repoRoot status --porcelain
$worktreeArtifacts = @($status | Where-Object { $_.Substring(0, 2) -notmatch 'D' } | ForEach-Object { $_.Substring(3).Replace('\', '/') } | Where-Object { $_ -match $artifactPattern })
if($worktreeArtifacts.Count) { throw "Diagnostic build artifacts in worktree: $($worktreeArtifacts -join ', ')" }

Write-Host 'Diagnostic build artifact ignore and repository contract passed.'