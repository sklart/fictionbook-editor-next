<# Native regression for XML declaration encoding parsing. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$directory = Join-Path ([IO.Path]::GetTempPath()) ("fbe-xml-declaration-$PID")
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $exe = Join-Path $directory 'xml-declaration-test.exe'
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /I (Join-Path $root 'src\fbe') (Join-Path $root 'tools\tests\xml-declaration-test.cpp') /Fe$exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper compilation failed.' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'XML declaration helper regression failed.' }
    Write-Host 'XML declaration native regression passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
