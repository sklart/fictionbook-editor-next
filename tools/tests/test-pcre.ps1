[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$legacyPcreDll = Join-Path $repoRoot "third_party\pcre\pcre.dll"

if (-not (Test-Path -LiteralPath $legacyPcreDll)) {
    Write-Host "Пропускаю legacy PCRE smoke test: файл third_party\\pcre\\pcre.dll отсутствует."
    exit 0
}

& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot "out\tests\pcre"
$testExe = Join-Path $testDir "pcre-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $repoRoot "third_party\pcre")" `
    "/Fo$(Join-Path $testDir "pcre-smoke.obj")" `
    (Join-Path $PSScriptRoot "pcre-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $repoRoot "third_party\pcre")" `
    "pcre.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Copy-Item -LiteralPath $legacyPcreDll -Destination $testDir -Force

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
        ($(if ($case.expectedMatch) { "1" } else { "0" }))
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Фикстура PCRE '$($case.id)' завершилась с кодом $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Smoke-тест runtime-контура legacy PCRE прошёл успешно."
