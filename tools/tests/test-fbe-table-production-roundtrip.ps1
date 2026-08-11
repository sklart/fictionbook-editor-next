<#
.SYNOPSIS
Exercises the real FBE FB2 -> visual DOM -> Source -> visual DOM pipeline.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180,
    [switch]$Huge
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$schemaPath = Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'
function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал сохранённый FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if ($validation.errorCode -ne 0) { throw "FictionBook.xsd validation failed: $($validation.reason)" }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'table.fb2'
$report = Join-Path $directory 'report.tsv'
$reopenReport = Join-Path $directory 'reopen.tsv'
try {
    if ($Huge) {
        $rows = [Text.StringBuilder]::new()
        for ($row = 0; $row -lt 356; ++$row) {
            [void]$rows.Append('<tr>')
            for ($column = 0; $column -lt 14; ++$column) { [void]$rows.AppendFormat('<td>r{0}c{1}</td>', $row, $column) }
            [void]$rows.Append('</tr>')
        }
        $fixtureText = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>huge table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>huge-table-test</id><version>1.0</version></document-info></description><body><section><p>Huge table.</p><table>' + $rows.ToString() + '</table></section></body></FictionBook>'
        Set-Content -LiteralPath $fixture -Value $fixtureText -Encoding utf8
    } else {
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
 <description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>table-test</id><version>1.0</version></document-info></description>
 <body><section><title><p>Table</p></title><table id="table-id" style="border: 1px solid"><tr align="center"><th id="head" valign="top"><strong>Header</strong></th><td id="cell" colspan="2" rowspan="1" align="right" valign="bottom"><emphasis>Cell</emphasis> <a l:href="#note">link</a></td></tr><tr><td>second</td><td>third</td><td>fourth</td></tr></table></section></body>
 <body name="notes"><section id="note"><title><p>Note</p></title><p>note</p></section></body>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    }

    $arguments = @('-s', '-b', $report, $fixture)
    if (-not $Huge) { $arguments = @('-s', '-c', '-b', $report, $fixture) }
    $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил table round-trip benchmark.' }
    if (-not (Test-Path -LiteralPath $report)) { throw 'FBE не записал отчёт table round-trip.' }

    $rows = Import-Csv -LiteralPath $report -Delimiter "`t"
    $tableRows = @($rows | Where-Object { $_.phase -like 'table-*' })
    $expectedCounts = if ($Huge) { 'table=1;tr=356;td=4984;th=0' } else { 'table=1;tr=2;td=4;th=1' }
    if ($tableRows.Count -lt 6) { throw "Недостаточно production table snapshots: $($tableRows.Count)." }
    foreach ($row in $tableRows) {
        if ($row.phase -notmatch $expectedCounts) { throw "Table structure changed in $($row.phase)." }
    }

    [xml]$saved = Get-Content -LiteralPath $fixture -Raw
    Assert-Fb2Schema $fixture
    $namespaces = [Xml.XmlNamespaceManager]::new($saved.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    if ($Huge) {
        $table = $saved.SelectSingleNode('/fb:FictionBook/fb:body/fb:section/fb:table', $namespaces)
        if ($null -eq $table -or @($table.SelectNodes('fb:tr', $namespaces)).Count -ne 356 -or @($table.SelectNodes('.//fb:td', $namespaces)).Count -ne 4984 -or @($table.SelectNodes('.//fb:th', $namespaces)).Count -ne 0) { throw 'Save изменил huge table.' }
    } else {
    $table = $saved.SelectSingleNode('/fb:FictionBook/fb:body/fb:section/fb:table[@id="table-id"]', $namespaces)
    if ($null -eq $table -or $table.GetAttribute('style') -ne 'border: 1px solid') { throw 'Save изменил table id/style.' }
    if (@($table.SelectNodes('fb:tr', $namespaces)).Count -ne 2 -or @($table.SelectNodes('.//fb:td', $namespaces)).Count -ne 4 -or @($table.SelectNodes('.//fb:th', $namespaces)).Count -ne 1) { throw 'Save изменил структуру table.' }
    $cell = $table.SelectSingleNode('fb:tr/fb:td[@id="cell"]', $namespaces)
    if ($null -eq $cell -or $cell.GetAttribute('colspan') -ne '2' -or $cell.GetAttribute('rowspan') -ne '1' -or $cell.GetAttribute('align') -ne 'right' -or $cell.GetAttribute('valign') -ne 'bottom' -or $cell.InnerXml -notmatch 'emphasis|a') { throw 'Save изменил cell attributes or inline markup.' }
    }

    $reopen = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reopenReport, $fixture) -PassThru
    if (-not $reopen.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $reopen.Id -Force; throw 'FBE не завершил повторное открытие таблицы.' }
    if (-not (Test-Path -LiteralPath $reopenReport)) { throw 'FBE не записал отчёт повторного открытия таблицы.' }
    $reopenRows = @(Import-Csv -LiteralPath $reopenReport -Delimiter "`t" | Where-Object { $_.phase -like 'table-*' })
    if ($reopenRows.Count -lt 1 -or ($reopenRows | Where-Object { $_.phase -notmatch $expectedCounts })) { throw 'Повторное открытие изменило структуру таблицы.' }
    Write-Host "Production table Save -> reopen round-trip passed ($($tableRows.Count) snapshots)."
}
finally {
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
