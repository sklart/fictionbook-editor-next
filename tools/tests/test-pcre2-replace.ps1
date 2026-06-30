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
$testDir = Join-Path $repoRoot "out\tests\pcre2-replace"
$testExe = Join-Path $testDir "pcre2-replace-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE /D_UNICODE `
    "/I$(Join-Path $repoRoot "third_party\wtl")" `
    "/I$(Join-Path $installDir "include")" `
    "/Fo$(Join-Path $testDir "pcre2-replace-smoke.obj")" `
    (Join-Path $PSScriptRoot "pcre2-replace-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $installDir "lib")" `
    "pcre2-8-static.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$fixturesPath = Join-Path $PSScriptRoot "regex-fixtures.json"
$fixtures = Get-Content -Raw -LiteralPath $fixturesPath | ConvertFrom-Json

foreach ($case in $fixtures.pcre.replace) {
    $arguments = @(
        (Convert-ToUtf8Hex ([string]$case.subject)),
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        (Convert-ToUtf8Hex ([string]$case.replacement)),
        ($(if ($case.ignoreCase) { "1" } else { "0" })),
        ($(if ($case.global) { "1" } else { "0" })),
        ($(if ($case.multiline) { "1" } else { "0" })),
        (Convert-ToUtf8Hex ([string]$case.expectedText))
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "PCRE2 replace-фикстура '$($case.id)' завершилась с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Smoke-тест замены PCRE2 прошёл успешно."
