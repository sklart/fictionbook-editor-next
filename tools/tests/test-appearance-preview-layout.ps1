[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $repoRoot 'build\tests\appearance-preview-layout'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'appearance-preview-layout-test.cpp') /Fe:appearance-preview-layout-test.exe /link kernel32.lib
    if($LASTEXITCODE -ne 0) { throw 'Appearance preview layout test compilation failed.' }
    & .\appearance-preview-layout-test.exe
    if($LASTEXITCODE -ne 0) { throw 'Appearance preview layout test failed.' }
} finally {
    Pop-Location
}
