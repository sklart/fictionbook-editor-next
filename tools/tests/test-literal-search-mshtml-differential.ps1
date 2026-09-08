[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$testDir = Join-Path $repoRoot 'out\tests\literal-search-mshtml-differential'
$testExe = Join-Path $testDir 'literal-search-mshtml-differential.exe'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE "/I$(Join-Path $repoRoot 'src\fbe')" "/I$(Join-Path $repoRoot 'src\fbe\search')" "/I$(Join-Path $repoRoot 'third_party\wtl')" "/Fo$testDir\" (Join-Path $PSScriptRoot 'literal-search-mshtml-differential.cpp') (Join-Path $repoRoot 'src\fbe\search\LiteralSearch.cpp') /link /SUBSYSTEM:CONSOLE ole32.lib oleaut32.lib uuid.lib "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $testExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'LiteralSearch/MSHTML differential test passed.'
