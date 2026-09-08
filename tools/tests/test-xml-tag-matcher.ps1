[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$outDir = Join-Path $repoRoot 'out\tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'xml-tag-matcher-test.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /I"$repoRoot\src\fbe" /c /Fo"$outDir\xml-tag-matcher-test.obj" "$PSScriptRoot\xml-tag-matcher-test.cpp"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cl.exe /nologo /EHsc /std:c++17 /MT /I"$repoRoot\src\fbe" /c /Fo"$outDir\xml-tag-matcher-core.obj" "$repoRoot\src\fbe\XmlTagMatcher.cpp"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& link.exe /nologo /SUBSYSTEM:CONSOLE /OUT:$exe "$outDir\xml-tag-matcher-test.obj" "$outDir\xml-tag-matcher-core.obj"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'XML tokenizer/matcher unit tests passed.'
