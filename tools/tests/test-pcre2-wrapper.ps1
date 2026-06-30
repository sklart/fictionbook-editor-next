[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

& (Join-Path $repoRoot "tools\build\build-pcre2.ps1") -Configuration $Configuration -Quiet
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$installDir = Join-Path $repoRoot "build\pcre2\install\$Configuration"
$testDir = Join-Path $repoRoot "out\tests\pcre2-wrapper"
$testExe = Join-Path $testDir "pcre2-wrapper-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $repoRoot "third_party\wtl")" `
    "/I$(Join-Path $installDir "include")" `
    "/Fo$(Join-Path $testDir "pcre2-wrapper-smoke.obj")" `
    (Join-Path $PSScriptRoot "pcre2-wrapper-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $installDir "lib")" `
    "pcre2-8-static.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$fixturesPath = Join-Path $PSScriptRoot "regex-fixtures.json"
$fixtures = Get-Content -Raw -LiteralPath $fixturesPath | ConvertFrom-Json

foreach ($case in $fixtures.pcre.wrapper) {
    $expectedFirstSubMatchCount = 0
    $expectedFirstSubMatchValue = ""
    if ($case.expectedFirstSubMatchCount -ne $null) {
        $expectedFirstSubMatchCount = [int]$case.expectedFirstSubMatchCount
    } elseif ($case.expectedSubMatches -ne $null) {
        $expectedFirstSubMatchCount = @($case.expectedSubMatches).Count
    }

    if ($case.expectedFirstSubMatchValue -ne $null) {
        $expectedFirstSubMatchValue = [string]$case.expectedFirstSubMatchValue
    } elseif ($case.expectedSubMatches -ne $null -and @($case.expectedSubMatches).Count -gt 0) {
        $expectedFirstSubMatchValue = [string]$case.expectedSubMatches[0]
    }

    $arguments = @(
        (Convert-ToUtf8Hex ([string]$case.subject)),
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        ($(if ($case.ignoreCase) { "1" } else { "0" })),
        ($(if ($case.global) { "1" } else { "0" })),
        ($(if ($case.multiline) { "1" } else { "0" })),
        ([string]$case.expectedCount),
        (Convert-ToUtf8Hex ([string]$case.expectedFirstValue)),
        ([string]$case.expectedFirstIndex),
        ($(if ($case.expectedCompileError) { "1" } else { "0" })),
        ([string]$expectedFirstSubMatchCount),
        (Convert-ToUtf8Hex $expectedFirstSubMatchValue)
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "PCRE2 wrapper-фикстура '$($case.id)' завершилась с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Smoke-тест wrapper-контура PCRE2 прошёл успешно."
