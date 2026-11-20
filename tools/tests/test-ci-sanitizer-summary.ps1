[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-ci-summary-' + [guid]::NewGuid().ToString('N'))
$saved = $env:GITHUB_STEP_SUMMARY
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $env:GITHUB_STEP_SUMMARY = Join-Path $root 'summary.md'
    $results = Join-Path $root 'results'; New-Item -ItemType Directory -Path $results | Out-Null
    @{ status='FAIL'; selectionComplete=$false; expectedCount=4; results=@(@{name='ok';status='PASS'},@{name='broken';status='FAIL'},@{name='skipped';status='SKIP'}) } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $results 'summary.json') -Encoding utf8
    & (Join-Path $PSScriptRoot 'write-sanitizer-ci-summary.ps1') -Directory asan -ResultsRoot $results
    $text = Get-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Raw
    foreach ($expected in @('PARTIAL selection','PASS 1 / FAIL 1 / SKIP 1 / planned but not run 1','broken')) {
        if (-not $text.Contains($expected)) { throw "Missing or incorrect summary: $expected" }
    }
    Remove-Item -LiteralPath (Join-Path $results 'summary.json')
    & (Join-Path $PSScriptRoot 'write-sanitizer-ci-summary.ps1') -Directory ubsan -ResultsRoot $results
    if ((Get-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Raw) -notmatch 'scope: NOT RUN') { throw 'Missing results must not be reported as PASS.' }
    Write-Host 'Sanitizer CI summary fixture passed.'
} finally {
    $env:GITHUB_STEP_SUMMARY = $saved
    foreach ($file in @((Join-Path $root 'summary.md'), (Join-Path $root 'results/summary.json'))) {
        if (Test-Path -LiteralPath $file -PathType Leaf) { Remove-Item -LiteralPath $file -Force }
    }
    $resultsDirectory = Join-Path $root 'results'
    if (Test-Path -LiteralPath $resultsDirectory -PathType Container) { Remove-Item -LiteralPath $resultsDirectory -Force }
    if (Test-Path -LiteralPath $root -PathType Container) { Remove-Item -LiteralPath $root -Force }
}
