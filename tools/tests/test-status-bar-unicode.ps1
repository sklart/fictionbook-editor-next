[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ("fbe-status-unicode-" + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\status-bar-unicode-test.cpp'
    $exe = Join-Path $temp 'status-bar-unicode-test.exe'
    & $compiler /nologo /utf-8 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') $source /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'Unicode behavioral test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Unicode behavioral test failed.' }
    Write-Host 'Status bar Unicode behavioral test passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
