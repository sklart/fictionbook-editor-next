[CmdletBinding()]
param([string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dll = Join-Path $root "out\$Configuration\Plugins\ImportEPUB.dll"
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw "Missing ImportEPUB.dll: $dll" }

# Reuse the normal Cyrillic EPUB fixture produced by the existing ExportEPUB
# smoke test.  This keeps the import test on the product's fixture path rather
# than adding a second EPUB generator.
$fixture = Join-Path $root 'out\tests\export-epub-cyrillic\fb2-metadata-cyrillic-smoke.epub'
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'test-export-epub-cyrillic.ps1') -Configuration $Configuration -OutputPath $fixture
}
if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) { throw "Missing ImportEPUB runtime fixture: $fixture" }

& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$exe = Join-Path $root "out\$Configuration\import-epub-v2-runtime.exe"
try {
    & cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'import-epub-v2-runtime-harness.cpp') (Join-Path $root 'src\fbe\FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"
    if ($LASTEXITCODE -ne 0) { throw 'ImportEPUB v2 runtime harness did not compile.' }
    & $exe $dll $fixture
    if ($LASTEXITCODE -ne 0) { throw "ImportEPUB v2 runtime harness failed: $LASTEXITCODE" }
}
finally {
    Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ($exe -replace '\.exe$','.pdb') -Force -ErrorAction SilentlyContinue
}
Write-Host 'ImportEPUB v2 runtime checks passed.'
