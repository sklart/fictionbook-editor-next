[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot 'out\tests\binary-file-save-runtime'
$testExe = Join-Path $testDir 'binary-file-save-runtime.exe'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MT /W4 `
    "/I$(Join-Path $repoRoot 'src\fbe')" `
    (Join-Path $PSScriptRoot 'binary-file-save-runtime.cpp') `
    "/Fe$testExe" /link /SUBSYSTEM:CONSOLE
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $testExe
if ($LASTEXITCODE -ne 0) { throw "Runtime-тест BinaryFileSave завершился с кодом $LASTEXITCODE." }
Write-Host 'Runtime-тест атомарного сохранения binary прошёл успешно.'
