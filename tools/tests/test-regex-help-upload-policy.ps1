<# Checks producer/upload gating; actual artifact transfer is verified by CI. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$workflow = Get-Content (Join-Path $root '.github/workflows/build.yml') -Raw
$producer = [regex]::Match($workflow, '(?s)- name: Run Full Regex Help regressions.*?(?=\n      - name:)').Value
$upload = [regex]::Match($workflow, '(?s)- name: Upload Full Regex Help visual smoke captures.*?(?=\n      - name:)').Value
if ($producer -notmatch 'id: full-regex-help' -or $upload -notmatch 'if-no-files-found: error') {
    throw 'Regex Help must retain a named producer and mandatory artifact validation.'
}
$condition = [regex]::Match($upload, '(?m)^\s*if: (.+)$').Groups[1].Value.Trim()
if ($condition -ne "always() && steps.full-regex-help.outcome != 'skipped' && steps.full-regex-help.outcome != ''") {
    throw 'Upload must run after success/failure, but never after a skipped/unstarted producer.'
}
foreach ($case in @(
    @{ Outcome='skipped'; Files=$false; Expected='SKIP' },
    @{ Outcome=''; Files=$false; Expected='SKIP' },
    @{ Outcome='success'; Files=$true; Expected='UPLOAD' },
    @{ Outcome='failure'; Files=$true; Expected='UPLOAD' },
    @{ Outcome='success'; Files=$false; Expected='ERROR' },
    @{ Outcome='failure'; Files=$false; Expected='ERROR' }
)) {
    # Evaluate the checked GitHub condition, then upload-artifact's error policy.
    $expression = $condition.Replace('always()', '$true').Replace('steps.full-regex-help.outcome', '$case.Outcome').Replace('&&', '-and').Replace('!=', '-ne')
    $run = & ([scriptblock]::Create($expression))
    $actual = if (!$run) { 'SKIP' } elseif ($case.Files) { 'UPLOAD' } else { 'ERROR' }
    if ($actual -ne $case.Expected) { throw "Upload policy mismatch: $($case.Outcome)/$($case.Files)" }
}
Write-Host 'Regex Help upload policy: PASS (six producer/artifact cases; transfer requires CI).'
