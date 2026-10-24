[CmdletBinding()] param([string]$Configuration = 'Release', [string]$RuntimeDirectory)
$ErrorActionPreference = 'Stop'; $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root "out\$Configuration" }
$dll = Join-Path $RuntimeDirectory "Plugins\ExportHTML.dll"; if (-not (Test-Path -LiteralPath $dll)) { throw "Missing ExportHTML.dll: $dll" }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$apiDirectory = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration $Configuration
$exe = Join-Path $RuntimeDirectory "export-html-v2-runtime.exe"
$objectDir = Join-Path $RuntimeDirectory 'test-obj\export-html-v2-runtime'; New-Item -ItemType Directory -Force -Path $objectDir | Out-Null
& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE "/I$apiDirectory" "/Fo:$objectDir\\" (Join-Path $PSScriptRoot 'export-html-v2-runtime-harness.cpp') (Join-Path $apiDirectory 'FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { throw 'ExportHTML v2 runtime harness did not compile.' }
try { & $exe $dll; if ($LASTEXITCODE -ne 0) { throw "ExportHTML v2 runtime harness failed: $LASTEXITCODE" } } finally { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath ($exe -replace '\.exe$','.pdb') -Force -ErrorAction SilentlyContinue }
