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
Write-Host 'CI duplicate-suite contract passed.'
