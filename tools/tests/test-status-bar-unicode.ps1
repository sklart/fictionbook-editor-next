[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$compiler = 'C:\BuildTools2022\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe'
$devCmd = 'C:\BuildTools2022\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $compiler)) { throw "MSVC compiler not found: $compiler" }
if (-not (Test-Path -LiteralPath $devCmd)) { throw "VS developer command script not found: $devCmd" }
$temp = Join-Path ([IO.Path]::GetTempPath()) ("fbe-status-unicode-" + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\status-bar-unicode-test.cpp'
    $exe = Join-Path $temp 'status-bar-unicode-test.exe'
    $command = 'call "{0}" -arch=x64 >nul && "{1}" /nologo /utf-8 /EHsc /std:c++17 /I "{2}" "{3}" /Fe"{4}"' -f $devCmd, $compiler, (Join-Path $root 'src\fbe'), $source, $exe
    & cmd.exe /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Unicode behavioral test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Unicode behavioral test failed.' }
    Write-Host 'Status bar Unicode behavioral test passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
