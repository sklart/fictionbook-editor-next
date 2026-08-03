$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'test-diagnostics.ps1')
$required = @('test-fbe-main-menu-connected-resource.ps1','test-fbe-main-menu-generated-resource.ps1','test-diagnostic-build-artifacts.ps1','test-resource-id-safety.ps1','test-diagnostic-localization.ps1','test-diagnostic-log-segments.ps1','test-diagnostic-trace-privacy.ps1','test-fbe-trace-bridge.ps1','test-fbe-typelib-diagnostics.ps1','test-fbe-startup.ps1')
foreach($test in $required) { if($runner.IndexOf($test, [StringComparison]::Ordinal) -lt 0) { throw "Diagnostic runner is missing: $test" } }
if($runner.IndexOf('[switch]$SkipStartupSmoke', [StringComparison]::Ordinal) -lt 0) { throw 'Diagnostic runner must run startup smoke by default.' }
$tracked = & git -C $repoRoot ls-files
$crashArtifacts = @($tracked | Where-Object { $_ -match '(?i)(^|/)(hs_err_pid[^/]*\.log|replay_pid[^/]*\.log)$' })
if($crashArtifacts.Count) { throw "Tracked crash/replay artifacts found: $($crashArtifacts -join ', ')" }
Write-Host 'Diagnostic runner contract passed.'