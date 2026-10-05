<# CI contract: production shell build scripts, UTF-8 console setup and output locations. #>
[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [switch]$RequireArtifacts
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$workflowPath = Join-Path $root '.github\workflows\build.yml'
$workflow = Get-Content -Raw -LiteralPath $workflowPath

if ($workflow.Contains('experimental-property-handler') -or $workflow -match '(?i)experimental') {
    throw 'CI workflow must not use obsolete experimental shell-integration naming.'
}
foreach ($required in @(
    'Build Win32 shell integration',
    'Build x64 shell integration',
    './tools/build/build-shell-integration.ps1 -Configuration Release -Platform Win32 -PlatformToolset v143',
    './tools/build/build-shell-integration.ps1 -Configuration Release -Platform x64 -PlatformToolset v143',
    './tools/build/Initialize-CiUtf8.ps1',
    './tools/tests/test-first-party-msbuild-policy.ps1',
    './tools/tests/test-localization-preflight.ps1',
    './tools/tests/test-ci-build-contract.ps1 -RequireArtifacts')) {
    if (-not $workflow.Contains($required)) { throw "CI workflow is missing '$required'." }
}
$localizationStep = $workflow.IndexOf('- name: Validate localization preflight')
$validateFixtures = $workflow.IndexOf('- name: Initialize MSBuild policy fixtures')
if ($localizationStep -lt 0 -or $localizationStep -gt $validateFixtures) {
    throw 'Localization preflight must run before validate initializes submodules and later checks.'
}
if ($workflow -match '(?s)- name: Strict first-party native warnings' -or -not $workflow.Contains('StrictFirstPartyWarnings = $true')) {
    throw 'CI must apply strict first-party warnings during the production build without a separate rebuild.'
}
if (-not $workflow.Contains('SkipLocalizationPreflight = $true') -or -not $workflow.Contains('SkipEarlyRuntimeSuites = $true')) {
    throw 'Late CI verification must skip preflight and post-build suites already completed by build.'
}
foreach ($suite in @('test-search-core-suite.ps1')) {
    if ([regex]::Matches($workflow, [regex]::Escape($suite)).Count -ne 1 -or [regex]::Matches((Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\verify-release.ps1')), [regex]::Escape($suite)).Count -ne 1) {
        throw "The '$suite' suite must be invoked once in build and once in the default local verifier."
    }
}
$packageBlock = [regex]::Match($workflow, '(?s)package:.*?publish:').Value
if ($packageBlock -match '(?m)^\s*submodules:\s*recursive\s*$' -or
    $packageBlock -notmatch 'git submodule update --init --depth=1 third_party/scintilla third_party/pcre2 third_party/hunspell third_party/libwebp third_party/openjpeg third_party/libheif third_party/libde265 third_party/aom third_party/lunasvg third_party/uac' -or
    $packageBlock -notmatch 'git -C third_party/lunasvg submodule update --init --depth=1 plutovg') {
    throw 'Package must use the explicit shallow license-submodule checkout, including only the required nested PlutoVG license.'
}
$buildBlock = [regex]::Match($workflow, '(?s)build:.*?package:').Value
if ($buildBlock -notmatch '(?s)- name: Ensure NSIS 3\.13\s*\n\s*if: github\.event_name == ''pull_request''' -or
    $packageBlock -notmatch '(?s)- name: Ensure NSIS 3\.13\s*\n\s*shell: pwsh' -or
    $buildBlock -notmatch '(?s)- name: Smoke NSIS install scopes\s*\n\s*if: github\.event_name == ''pull_request''' -or
    $packageBlock -notmatch '(?s)- name: Smoke NSIS install scopes\s*\n\s*shell: pwsh' -or
    $packageBlock -notmatch 'Smoke packaged installer upgrade and uninstall') {
    throw 'NSIS must run only for PR smoke in build and once for the real package setup on push/tag.'
}
$imageCache = [regex]::Match($workflow, '(?s)- name: Restore prepared image stack cache.*?(?=\n\s*- name:)').Value
foreach ($requiredPath in @('build/libwebp','build/openjpeg','build/aom','build/libde265','build/libheif')) { if ($imageCache -notmatch [regex]::Escape($requiredPath)) { throw "Image cache is missing $requiredPath." } }
foreach ($script in @('tools\build\build-libwebp.ps1','tools\build\build-openjpeg.ps1')) {
    $scriptText = Get-Content -Raw -LiteralPath (Join-Path $root $script)
    foreach ($required in @('Resolve-VsCmake.ps1','CmakeBuildTree.ps1','-Action Prepare','-Action Write')) { if (-not $scriptText.Contains($required)) { throw "$script must use the shared CMake resolver and fingerprint." } }
}
if ($workflow -notmatch '(?m)^\s*build:\s*\r?\n\s*needs:\s*validate\s*$' -or
    $workflow -notmatch '(?m)^\s*package:\s*\r?\n\s*if:.*\r?\n\s*needs:\s*\[validate, build\]\s*$' -or
    $workflow -notmatch '(?m)^\s*publish:\s*\r?\n\s*if:.*\r?\n\s*needs:\s*\[validate, package\]\s*$') {
    throw 'Build, package and publish must depend on successful validate.'
}

foreach ($artifact in @(
        @{ Name = 'editor background runtime diagnostics'; Output = 'editor_background_has_files'; Path = 'out/tests/editor-background-runtime-failure/**' },
        @{ Name = 'failed runtime diagnostics'; Output = 'runtime_has_files'; Path = 'out/tests/runtime-failure/**' }
    )) {
    $pattern = "(?s)- name: Upload $([regex]::Escape($artifact.Name)).*?if: failure\(\) && steps\.runtime-diagnostics\.outputs\.$($artifact.Output) == 'True'.*?path: $([regex]::Escape($artifact.Path)).*?if-no-files-found: error"
    if ($workflow -notmatch $pattern) {
        throw "CI must upload $($artifact.Name) only when its diagnostic files exist."
    }
}

$validateCheckout = [regex]::Match($workflow, '(?s)validate:.*?actions/checkout@v7\s*\r?\n\s*with:(?<options>.*?)\r?\n\s*# validate reads.*?- name: Initialize MSBuild policy fixtures\s*\r?\n\s*shell: pwsh\s*\r?\n\s*run: (?<command>.*?)\r?\n\s*- name: Check generated')
if(-not $validateCheckout.Success -or $validateCheckout.Groups['options'].Value -match '(?m)^\s*submodules:\s*recursive\s*$' -or
    $validateCheckout.Groups['command'].Value -notmatch 'git submodule update --init --depth=1 third_party/lexilla third_party/hunspell') {
    throw 'Validate must initialize only the vendored MSBuild policy fixtures.'
}

foreach ($match in [regex]::Matches($workflow, '(?m)(?:\./|\.\\)(tools[\\/][A-Za-z0-9_.\\/-]+\.ps1)')) {
    $relativePath = $match.Groups[1].Value -replace '/', '\\'
    if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf)) {
        throw "CI workflow references missing script: $relativePath"
    }
}

foreach ($scriptName in @('build-libde265.ps1', 'build-aom.ps1', 'build-libheif.ps1')) {
    $scriptText = Get-Content -Raw -LiteralPath (Join-Path $root "tools\build\$scriptName")
    if (-not $scriptText.Contains("'-DCMAKE_SYSTEM_VERSION=6.1'") -or
        $scriptText -match '(?<![\x27\x22])-DCMAKE_SYSTEM_VERSION=6\.1') {
        throw "$scriptName must pass CMAKE_SYSTEM_VERSION=6.1 as one quoted native argument."
    }
}

$buildScript = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\build.ps1')
if ([regex]::Matches($buildScript, '(?i)\$msbuild[^\r\n]*?/nr:false').Count -lt 2) {
    throw 'The public build entry point must disable MSBuild node reuse for both solution and required-project builds.'
}
foreach ($required in @('Get-FirstPartyToolchainFingerprint', 'Test-FirstPartyToolchainFingerprint', 'first-party-{0}-{1}.json', '/t:Clean')) {
    if (-not $buildScript.Contains($required)) {
        throw "The public build entry point is missing the first-party toolchain fingerprint contract: $required"
    }
}

$utf8Bootstrap = Join-Path $root 'tools\build\Initialize-CiUtf8.ps1'
& $utf8Bootstrap
$expected = 'Проверка UTF-8: Ёж'
$nativeOutput = (& cmd.exe /d /c "echo $expected").Trim()
if ($nativeOutput -ne $expected -or $nativeOutput -match ('[?' + [char]0xFFFD + ']')) {
    throw "UTF-8 console regression: expected '$expected', got '$nativeOutput'."
}

if ($RequireArtifacts) {
    foreach ($platform in @('Win32', 'x64')) {
        $artifact = Join-Path $root "out\package\shell-build\$platform\$Configuration\FBShell.dll"
        if (-not (Test-Path -LiteralPath $artifact -PathType Leaf) -or (Get-Item -LiteralPath $artifact).Length -eq 0) {
            throw "Shell integration artifact is missing or empty: $artifact"
        }
    }
}

Write-Host 'CI shell integration and UTF-8 contract passed.'
