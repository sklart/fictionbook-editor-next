<#
.SYNOPSIS
Собирает Win32-обработчики ZIP/RAR из единого C++ исходника.
#>

[CmdletBinding()]
param(
    [string]$PlatformToolset,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset -VcVarsVersion '14.44'

$source = Join-Path $repoRoot 'runtime\Utilities\ArchHandler\archhand.cpp'
$outputDir = if ($OutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
} else {
    Join-Path $repoRoot 'out\archhandler\Win32\Release'
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$zipHandler = Join-Path $outputDir 'ZipHandler.exe'
$rarHandler = Join-Path $outputDir 'RarHandler.exe'

& cl.exe /nologo /std:c++17 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE $source "/Fe$zipHandler" /link /SUBSYSTEM:WINDOWS Shell32.lib Advapi32.lib User32.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Copy-Item -LiteralPath $zipHandler -Destination $rarHandler -Force
Write-Host "ArchHandler собран: $zipHandler; $rarHandler"
