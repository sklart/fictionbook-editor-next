[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $repoRoot 'build\tests\palette-preview-contrast'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'palette-preview-contrast-test.cpp') /Fe:palette-preview-contrast-test.exe
    if($LASTEXITCODE -ne 0) { throw 'Palette preview contrast test compilation failed.' }
    & .\palette-preview-contrast-test.exe
    if($LASTEXITCODE -ne 0) { throw 'Palette preview contrast test failed.' }
} finally {
    Pop-Location
}
