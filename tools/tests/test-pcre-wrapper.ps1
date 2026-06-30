[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$legacyPcreDll = Join-Path $repoRoot "third_party\pcre\pcre.dll"

if (-not (Test-Path -LiteralPath $legacyPcreDll)) {
    Write-Host "Пропускаю legacy PCRE wrapper smoke test: файл third_party\\pcre\\pcre.dll отсутствует."
    exit 0
}

& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

$testDir = Join-Path $repoRoot "out\tests\pcre-wrapper"
$testExe = Join-Path $testDir "pcre-wrapper-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $repoRoot "third_party\pcre")" `
    "/I$(Join-Path $repoRoot "third_party\wtl")" `
    "/Fo$(Join-Path $testDir "pcre-wrapper-smoke.obj")" `
    (Join-Path $PSScriptRoot "pcre-wrapper-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $repoRoot "third_party\pcre")" `
    "pcre.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Copy-Item -LiteralPath $legacyPcreDll -Destination $testDir -Force

$fixturesPath = Join-Path $PSScriptRoot "regex-fixtures.json"
$fixtures = Get-Content -Raw -LiteralPath $fixturesPath | ConvertFrom-Json

foreach ($case in $fixtures.pcre.wrapper) {
    $arguments = @(
        (Convert-ToUtf8Hex ([string]$case.subject)),
        (Convert-ToUtf8Hex ([string]$case.pattern)),
        ($(if ($case.ignoreCase) { "1" } else { "0" })),
        ($(if ($case.global) { "1" } else { "0" })),
        ($(if ($case.multiline) { "1" } else { "0" })),
        ([string]$case.expectedCount),
        (Convert-ToUtf8Hex ([string]$case.expectedFirstValue)),
        ([string]$case.expectedFirstIndex)
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Фикстура wrapper-контура PCRE '$($case.id)' завершилась с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Smoke-тест wrapper-контура legacy PCRE прошёл успешно."
