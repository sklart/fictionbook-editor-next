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
    $buildPcre2Arguments = @{ Configuration = $Configuration; Quiet = $true }
    if ($PlatformToolset) { $buildPcre2Arguments.PlatformToolset = $PlatformToolset }
    & (Join-Path $repoRoot "tools\build\build-pcre2.ps1") @buildPcre2Arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$benchmarkDir = Join-Path $repoRoot "out\tests\pcre2-cache-benchmark"
$benchmarkExe = Join-Path $benchmarkDir "pcre2-cache-benchmark.exe"
$resultsPath = Join-Path $benchmarkDir "results.csv"
New-Item -ItemType Directory -Path $benchmarkDir -Force | Out-Null

$clArguments = @(
    "/nologo", "/EHsc", "/std:c++17", "/O2", "/MT", "/DUNICODE", "/D_UNICODE",
    "/I$(Join-Path $repoRoot "third_party\wtl")",
    "/I$(Join-Path $repoRoot "src\fbe")",
    "/I$(Join-Path $installDir "include")",
    "/Fo$(Join-Path $benchmarkDir "pcre2-cache-benchmark.obj")",
    (Join-Path $PSScriptRoot "pcre2-cache-benchmark.cpp"),
    "/link", "/SUBSYSTEM:CONSOLE",
    "/LIBPATH:$(Join-Path $installDir "lib")",
    "pcre2-16-static.lib", "/OUT:$benchmarkExe"
)
& cl.exe @clArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $benchmarkExe | Tee-Object -LiteralPath $resultsPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "PCRE2 cache benchmark saved to $resultsPath"
