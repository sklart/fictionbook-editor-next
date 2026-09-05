[CmdletBinding()] param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'; $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dll = Join-Path $root "out\$Configuration\Plugins\ExportHTML.dll"; if (-not (Test-Path -LiteralPath $dll)) { throw "Missing ExportHTML.dll: $dll" }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$exe = Join-Path $root "out\$Configuration\export-html-v2-runtime.exe"
& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'export-html-v2-runtime-harness.cpp') (Join-Path $root 'src\fbe\FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { throw 'ExportHTML v2 runtime harness did not compile.' }
try { & $exe $dll; if ($LASTEXITCODE -ne 0) { throw "ExportHTML v2 runtime harness failed: $LASTEXITCODE" } } finally { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath ($exe -replace '\.exe$','.pdb') -Force -ErrorAction SilentlyContinue }
