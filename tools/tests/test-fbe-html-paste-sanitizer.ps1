[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$temp = Join-Path ([IO.Path]::GetTempPath()) ('fbe-html-paste-' + $PID)
New-Item -ItemType Directory -Path $temp | Out-Null
try {
    $source = Join-Path $root 'tools\tests\html-paste-sanitizer-test.cpp'
    $sanitizer = Join-Path $root 'src\fbe\clipboard\HtmlPasteSanitizer.cpp'
    $exe = Join-Path $temp 'html-paste-sanitizer-test.exe'
    & $compiler /nologo /DUNICODE /D_UNICODE /source-charset:utf-8 /EHsc /std:c++17 /I (Join-Path $root 'src\fbe') /I (Join-Path $root 'third_party\wtl') "/Fo$temp\\" $source $sanitizer /Fe$exe
    if($LASTEXITCODE -ne 0) { throw 'HTML paste sanitizer test compilation failed.' }
    & $exe
    if($LASTEXITCODE -ne 0) { throw 'HTML paste sanitizer behavior test failed.' }
    Write-Host 'HTML paste sanitizer behavior passed.'
}
finally { Remove-Item -Recurse -Force -LiteralPath $temp -ErrorAction SilentlyContinue }
