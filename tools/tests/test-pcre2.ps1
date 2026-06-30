[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$buildPcre2Arguments = @{
    Configuration = $Configuration
    Quiet = $true
}
if ($PlatformToolset) {
    $buildPcre2Arguments.PlatformToolset = $PlatformToolset
}
& (Join-Path $repoRoot "tools\build\build-pcre2.ps1") @buildPcre2Arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$installDir = Join-Path $repoRoot "build\pcre2\install\$Configuration"
$testDir = Join-Path $repoRoot "out\tests\pcre2"
$testExe = Join-Path $testDir "pcre2-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $installDir "include")" `
    "/Fo$(Join-Path $testDir "pcre2-smoke.obj")" `
    (Join-Path $PSScriptRoot "pcre2-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $installDir "lib")" `
    "pcre2-8-static.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$fixturesPath = Join-Path $PSScriptRoot "regex-fixtures.json"
$fixtures = Get-Content -Raw -LiteralPath $fixturesPath | ConvertFrom-Json

foreach ($case in $fixtures.pcre.search) {
    $optionsText = ""
    if ($case.options) {
        $optionsText = (($case.options | ForEach-Object { [string]$_ }) -join ",")
    }

    $arguments = @(
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        (Convert-ToUtf8Hex ([string]$case.subject)),
        $optionsText,
        ($(if ($case.expectedMatch) { "1" } else { "0" })),
        ($(if ($case.expectedCompileError) { "1" } else { "0" }))
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "PCRE2-фикстура '$($case.id)' завершилась с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Smoke-тест runtime-контура PCRE2 прошёл успешно."
