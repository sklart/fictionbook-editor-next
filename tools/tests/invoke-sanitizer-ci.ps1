[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Address','Undefined')][string]$Sanitizer,
    [switch]$Focused,
    [switch]$AllowUserComRegistration
)
$ErrorActionPreference = 'Stop'
# IDs are resolved and checked against the existing release catalog by the runner.
$ids = @(
    'release.fbe-binary-production-roundtrip', 'release.fbe-file-close-runtime',
    'release.fbe-save-decision-runtime', 'release.fbe-malformed-source-fallback-runtime',
    'release.fbe-editor-view-lifecycle-runtime', 'release.fbe-table-attributes-runtime',
    'release.issue64-runtime'
)
$arguments = @{ Sanitizer = $Sanitizer; Contour = if ($Focused) { 'Fast' } else { 'Full' } }
if ($Focused) { $arguments.Id = $ids }
if ($AllowUserComRegistration) { $arguments.AllowUserComRegistration = $true }
$watch = [Diagnostics.Stopwatch]::StartNew()
try { & (Join-Path $PSScriptRoot 'run-sanitizers.ps1') @arguments }
finally {
    $watch.Stop()
    if ($env:GITHUB_STEP_SUMMARY) {
        $scope = if ($Focused) { 'focused (partial selection)' } else { 'full' }
        "Sanitizer $Sanitizer runtime: $scope; $([math]::Round($watch.Elapsed.TotalSeconds, 1)) s." | Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8
    }
}
