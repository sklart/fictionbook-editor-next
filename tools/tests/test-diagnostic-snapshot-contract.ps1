$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.cpp')
foreach($required in @('lastHResultFailure = failure', 'lastDispatchFailure = failure', 'lastScriptFailureStage')) {
    if($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing diagnostic snapshot update: $required" }
}
$comException = [regex]::Match($source, '(?s)void StartupTrace::ComException\(.*?\n\}')
if(-not $comException.Success -or $comException.Value.IndexOf('lastHResultFailure = failure', [StringComparison]::Ordinal) -lt 0) { throw 'ComException must update lastHResultFailure.' }
Write-Host 'Diagnostic snapshot contract passed.'