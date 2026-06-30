[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot "out\tests\fb2-cover"
$testExe = Join-Path $testDir "fb2-cover-smoke.exe"
$validFixture = Join-Path $PSScriptRoot "fb2-cover-smoke.fb2"
$brokenFixture = Join-Path $PSScriptRoot "fb2-cover-broken.fb2"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/I$(Join-Path $repoRoot "src\fbe")" `
    "/Fo$($testDir)\\" `
    (Join-Path $repoRoot "src\fbe\Fb2CoverImage.cpp") `
    (Join-Path $PSScriptRoot "fb2-cover-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "oleaut32.lib" "comsuppw.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $testDir
try {
    & $testExe $validFixture $brokenFixture
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke-тест чтения обложки FB2 завершился с кодом $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Smoke-тест чтения обложки FB2 прошёл успешно."
