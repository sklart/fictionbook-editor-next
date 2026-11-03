[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $repoRoot 'build\tests\palette-preview-clip'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'palette-preview-clip-test.cpp') /Fe:palette-preview-clip-test.exe /link gdi32.lib user32.lib
    if($LASTEXITCODE -ne 0) { throw 'Palette preview clipping test compilation failed.' }
    & .\palette-preview-clip-test.exe
    if($LASTEXITCODE -ne 0) { throw 'Palette preview clipping test failed.' }
} finally {
    Pop-Location
}
