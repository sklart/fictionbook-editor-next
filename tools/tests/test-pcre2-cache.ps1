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
foreach ($path in @((Join-Path $installDir "include\pcre2.h"), (Join-Path $installDir "lib\pcre2-16-static.lib"))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найдена подготовленная PCRE2-зависимость: $path" }
}

$testDir = Join-Path $repoRoot "out\tests\pcre2-cache"
$testExe = Join-Path $testDir "pcre2-cache-smoke.exe"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null

$clArguments = @(
    "/nologo", "/EHsc", "/std:c++17", "/MT", "/DUNICODE", "/D_UNICODE", "/DPCRE2_CODE_CACHE_TESTING",
    "/I$(Join-Path $repoRoot "third_party\wtl")",
    "/I$(Join-Path $repoRoot "src\fbe\search")",
    "/I$(Join-Path $installDir "include")",
    "/Fo$(Join-Path $testDir "pcre2-cache-smoke.obj")",
    (Join-Path $PSScriptRoot "pcre2-cache-smoke.cpp"),
    "/link", "/SUBSYSTEM:CONSOLE",
    "/LIBPATH:$(Join-Path $installDir "lib")",
    "pcre2-16-static.lib", "/OUT:$testExe"
)
& cl.exe @clArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $testExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Smoke-тест cache compiled PCRE2 code прошёл успешно."
