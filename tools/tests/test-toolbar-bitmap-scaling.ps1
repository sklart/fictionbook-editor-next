[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputDirectory = Join-Path $repoRoot 'build\tests\toolbar-bitmap-scaling'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    $visualStyles = "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'toolbar-bitmap-scaling-test.cpp') /Fe:toolbar-bitmap-scaling-test.exe /link $visualStyles comctl32.lib gdi32.lib user32.lib
    if($LASTEXITCODE -ne 0) { throw 'Toolbar bitmap scaling test compilation failed.' }
    & .\toolbar-bitmap-scaling-test.exe (Join-Path $repoRoot 'src\fbe\res\Toolbar.bmp')
    if($LASTEXITCODE -ne 0) { throw 'Toolbar bitmap scaling test failed.' }
} finally {
    Pop-Location
}
