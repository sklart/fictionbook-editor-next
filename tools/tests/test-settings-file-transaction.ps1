[CmdletBinding()]
param([string]$PlatformToolset = 'v143')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
$testDirectory = Join-Path $root 'out\tests\settings-files'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
$exe = Join-Path $testDirectory 'settings-file-transaction.exe'
& cl.exe /nologo /W4 /WX /EHsc /std:c++17 /Zc:wchar_t- /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'settings-file-transaction-test.cpp') "/Fo$testDirectory\\" "/Fe$exe" /link shlwapi.lib
if($LASTEXITCODE -ne 0) { throw 'Settings file transaction test compilation failed.' }
$fixture = Join-Path $testDirectory ('fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    & $exe $fixture
    if($LASTEXITCODE -ne 0) { throw "Settings file transaction test failed: $LASTEXITCODE" }
    Write-Host 'Settings file transaction: transient reader denials, persistent lock, rollback after partial failure, cancellation, replacement, deletion and cleanup passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    if($resolved.StartsWith([IO.Path]::GetFullPath($testDirectory) + '\', [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
