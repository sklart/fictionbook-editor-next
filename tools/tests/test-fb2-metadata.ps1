[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot "out\tests\fb2-metadata"
$testExe = Join-Path $testDir "fb2-metadata-smoke.exe"
$fixture = Join-Path $PSScriptRoot "fb2-metadata-smoke.fb2"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/I$(Join-Path $repoRoot "src\fbe")" `
    "/I$(Join-Path $repoRoot "src\fbshell")" `
    "/Fo$($testDir)\\" `
    (Join-Path $repoRoot "src\fbe\Fb2Metadata.cpp") `
    (Join-Path $repoRoot "src\fbe\Fb2ShellProperties.cpp") `
    (Join-Path $repoRoot "src\fbshell\FbeCustomProperties.cpp") `
    (Join-Path $repoRoot "src\fbshell\Fb2PropertyKeys.cpp") `
    (Join-Path $repoRoot "src\fbshell\Fb2PropertyStore.cpp") `
    (Join-Path $PSScriptRoot "fb2-metadata-smoke.cpp") `
    "/link" "/SUBSYSTEM:CONSOLE" "ole32.lib" "oleaut32.lib" "comsuppw.lib" "propsys.lib" "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location $testDir
try {
    & $testExe $fixture
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke-тест чтения метаданных FB2 завершился с кодом $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Smoke-тест чтения метаданных FB2 прошёл успешно."

