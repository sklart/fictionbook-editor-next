[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64

$testDir = Join-Path $repoRoot 'out\tests\search-session'
$testExe = Join-Path $testDir 'search-session-smoke.exe'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
& cl.exe /nologo /EHsc /std:c++17 /MT "/I$(Join-Path $repoRoot 'src\fbe\search')" "/Fo$testDir\" (Join-Path $PSScriptRoot 'search-session-smoke.cpp') (Join-Path $repoRoot 'src\fbe\search\SearchSession.cpp') (Join-Path $repoRoot 'src\fbe\search\SearchResults.cpp') (Join-Path $repoRoot 'src\fbe\search\LiteralSearch.cpp') (Join-Path $repoRoot 'src\fbe\search\SearchTextSnapshot.cpp') /link /SUBSYSTEM:CONSOLE "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $testExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'Search session smoke test passed.'
