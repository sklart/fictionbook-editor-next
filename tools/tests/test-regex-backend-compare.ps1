[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$legacyPcreDll = Join-Path $repoRoot "third_party\pcre\pcre.dll"

if (-not (Test-Path -LiteralPath $legacyPcreDll)) {
    Write-Host "Пропускаю сравнение PCRE и PCRE2: файл third_party\\pcre\\pcre.dll отсутствует."
    exit 0
}

& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

& (Join-Path $repoRoot "tools\build\build-pcre2.ps1") -Configuration $Configuration -Quiet
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$installDir = Join-Path $repoRoot "build\pcre2\install\$Configuration"
$testDir = Join-Path $repoRoot "out\tests\regex-backend-compare"
$testExe = Join-Path $testDir "regex-backend-compare.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

function Convert-ToUtf8Hex([string]$Text) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return -join ($bytes | ForEach-Object { $_.ToString("x2") })
}

& cl.exe /nologo /EHsc /std:c++17 /MT `
    "/I$(Join-Path $repoRoot "third_party\pcre")" `
    "/I$(Join-Path $repoRoot "third_party\wtl")" `
    "/I$(Join-Path $installDir "include")" `
    "/Fo$(Join-Path $testDir "regex-backend-compare.obj")" `
    (Join-Path $PSScriptRoot "regex-backend-compare.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" `
    "/LIBPATH:$(Join-Path $repoRoot "third_party\pcre")" `
    "/LIBPATH:$(Join-Path $installDir "lib")" `
    "pcre.lib" "pcre2-8-static.lib" "/OUT:$testExe"
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
        ($(if ($case.multiline) { "1" } else { "0" }))
    )

    Push-Location $testDir
    try {
        & $testExe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Сравнение regex-backend для кейса '$($case.id)' вернуло код $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Сравнение PCRE и PCRE2 на wrapper-фикстурах прошло успешно."
