<# Builds and executes direct writer ownership and rollback coverage for ExportHTML. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('fbe-export-html-writer-' + $PID)
$out = Join-Path $scratch 'export-html-writer.exe'
$obj = Join-Path $scratch 'obj'
New-Item -ItemType Directory -Force -Path $obj | Out-Null
try {
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE "/I$(Join-Path $root 'src\export-html')" "/I$(Join-Path $root 'third_party')" "/Fo:$obj\\" `
        (Join-Path $PSScriptRoot 'export-html-writer-harness.cpp') `
        (Join-Path $root 'src\export-html\HtmlExportWriter.cpp') `
        (Join-Path $root 'src\export-html\HtmlExportWriterHelpers.cpp') `
        (Join-Path $root 'src\common\fb2\Fb2BinaryInspector.cpp') `
        /link /OUT:$out ole32.lib oleaut32.lib comsuppw.lib msxml6.lib
    if ($LASTEXITCODE -ne 0) { throw 'ExportHTML writer harness did not compile.' }
    & $out
    if ($LASTEXITCODE -ne 0) { throw "ExportHTML writer harness failed: $LASTEXITCODE" }
}
finally {
    Remove-Item -LiteralPath $out -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ($out -replace '\.exe$', '.pdb') -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
}
$writerSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\HtmlExportWriter.cpp')
if ($writerSource -match 'ERROR_CANCELLED') { throw 'Writer must not translate cancellation into a writer failure.' }
Write-Host 'ExportHTML writer harness passed.'