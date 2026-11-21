[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$workflow = Get-Content -LiteralPath (Join-Path $root '.github/workflows/build.yml') -Raw
$runner = Get-Content -LiteralPath (Join-Path $root 'tools/tests/invoke-sanitizer-ci.ps1') -Raw
foreach ($cache in @('pcre2-cache','hunspell-cache','image-stack-cache','editor-runtime-cache')) {
    if ($workflow -notmatch "(?s)actions/cache/restore@v6\s*\r?\n\s*id: $cache") { throw "Missing explicit restore for $cache" }
    if ($workflow -notmatch "(?s)if: steps\.$cache\.outputs\.cache-hit != 'true'\s*\r?\n\s*uses: actions/cache/save@v6\s*\r?\n\s*with:.*?key: \`$\{\{ steps\.$cache\.outputs\.cache-primary-key \}\}") { throw "Missing conditional save for $cache" }
}
$compile = $workflow.IndexOf('- name: Build Release Win32')
$save = $workflow.IndexOf('- name: Save PCRE2 cache')
$runtime = $workflow.IndexOf('- name: Run XML source theme v1 runtime regression')
if ($compile -lt 0 -or $save -le $compile -or $runtime -le $save) { throw 'Caches must be saved after a successful build and before runtime tests.' }
foreach ($required in @('Get-FileHash -LiteralPath $compiler','VCToolsVersion','ImageVersion')) {
    if (-not $workflow.Contains($required)) { throw "Missing cache toolchain identity: $required" }
}
$build = Get-Content -LiteralPath (Join-Path $root 'tools/build/build.ps1') -Raw
if ($build -notmatch '(?s)if \(\$ReusePreparedPcre2\).*?& pwsh @pcre2BuildArgs' -or
    $build -notmatch '(?s)if \(\$ReusePreparedImageStack\).*?CmakeBuildTree\.ps1.*?-Action Verify') { throw 'Prepared dependencies must validate their toolchain fingerprints.' }
$catalog = (& (Join-Path $PSScriptRoot 'get-release-test-catalog.ps1') -AsJson -Validate) | ConvertFrom-Json
$ids = @([regex]::Matches($runner, "'(?<id>release\.[^']+)'") | ForEach-Object { $_.Groups['id'].Value })
if ($ids.Count -lt 1 -or $ids.Count -ne @($ids | Select-Object -Unique).Count) { throw 'Focused sanitizer IDs must be nonempty and unique.' }
foreach ($id in $ids) {
    $entry = @($catalog.tests | Where-Object id -eq $id)
    if ($entry.Count -ne 1 -or -not $entry[0].sanitizer.eligible -or 'FAST' -notin $entry[0].sanitizer.contours) { throw "Ineligible focused case: $id" }
}
foreach ($sanitizer in @('asan','ubsan')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root ".github/workflows/$sanitizer.yml")
    foreach ($part in @('workflow_call:', 'schedule:', 'github.ref_type == ''tag''', 'github.event_name == ''schedule''', 'github.event_name == ''pull_request''', '-Focused', 'write-sanitizer-ci-summary.ps1')) {
        if (-not $text.Contains($part)) { throw "$sanitizer is missing $part" }
    }
    $group = 'group: ' + $sanitizer + '-${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}'
    if (-not $text.Contains($group)) { throw "$sanitizer reusable and standalone runs must have distinct concurrency groups." }
    if ($text -notmatch '(?s)name: Run (?:sanitizer|UBSan) FULL.*?invoke-sanitizer-ci\.ps1 -Sanitizer (?:Address|Undefined) -AllowUserComRegistration') { throw "$sanitizer FULL must explicitly opt into COM registration on its hosted runner." }
    if ($text -match "(?m)^\s*tags:\s*\[") { throw "$sanitizer would duplicate the tag-triggered reusable job" }
    if ($workflow -notmatch "(?s)release-${sanitizer}:\s*\r?\n\s*if: github.ref_type == 'tag'.*\r?\n\s*uses: ./\.github/workflows/$sanitizer.yml") { throw "Release must call $sanitizer on the tagged SHA" }
}
. (Join-Path $PSScriptRoot 'SanitizerCatalog.ps1')
$testSha = 'a' * 40
Assert-FbeSanitizerProvenance -Head $testSha -BuildCommit $testSha
$rejected = $false
try { Assert-FbeSanitizerProvenance -Head $testSha -BuildCommit ('b' * 40) } catch { $rejected = $true }
if (-not $rejected) { throw 'Sanitizer build from another SHA must be rejected.' }
$registrationCase = @(@{ name='release.import-epub-registration.1' })
$rejected = $false
try { Assert-FbeSanitizerRegistrationIsolation -Cases $registrationCase } catch { $rejected = $true }
if (-not $rejected) { throw 'COM registration must be rejected without an explicit disposable-profile opt-in.' }
$rejected = $false
try { Assert-FbeSanitizerRegistrationIsolation -Cases @(@{ name='custom.1'; command='test-import-epub-registration.ps1 -AllowUserRegistration' }) } catch { $rejected = $true }
if (-not $rejected) { throw 'Custom registration cases must also require a disposable-profile opt-in.' }
Assert-FbeSanitizerRegistrationIsolation -Cases $registrationCase -AllowUserComRegistration
Assert-FbeSanitizerRegistrationIsolation -Cases @(@{ name='release.fbe-file-close-runtime.1' })
$sanitizerRunner = Get-Content -Raw -LiteralPath (Join-Path $root 'tools/tests/run-sanitizers.ps1')
if ($sanitizerRunner -notmatch '(?s)Assert-FbeSanitizerRegistrationIsolation.*?foreach\(\$case in \$cases\)') { throw 'COM registration isolation must be checked before executing cases.' }
$scriptsWorkflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github/workflows/check-scripts.yml')
if (-not $scriptsWorkflow.Contains('group: check-scripts-${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}')) { throw 'Reusable and standalone script checks must have distinct concurrency groups.' }
if ($workflow -notmatch 'needs: \[validate, package, release-asan, release-ubsan, release-scripts, release-docs\]') { throw 'Publication must wait for both sanitizers.' }
foreach ($check in @('scripts','docs')) {
    $file = if ($check -eq 'scripts') { 'check-scripts' } else { 'docs' }
    if ($workflow -notmatch "(?s)release-${check}:\s*\r?\n\s*if: github.ref_type == 'tag'\s*\r?\n\s*uses: ./\.github/workflows/$file.yml") { throw "Missing release $check gate." }
    if (-not (Get-Content -Raw (Join-Path $root ".github/workflows/$file.yml")).Contains('workflow_call:')) { throw "Missing reusable $check workflow." }
}
if ($workflow -notmatch "(?s)manual-full-gate:.*?always\(\).*?needs: \[validate, package, release-asan, release-ubsan\].*?needs.release-asan.result != 'success'.*?needs.release-ubsan.result != 'success'") { throw 'Manual full gate must reject skipped or failed sanitizer jobs.' }
foreach ($sanitizer in @('asan','ubsan')) {
    if ($workflow -notmatch "(?s)release-${sanitizer}:.*?workflow_dispatch.*?inputs.full_validation.*?uses: ./\.github/workflows/$sanitizer.yml\s*\r?\n\s*with:\s*\r?\n\s*full_validation: true") { throw "Manual FULL must explicitly call $sanitizer." }
}
if (-not $runner.Contains("if (`$Focused) { `$arguments.Id = `$ids }")) { throw 'Focused coverage must use the existing -Id selector.' }
Write-Host "CI cache and sanitizer gate contract passed; focused catalog IDs: $($ids.Count)."
