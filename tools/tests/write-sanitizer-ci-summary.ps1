[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('asan','ubsan')][string]$Directory, [string]$ResultsRoot)
$ErrorActionPreference = 'Stop'
if (-not $env:GITHUB_STEP_SUMMARY) { return }
$root = if ($ResultsRoot) { $ResultsRoot } else { Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path "out/tests/$Directory" }
$files = @(Get-ChildItem -LiteralPath $root -Recurse -Filter summary.json -File -ErrorAction SilentlyContinue)
$rows = @(); $missing = 0; $failed = @(); $partial = $false
foreach ($file in $files) {
    $data = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $results = @($data.results | Where-Object { $null -ne $_ })
    $rows += $results
    if ($null -ne $data.expectedCount) { $missing += [math]::Max(0, [int]$data.expectedCount - $results.Count) }
    if ($data.selectionComplete -eq $false) { $partial = $true }
    if ($data.status -eq 'FAIL' -and -not @($data.results | Where-Object status -eq 'FAIL').Count) { $failed += 'build or infrastructure' }
}
$failed += @($rows | Where-Object status -eq 'FAIL' | ForEach-Object name)
$pass = @($rows | Where-Object status -eq 'PASS').Count
$fail = @($rows | Where-Object status -eq 'FAIL').Count
$skip = @($rows | Where-Object status -eq 'SKIP').Count
$scope = if (-not $files.Count) { 'NOT RUN' } elseif ($partial) { 'PARTIAL selection' } else { 'complete contour' }
$artifactUrl = "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID#artifacts"
@(
    "### $Directory sanitizer", "Job: $env:SANITIZER_JOB_STATUS; scope: $scope",
    "Cases: PASS $pass / FAIL $fail / SKIP $skip / planned but not run $missing.",
    "Failed: $(if ($failed.Count) { $failed -join ', ' } else { 'none reported' }).",
    "[Run artifacts]($artifactUrl)"
) | Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8
