<#
.SYNOPSIS
Exercises context attribute bars against a live MSHTML table: paired attributes,
Undo/Redo, Save -> reopen and FictionBook XSD.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'ci-runtime'
$FbeExe = $isolation.Exe
$isolationPassed = $false
try {
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$schemaPath = Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'
function Invoke-Fbe([string[]]$Arguments, [string]$Name) {
    $p=Start-Process -FilePath $FbeExe -ArgumentList $Arguments -PassThru
    if(-not $p.WaitForExit($TimeoutSeconds*1000)) { Stop-Process -Id $p.Id -Force; throw "FBE не завершил $Name." }
    if($p.ExitCode -ne 0) { throw "FBE вернул код $($p.ExitCode): $Name." }
}
function Assert-Xsd([string]$Path) {
    $cache=New-Object -ComObject Msxml2.XMLSchemaCache.6.0; $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0',$schemaPath)
    $doc=New-Object -ComObject Msxml2.DOMDocument.6.0; $doc.async=$false
    if(-not $doc.load($Path)) { throw "MSXML не прочитал FB2: $($doc.parseError.reason)" }; $doc.schemas=$cache
    if($doc.validate().errorCode -ne 0) { throw 'FictionBook.xsd validation failed.' }
}
function Assert-Phase($rows,[string]$Phase,[string[]]$Expected) {
    $row=@($rows | Where-Object phase -eq $Phase)
    if($row.Count -ne 1) { throw "Нет ровно одного snapshot: $Phase" }
    foreach($item in $Expected) { $pair=$item.Split('=',2); if([string]$row[0].($pair[0]) -ne $pair[1]) { throw "${Phase}: $($pair[0])='$($row[0].($pair[0]))', ожидалось '$($pair[1])'." } }
}
$dir=Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-attrs-'+[guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $dir | Out-Null
try {
    $fb2=Join-Path $dir 'table-attributes.fb2'; $report=Join-Path $dir 'attributes.tsv'; $reopen=Join-Path $dir 'reopen.tsv'
    $fixture='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>table attributes</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>table-attributes</id><version>1.0</version></document-info></description><body><section><table><tr><td id="target" colspan="1" rowspan="1" align="right" valign="top">target</td><td>peer</td><td>peer 2</td></tr><tr><td>tail</td><td>tail 2</td><td>tail 3</td></tr></table></section></body></FictionBook>'
    Set-Content -LiteralPath $fb2 -Value $fixture -Encoding utf8
    $saved=@{}; foreach($key in 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_TABLE_ATTRIBUTES_REOPEN') { $saved[$key]=[Environment]::GetEnvironmentVariable($key) }
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-attributes'; Remove-Item Env:FBE_NEXT_TEST_TABLE_ATTRIBUTES_REOPEN -ErrorAction SilentlyContinue; Invoke-Fbe @('-b',$report,$fb2) 'table attributes' }
    finally { foreach($key in $saved.Keys) { if($null -eq $saved[$key]) { Remove-Item ("Env:"+$key) -ErrorAction SilentlyContinue } else { Set-Item ("Env:"+$key) $saved[$key] } } }
    $rows=Import-Csv -LiteralPath $report -Delimiter "`t"
    # The FB2 fixture intentionally has no internal fb* attributes.  fb* must
    # be materialized by the normal FB2 XSLT -> Design DOM path before editing.
    Assert-Phase $rows 'colspan-before' @('colspan=1','fbcolspan=1','rowspan=1','fbrowspan=1','align=right','fbalign=right','valign=top','fbvalign=top')
    Assert-Phase $rows 'colspan-after' @('colspan=2','fbcolspan=2'); Assert-Phase $rows 'colspan-undo' @('colspan=1','fbcolspan=1'); Assert-Phase $rows 'colspan-redo' @('colspan=2','fbcolspan=2')
    Assert-Phase $rows 'rowspan-after' @('rowspan=2','fbrowspan=2'); Assert-Phase $rows 'rowspan-undo' @('rowspan=1','fbrowspan=1'); Assert-Phase $rows 'rowspan-redo' @('rowspan=2','fbrowspan=2')
    Assert-Phase $rows 'align-after' @('align=center','fbalign=center'); Assert-Phase $rows 'align-undo' @('align=right','fbalign=right'); Assert-Phase $rows 'align-redo' @('align=center','fbalign=center')
    Assert-Phase $rows 'valign-after' @('valign=bottom','fbvalign=bottom'); Assert-Phase $rows 'valign-undo' @('valign=top','fbvalign=top'); Assert-Phase $rows 'valign-redo' @('valign=bottom','fbvalign=bottom')
    Assert-Phase $rows 'align-clear-after' @('align=','fbalign='); Assert-Phase $rows 'align-clear-undo' @('align=center','fbalign=center'); Assert-Phase $rows 'align-clear-redo' @('align=','fbalign=')
    Assert-Xsd $fb2
    $xml=New-Object -ComObject Msxml2.DOMDocument.6.0; $xml.async=$false; if(-not $xml.load($fb2)){throw $xml.parseError.reason}; $node=$xml.selectSingleNode('//*[local-name()="td" and @id="target"]')
    foreach($pair in @('colspan=2','rowspan=2','valign=bottom')) { $p=$pair.Split('=',2); if([string]$node.getAttribute($p[0]) -ne $p[1]){throw "Сохранённый FB2: $pair отсутствует."} }
    foreach($name in 'align') { if([string]$node.getAttribute($name)){throw "Сохранённый FB2 не очистил $name."} }
    $saved=@{}; foreach($key in 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_TABLE_ATTRIBUTES_REOPEN') { $saved[$key]=[Environment]::GetEnvironmentVariable($key) }
    try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-attributes'; $env:FBE_NEXT_TEST_TABLE_ATTRIBUTES_REOPEN='1'; Invoke-Fbe @('-b',$reopen,$fb2) 'table attributes reopen' }
    finally { foreach($key in $saved.Keys) { if($null -eq $saved[$key]) { Remove-Item ("Env:"+$key) -ErrorAction SilentlyContinue } else { Set-Item ("Env:"+$key) $saved[$key] } } }
    Assert-Phase (Import-Csv -LiteralPath $reopen -Delimiter "`t") 'reopen' @('colspan=2','fbcolspan=2','rowspan=2','fbrowspan=2','align=','fbalign=','valign=bottom','fbvalign=bottom')
    Write-Host 'Production table attribute pairing, Undo/Redo, Save -> reopen and XSD passed.'
} finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
$isolationPassed = $true
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $isolationPassed }
