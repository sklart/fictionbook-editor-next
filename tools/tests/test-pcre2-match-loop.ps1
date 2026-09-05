[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset,

    [switch]$UsePreparedPcre2
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
$installDir = Join-Path $repoRoot "build\pcre2\install\$Configuration"
if (-not $UsePreparedPcre2) {
    $arguments = @{ Configuration = $Configuration; Quiet = $true }
    if ($PlatformToolset) { $arguments.PlatformToolset = $PlatformToolset }
    & (Join-Path $repoRoot "tools\build\build-pcre2.ps1") @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$testDir = Join-Path $repoRoot "out\tests\pcre2-match-loop"
$testExe = Join-Path $testDir "pcre2-match-loop-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$clArguments = @(
    "/nologo", "/EHsc", "/std:c++17", "/MT",
    "/I$(Join-Path $repoRoot "src\fbe\search")",
    "/I$(Join-Path $installDir "include")",
    "/Fo$(Join-Path $testDir "pcre2-match-loop-smoke.obj")",
    (Join-Path $PSScriptRoot "pcre2-match-loop-smoke.cpp"),
    "/link", "/SUBSYSTEM:CONSOLE",
    "/LIBPATH:$(Join-Path $installDir "lib")",
    "pcre2-16-static.lib", "/OUT:$testExe"
)
& cl.exe @clArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $testExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Smoke-тест matching-loop PCRE2 прошёл успешно."
