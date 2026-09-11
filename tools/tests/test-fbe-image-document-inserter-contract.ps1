<# Exercises invalid-argument result propagation without allocating image data. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ('fbe-image-inserter-contract-' + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\image-document-inserter-contract-test.cpp'
    $exe = Join-Path $temp 'image-document-inserter-contract-test.exe'
    & $compiler /nologo /DUNICODE /D_UNICODE /source-charset:windows-1251 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') /I (Join-Path $root 'third_party\wtl') "/Fo$temp\\" $source /Fe$exe oleaut32.lib
    if($LASTEXITCODE -ne 0) { throw 'Image document inserter contract compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'Image document inserter contract failed.' }
    Write-Host 'Image document inserter error-result contract passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
