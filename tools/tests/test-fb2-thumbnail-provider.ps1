[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot "out\tests\fb2-thumbnail-provider"
$testExe = Join-Path $testDir "fb2-thumbnail-provider-smoke.exe"
$pngFixture = Join-Path $PSScriptRoot "fb2-cover-smoke.fb2"
$jpegFixture = Join-Path $PSScriptRoot "fb2-cover-jpeg-smoke.fb2"
$bmpFixture = Join-Path $PSScriptRoot "fb2-cover-bmp-smoke.fb2"
$brokenFixture = Join-Path $PSScriptRoot "fb2-cover-broken.fb2"
$missingCoverFixture = Join-Path $PSScriptRoot "fb2-cover-missing-coverpage.fb2"
$missingBinaryFixture = Join-Path $PSScriptRoot "fb2-cover-missing-binary.fb2"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/I$(Join-Path $repoRoot "src\fbe")" `
    "/I$(Join-Path $repoRoot "src\fbshell")" `
    "/I$(Join-Path $repoRoot "third_party\wtl")" `
    "/Fo$($testDir)\\" `
    (Join-Path $repoRoot "src\fbe\Fb2CoverImage.cpp") `
    (Join-Path $repoRoot "src\fbe\Fb2CoverThumbnail.cpp") `
    (Join-Path $repoRoot "src\fbshell\AtlImageStatics.cpp") `
    (Join-Path $repoRoot "src\fbshell\Fb2ThumbnailProvider.cpp") `
    (Join-Path $PSScriptRoot "fb2-thumbnail-provider-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "oleaut32.lib" "comsuppw.lib" "gdiplus.lib" "propsys.lib" "shlwapi.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $testDir
try {
    & $testExe $pngFixture $jpegFixture $bmpFixture $brokenFixture $missingCoverFixture $missingBinaryFixture
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke-тест COM thumbnail provider завершился с кодом $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Smoke-тест COM thumbnail provider прошёл успешно."

