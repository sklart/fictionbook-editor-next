<# Verifies that CI executes each transferred early suite once. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\build.yml')
$verify = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\verify-release.ps1')
if (-not $workflow.Contains('SkipEarlyRuntimeSuites = $true') -or -not $verify.Contains('[switch]$SkipEarlyRuntimeSuites')) { throw 'CI must explicitly skip suites already run before verify-release.' }
foreach ($scenario in @('test-search-core-suite.ps1','test-export-html-writer.ps1','test-export-html-split.ps1','test-export-html-split-e2e.ps1','test-script-toolbar-lifecycle-runtime.ps1','test-fbe-navigation-scripts-runtime.ps1','test-fbe-script-startup-validation-runtime.ps1','test-fbe-xml-source-theme-v1-runtime.ps1')) {
    if ([regex]::Matches($workflow, [regex]::Escape($scenario)).Count -ne 1) { throw "$scenario must be scheduled exactly once by CI." }
}
if ($workflow -notmatch '(?s)Build Release Win32.*?test-fbe-xml-source-theme-v1-runtime\.ps1.*?-FbeExe \./out/Release/FBE\.exe') { throw 'XML source theme runtime must run directly after the Release Win32 build.' }
if ($verify -notmatch '(?s)if \(-not \$SkipEarlyRuntimeSuites\) \{\s*& \(Join-Path \$repoRoot "tools\\tests\\test-fbe-xml-source-theme-v1-runtime\.ps1"\)') { throw 'verify-release must skip the XML source theme runtime in CI.' }
foreach ($static in @('test-plugin-localization-catalog.ps1','test-runtime-lang-export.ps1','test-search-preset-localization.ps1','test-regex-help-runtime-localization.ps1')) {
    if ([regex]::Matches($workflow, [regex]::Escape($static)).Count -gt 1) { throw "$static must not be scheduled twice by CI." }
    if ($verify.Contains($static)) { throw "$static must run exclusively through localization preflight." }
}
foreach ($blockStart in @('if (-not $SkipEarlyRuntimeSuites) {')) {
    if (-not $verify.Contains($blockStart)) { throw 'Default local verifier must retain early suites behind the CI-only skip.' }
}
if ($workflow.Contains('test-interface-theme-policy-runtime.ps1') -or
    [regex]::Matches($verify, 'test-interface-theme-policy-runtime\.ps1').Count -ne 1 -or
    $verify -notmatch '(?m)^& \(Join-Path \$repoRoot "tools\\tests\\test-interface-theme-policy-runtime\.ps1"\) -FbeExe') {
    throw 'Interface theme policy runtime must run once through the mandatory verify-release contour.'
}
# Icon-size stress is already part of lifecycle in both profiles. Do not also
# schedule the standalone convenience runner and repeat its 100 size switches.
if ($workflow.Contains('test-script-icon-size-runtime.ps1') -or $verify.Contains('test-script-icon-size-runtime.ps1')) {
    throw 'Standalone icon-size runner duplicates lifecycle stress coverage.'
}
$lifecycle = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/tests/test-script-toolbar-lifecycle-runtime.ps1')
foreach ($mode in @('portable','installed')) {
    foreach ($scenario in @('script-toolbar-runtime-size','script-icon-size-read')) {
        if ([regex]::Matches($lifecycle, [regex]::Escape("Invoke-Lifecycle '--$mode' '$scenario'")).Count -ne 1) {
            throw "Lifecycle must cover $mode/$scenario exactly once."
        }
    }
}
$tokens = $null; $errors = $null
$verifyAst = [Management.Automation.Language.Parser]::ParseInput($verify, [ref]$tokens, [ref]$errors)
$themeCall = $verifyAst.Find({ param($node)
    $node -is [Management.Automation.Language.CommandAst] -and $node.Extent.Text.Contains('test-interface-theme-policy-runtime.ps1')
}, $true)
for ($parent = $themeCall.Parent; $parent; $parent = $parent.Parent) {
    if ($parent -is [Management.Automation.Language.IfStatementAst]) {
        throw 'Theme policy runtime must be unconditional, including SkipEarlyRuntimeSuites and FAST.'
    }
}
# The P0 contour is owned by verify-release and must not be duplicated in CI.
if ([regex]::Matches($verify, 'invoke-prerelease-p0\.ps1').Count -ne 1 -or $workflow.Contains('invoke-prerelease-p0.ps1')) { throw 'P0 runner must execute exactly once through verify-release.' }
if (-not $workflow.Contains('out/tests/prerelease-p0/**') -or -not $workflow.Contains('name: prerelease-p0-${{ github.sha }}')) { throw 'CI must publish SHA-labelled P0 evidence.' }
if ($verify -notmatch '(?s)if \(\$FullValidation\) \{ throw "FullValidation INCOMPLETE:') { throw 'FULL must fail if the verifier reports skipped mandatory checks.' }
$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/tests/invoke-prerelease-p0.ps1')
foreach ($test in @('test-fbe-issue72-runtime.ps1','test-fbe-issue72-save-runtime.ps1','test-fbe-save-timer-runtime.ps1','test-fbe-recovery-history-runtime.ps1','test-fbe-binary-dirty-runtime.ps1','test-fbe-reentrant-close-runtime.ps1','test-fbe-script-form-dirty-runtime.ps1')) {
    if (-not $runner.Contains($test)) { throw "P0 runner is missing $test." }
}
if (-not $runner.Contains('executableSha256') -or -not $runner.Contains('GITHUB_SHA') -or -not $runner.Contains("status='RUNNING'") -or -not $runner.Contains("status='FAIL'")) { throw 'P0 reports must identify the source and binary and distinguish incomplete from failure.' }
$probe = Join-Path ([IO.Path]::GetTempPath()) ('fbe-p0-preflight-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $probe | Out-Null
    $missingExe = Join-Path $probe 'missing.exe'
    $process = Start-Process -FilePath (Get-Process -Id $PID).Path -Wait -PassThru -WindowStyle Hidden -ArgumentList @(
        '-NoProfile', '-File', ('"' + (Join-Path $root 'tools/tests/invoke-prerelease-p0.ps1') + '"'),
        '-FbeExe', ('"' + $missingExe + '"'), '-OutputDirectory', ('"' + $probe + '"'))
    $reports = @(Get-ChildItem -LiteralPath $probe -Recurse -Filter summary.json)
    if ($process.ExitCode -eq 0 -or $reports.Count -ne 1) { throw 'P0 preflight failure must produce one report and a nonzero exit.' }
    $report = Get-Content -Raw -LiteralPath $reports[0].FullName | ConvertFrom-Json
    if ($report.status -ne 'FAIL' -or -not $report.error.Contains('missing.exe') -or $report.sha -notmatch '^[0-9a-f]{40}$' -or $report.results.Count -ne 0) {
        throw 'P0 preflight report must identify the SHA and describe failure before any test runs.'
    }
} finally {
    if (Test-Path -LiteralPath $probe) { Remove-Item -LiteralPath $probe -Recurse -Force }
}
Write-Host 'CI duplicate-suite contract passed.'
