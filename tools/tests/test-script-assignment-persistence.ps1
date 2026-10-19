[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$directory = Join-Path $root 'out\tests\script-assignment-persistence'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$exe = Join-Path $directory 'script-assignment-persistence.exe'
$sources = @('tools\tests\script-assignment-persistence-test.cpp', 'src\fbe\scripts\ScriptRegistry.cpp',
    'src\fbe\toolbars\PortableToolbarStore.cpp', 'src\fbe\toolbars\ToolbarsV2Codec.cpp') | ForEach-Object { Join-Path $root $_ }
& cl.exe /nologo /W4 /WX /EHsc /std:c++17 /utf-8 /Zc:wchar_t- /DUNICODE /D_UNICODE `
    "/FI$(Join-Path $PSScriptRoot 'script-assignment-test-pch.h')" "/I$(Join-Path $root 'src\fbe')" $sources "/Fo$directory\\" "/Fe$exe" /link shell32.lib ole32.lib rpcrt4.lib
if($LASTEXITCODE -ne 0) { throw 'Script assignment native test compilation failed.' }
$fixture = Join-Path $directory ('fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
Copy-Item -LiteralPath $exe -Destination $fixture
& (Join-Path $fixture 'script-assignment-persistence.exe') --portable
if($LASTEXITCODE -ne 0) { throw "Script assignment native regression failed. Fixture: $fixture" }
$installed = Join-Path $fixture 'InstalledSettings'
New-Item -ItemType Directory -Path $installed | Out-Null
$previousMode = $env:FBE_NEXT_TEST_MODE; $previousSettings = $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
try {
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $installed
    & (Join-Path $fixture 'script-assignment-persistence.exe') --installed
    if($LASTEXITCODE -ne 0) { throw "Script assignment installed native regression failed. Fixture: $fixture" }
} finally { $env:FBE_NEXT_TEST_MODE = $previousMode; $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $previousSettings }
Write-Host 'Script assignment persistence regression passed in isolated portable and installed fixtures.'
