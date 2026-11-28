[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'font-preview-combo'
$exe = Join-Path $scratch.Root 'font-preview-combo-runtime.exe'
$object = Join-Path $scratch.Root 'font-preview-combo-runtime.obj'
$passed = $false
try {
    & cl.exe /nologo /EHsc /std:c++17 /W4 /WX /utf-8 /DUNICODE /D_UNICODE "/Fo:$object" "/Fe:$exe" `
        (Join-Path $PSScriptRoot 'font-preview-combo-runtime.cpp') /link user32.lib gdi32.lib comctl32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Font picker runtime harness did not compile.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Font picker runtime harness failed: $LASTEXITCODE" }
    $passed = $true
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed
}
