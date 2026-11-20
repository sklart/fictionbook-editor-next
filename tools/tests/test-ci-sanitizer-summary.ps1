[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-ci-summary-' + [guid]::NewGuid().ToString('N'))
$saved = $env:GITHUB_STEP_SUMMARY
$savedStatus = $env:SANITIZER_JOB_STATUS
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $env:GITHUB_STEP_SUMMARY = Join-Path $root 'summary.md'
    $results = Join-Path $root 'results'; New-Item -ItemType Directory -Path $results | Out-Null
    $env:SANITIZER_JOB_STATUS = 'success'
    $cases = @(
        @{ label='NOT RUN'; data=$null; expected='scope: NOT RUN' },
        @{ label='fallback'; data=@{status='FAIL'; stage='build or infrastructure'}; expected='scope: INCOMPLETE' },
        @{ label='interrupted'; data=@{status='FAIL'; selectionComplete=$false; expectedCount=2; results=@(@{name='ok';status='PASS'})}; expected='scope: INCOMPLETE' },
        @{ label='empty interrupted'; data=@{status='RUNNING'; selectionComplete=$true; expectedCount=1; plannedCases=@('one'); results=@()}; expected='scope: INCOMPLETE' },
        @{ label='fast contour'; data=@{status='PASS'; selectionComplete=$true; contour='Fast'; expectedCount=1; results=@(@{name='ok';status='PASS'})}; expected='scope: PARTIAL' },
        @{ label='partial'; data=@{status='PASS'; selectionComplete=$false; expectedCount=1; results=@(@{name='ok';status='PASS'})}; expected='scope: PARTIAL' },
        @{ label='complete'; data=@{status='PASS'; selectionComplete=$true; expectedCount=1; results=@(@{name='ok';status='PASS'})}; expected='scope: COMPLETE' },
        @{ label='complete failures'; data=@{status='FAIL'; selectionComplete=$true; expectedCount=1; results=@(@{name='broken';status='FAIL'})}; expected='FAIL 1' }
    )
    foreach ($case in $cases) {
        $summary = Join-Path $results 'summary.json'
        if (Test-Path $summary) { Remove-Item -LiteralPath $summary }
        if (Test-Path $env:GITHUB_STEP_SUMMARY) { Remove-Item -LiteralPath $env:GITHUB_STEP_SUMMARY }
        if ($case.data) { $case.data | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summary -Encoding utf8 }
        & (Join-Path $PSScriptRoot 'write-sanitizer-ci-summary.ps1') -Directory asan -ResultsRoot $results
        $text = Get-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Raw
        if (-not $text.Contains($case.expected)) { throw "Incorrect summary for $($case.label): $text" }
        if ($case.label -eq 'complete failures' -and (-not $text.Contains('scope: COMPLETE') -or -not $text.Contains('broken'))) { throw 'Completed failures must be visible.' }
    }
    Write-Host 'Sanitizer CI summary fixture passed.'
} finally {
    $env:GITHUB_STEP_SUMMARY = $saved
    $env:SANITIZER_JOB_STATUS = $savedStatus
    foreach ($file in @((Join-Path $root 'summary.md'), (Join-Path $root 'results/summary.json'))) {
        if (Test-Path -LiteralPath $file -PathType Leaf) { Remove-Item -LiteralPath $file -Force }
    }
    $resultsDirectory = Join-Path $root 'results'
    if (Test-Path -LiteralPath $resultsDirectory -PathType Container) { Remove-Item -LiteralPath $resultsDirectory -Force }
    if (Test-Path -LiteralPath $root -PathType Container) { Remove-Item -LiteralPath $root -Force }
}
