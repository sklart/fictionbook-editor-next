[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ("fbe-spell-visible-paragraphs-" + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\spell-visible-paragraphs-test.cpp'
    $exe = Join-Path $temp 'spell-visible-paragraphs-test.exe'
    & $compiler /nologo /utf-8 /WX /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') $source /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'Visible spelling paragraph test compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Visible spelling paragraph test failed.' }
    Write-Host 'Visible spelling paragraph behavioral test passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
