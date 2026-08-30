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
    './tools/tests/test-ci-build-contract.ps1 -RequireArtifacts')) {
    if (-not $workflow.Contains($required)) { throw "CI workflow is missing '$required'." }
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

$utf8Bootstrap = Join-Path $root 'tools\build\Initialize-CiUtf8.ps1'
& $utf8Bootstrap
$expected = 'Проверка UTF-8: Ёж'
$nativeOutput = (& cmd.exe /d /c "echo $expected").Trim()
if ($nativeOutput -ne $expected -or $nativeOutput -match '[?�]') {
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
