[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
$installDir = Join-Path $repoRoot "build\pcre2\install\$Configuration"
$testDir = Join-Path $repoRoot 'out\tests\search-document-adapter-mshtml'
$testExe = Join-Path $testDir 'search-document-adapter-mshtml.exe'
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$sources = @(
    (Join-Path $PSScriptRoot 'search-document-adapter-mshtml.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\SearchDocumentAdapter.cpp'),
    (Join-Path $repoRoot 'src\fbe\ReplacementPreflight.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\DocumentSearchCoordinator.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\SearchSession.cpp'),
	(Join-Path $repoRoot 'src\fbe\search\SearchResults.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\SearchTextSnapshot.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\LiteralSearch.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\RegexBackend.cpp'),
    (Join-Path $repoRoot 'src\fbe\search\RegexBackendPcre2.cpp')
)
& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE "/I$(Join-Path $repoRoot 'src\fbe')" "/I$(Join-Path $repoRoot 'src\fbe\search')" "/I$(Join-Path $repoRoot 'third_party\wtl')" "/I$(Join-Path $installDir 'include')" "/Fo$testDir\" $sources /link /SUBSYSTEM:CONSOLE ole32.lib oleaut32.lib uuid.lib "/LIBPATH:$(Join-Path $installDir 'lib')" pcre2-16-static.lib "/OUT:$testExe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $testExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'SearchDocumentAdapter/MSHTML regression test passed.'
