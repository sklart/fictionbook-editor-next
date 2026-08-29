[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ("fbe-status-bar-behavior-" + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\status-bar-behavior-test.cpp'
    $exe = Join-Path $temp 'status-bar-behavior-test.exe'
    & $compiler /nologo /utf-8 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') $source /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'Status bar behavioral test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Status bar behavioral test failed.' }
    Write-Host 'Status bar behavioral test passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
