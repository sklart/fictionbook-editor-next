[CmdletBinding()]
param([string]$DllPath = (Join-Path $PSScriptRoot '..\..\out\Release\Plugins\ImportEPUB.dll'),
      [string]$Fixture = (Join-Path $PSScriptRoot '..\..\out\tests\export-epub-cyrillic\fb2-metadata-cyrillic-smoke.epub'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$DllPath = (Resolve-Path -LiteralPath $DllPath).Path
$Fixture = (Resolve-Path -LiteralPath $Fixture).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root 'out\tests\import-epub-api-version'; New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'import-epub-api-version.exe'
& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE ("/Fo:$out\") (Join-Path $PSScriptRoot 'import-epub-api-version-harness.cpp') /link ole32.lib oleaut32.lib ("/OUT:$exe")
if($LASTEXITCODE -ne 0) { throw 'API version harness failed to compile.' }
& $exe $DllPath $Fixture
if($LASTEXITCODE -ne 0) { throw 'API version compatibility failed.' }
