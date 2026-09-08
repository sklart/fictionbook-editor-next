[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64

$outDir = Join-Path $repoRoot 'out\tests\xml-source-cache-runtime'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'xml-source-cache-runtime-test.exe'
$common = @('/nologo', '/EHsc', '/std:c++17', '/MT', '/DUNICODE', '/D_UNICODE', '/DFBE_XML_TAG_HIGHLIGHTER_TEST', "/I$repoRoot\src\fbe", "/I$repoRoot\third_party\wtl", "/I$repoRoot\third_party\scintilla\include")

& cl.exe @common /c "/Fo$outDir\xml-source-cache-runtime-test.obj" "$PSScriptRoot\xml-source-cache-runtime-test.cpp"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cl.exe @common /c "/Fo$outDir\XmlSourceTagHighlighter.obj" "$repoRoot\src\fbe\XmlSourceTagHighlighter.cpp"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cl.exe @common /c "/Fo$outDir\XmlTagMatcher.obj" "$repoRoot\src\fbe\XmlTagMatcher.cpp"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& link.exe /nologo /SUBSYSTEM:CONSOLE /OUT:$exe "$outDir\xml-source-cache-runtime-test.obj" "$outDir\XmlSourceTagHighlighter.obj" "$outDir\XmlTagMatcher.obj" user32.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item -LiteralPath (Join-Path $repoRoot 'runtime\Scintilla.dll') -Destination $outDir -Force
Push-Location $outDir
try {
    & .\xml-source-cache-runtime-test.exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
