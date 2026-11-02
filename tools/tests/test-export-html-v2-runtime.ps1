[CmdletBinding()] param([string]$Configuration = 'Release', [string]$RuntimeDirectory)
$ErrorActionPreference = 'Stop'; $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root "out\$Configuration" }
$dll = Join-Path $RuntimeDirectory "Plugins\ExportHTML.dll"; if (-not (Test-Path -LiteralPath $dll)) { throw "Missing ExportHTML.dll: $dll" }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$apiDirectory = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration $Configuration
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'export-html-v2'
$exe = Join-Path $scratch.Root 'export-html-v2-runtime.exe'
$objectDir = Join-Path $scratch.Root 'obj'
$oldTemp, $oldTmp = $env:TEMP, $env:TMP; $passed = $false
try { $env:TEMP = $scratch.Root; $env:TMP = $scratch.Root; New-Item -ItemType Directory -Path $objectDir | Out-Null; & cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE "/I$apiDirectory" "/Fo:$objectDir\\" (Join-Path $PSScriptRoot 'export-html-v2-runtime-harness.cpp') (Join-Path $apiDirectory 'FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"; if ($LASTEXITCODE -ne 0) { throw 'ExportHTML v2 runtime harness did not compile.' }; & $exe $dll; if ($LASTEXITCODE -ne 0) { throw "ExportHTML v2 runtime harness failed: $LASTEXITCODE" }; $passed = $true } finally { $env:TEMP = $oldTemp; $env:TMP = $oldTmp; Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents }
