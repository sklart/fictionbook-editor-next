<# Exercises text/NBSP/Unicode and CF_BITMAP preparation without an editor DOM. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ('fbe-clipboard-preparer-' + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\clipboard-paste-preparer-test.cpp'
    $preparer = Join-Path $root 'src\fbe\clipboard\ClipboardPastePreparer.cpp'
    $exe = Join-Path $temp 'clipboard-paste-preparer-test.exe'
    & $compiler /nologo /DUNICODE /D_UNICODE /source-charset:windows-1251 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') /I (Join-Path $root 'third_party\wtl') "/Fo$temp\\" $source $preparer /Fe$exe gdiplus.lib
    if($LASTEXITCODE -ne 0) { throw 'Clipboard paste preparer test compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'Clipboard paste preparer behavior test failed.' }
    Write-Host 'Clipboard paste text/NBSP/Unicode/bitmap behavior passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
