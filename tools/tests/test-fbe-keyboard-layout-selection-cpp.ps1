[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Join-Path $PSScriptRoot 'keyboard-layout-selection-test.cpp'
$testDirectory = Join-Path $env:TEMP 'fbe-keyboard-layout-selection-test'
$output = Join-Path $testDirectory 'fbe-keyboard-layout-selection-test.exe'
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
& cl.exe /nologo /EHsc /std:c++17 "/Fo:$testDirectory\\" $source "/Fe:$output"
if($LASTEXITCODE -ne 0) { throw 'Could not compile keyboard layout C++ regression test.' }
& $output
if($LASTEXITCODE -ne 0) { throw 'Keyboard layout C++ regression test failed.' }
Write-Host 'Keyboard-layout C++ behavior passed.'
