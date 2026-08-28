[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Join-Path $PSScriptRoot 'keyboard-layout-selection-test.cpp'
$output = Join-Path $env:TEMP 'fbe-keyboard-layout-selection-test.exe'
& cl.exe /nologo /EHsc /std:c++17 $source "/Fe:$output"
if($LASTEXITCODE -ne 0) { throw 'Could not compile keyboard layout C++ regression test.' }
& $output
if($LASTEXITCODE -ne 0) { throw 'Keyboard layout C++ regression test failed.' }
Write-Host 'Keyboard-layout C++ behavior passed.'
