<#
.SYNOPSIS
Создаёт воспроизводимый валидный FB2 fixture для full-process benchmark FBE.

.DESCRIPTION
Текст разрастается за счёт обычной структуры книги (body/section/title/p/link/note),
а не <binary>. Размер является целевым минимумом: итоговый файл может быть немного больше.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateRange(1, 100)]
    [int]$SizeMiB,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$targetBytes = [int64]$SizeMiB * 1MB
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$directory = Split-Path -Parent $output
if (-not [string]::IsNullOrWhiteSpace($directory)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$prefix = @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
  <description><title-info><genre>prose</genre><author><first-name>Benchmark</first-name><last-name>Fixture</last-name></author><book-title>FBE Source memory benchmark</book-title><annotation><p>Generated fixture.</p></annotation><coverpage><image l:href="#cover"/></coverpage><lang>en</lang></title-info><document-info><program-used>FBE Next benchmark</program-used><id>fbe-memory-fixture</id><version>1.0</version></document-info></description>
  <body>
'@
$suffix = @'
  </body>
  <body name="notes"><section id="note-1"><title><p>Note</p></title><p>Generated note body.</p></section></body>
  <binary id="cover" content-type="image/png">iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIW2Nk+M/wHwAF/gL+G3D2ZQAAAABJRU5ErkJggg==</binary>
</FictionBook>
'@
$paragraphFiller = (' This generated prose stays within an ordinary FictionBook paragraph and exercises text layout, source serialization, XML lexing, wrapping, navigation, and undo history.' * 28)
$sectionTemplate = @'
    <section id="section-{0}"><title><p>Section {0}</p></title><epigraph><p>Generated structural epigraph.</p></epigraph><p id="p-{0}">This is generated benchmark prose with a <a l:href="#note-1">note link</a>, nested <emphasis>markup</emphasis>, and enough ordinary XML text.{1}</p><section id="nested-{0}"><title><p>Nested section {0}</p></title><p>Nested paragraph keeps realistic FictionBook structure instead of binary payload.</p></section></section>
'@

$encoding = [Text.UTF8Encoding]::new($false)
$builder = [Text.StringBuilder]::new()
[void]$builder.Append($prefix)
$index = 1
# All generated fragments are ASCII, so StringBuilder.Length is also their UTF-8 byte
# count.  Avoid materialising the growing document on every iteration: that made the
# 50 MiB fixture needlessly quadratic to generate.
while ($builder.Length + $suffix.Length -lt $targetBytes) {
    [void]$builder.AppendFormat($sectionTemplate, [object[]]@($index, $paragraphFiller))
    ++$index
}
[void]$builder.Append($suffix)
[IO.File]::WriteAllText($output, $builder.ToString(), $encoding)

$length = (Get-Item -LiteralPath $output).Length
Write-Host "FB2 benchmark fixture created."
Write-Host "  Path: $output"
Write-Host "  Bytes: $length"
Write-Host "  Sections: $($index - 1)"
