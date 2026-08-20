<#
.SYNOPSIS
Проверяет автономный режим ExportHTML.

.DESCRIPTION
Прогоняет штатный XSLT через MSXML с FB2, содержащей обложку и иллюстрацию.
Проверяет data URI, встроенный CSS, HTML5-кодировку и якоря внутренней навигации.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$pluginSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\export-html\ExportHTMLPlugin.cpp')
if ($pluginSource -notmatch 'dlg\.m_ofn\.nFilterIndex\s*=\s*4') {
    throw 'Автономный HTML должен быть выбранным по умолчанию форматом сохранения.'
}
if ($pluginSource -notmatch 'proc->put_input\(variant_t\(\(IDispatch\*\)source\)\)') {
    throw 'Экспорт должен передавать в XSLT подготовленную копию FB2-документа.'
}

$sourceXslPath = Join-Path $repoRoot 'src\export-html\html.xsl'
$runtimeXslPath = Join-Path $repoRoot 'runtime\html.xsl'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourceXslPath).Hash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $runtimeXslPath).Hash) {
    throw 'runtime/html.xsl должен совпадать с исходным XSL экспортера.'
}

$xsl = New-Object -ComObject Msxml2.DOMDocument.6.0
$xsl.async = $false
if (-not $xsl.load($runtimeXslPath)) {
    throw $xsl.parseError.reason
}

$source = New-Object -ComObject Msxml2.DOMDocument.6.0
$source.async = $false
$fb2 = @'
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
  <description><title-info><book-title>Standalone test</book-title><annotation><p>Annotation</p></annotation><coverpage><image l:href="#cover"/></coverpage></title-info></description>
  <body><section id="chapter"><title><p>Chapter</p></title><p><a type="note" l:href="#note-1">1</a></p><image l:href="#cover"/><table><tr><th>Head</th><td colspan="2">Cell</td></tr></table></section></body>
  <body name="notes"><section id="note-1"><p>Note text</p></section></body>
  <binary id="cover" content-type="image/png">iVBOR
    w0KGgo=</binary>
</FictionBook>
'@
if (-not $source.loadXML($fb2)) {
    throw $source.parseError.reason
}

$template = New-Object -ComObject Msxml2.XSLTemplate.6.0
$template.stylesheet = $xsl
$processor = $template.createProcessor()
$processor.input = $source
$processor.addParameter('saveimages', $true, '')
$processor.addParameter('embedimages', $true, '')
$processor.addParameter('customcss', 'body { color: rgb(1, 2, 3); }', '')
$processor.addParameter('imagemaxwidth', 640, '')
$processor.addParameter('imagemaxheight', 480, '')
[void] $processor.transform()
$html = [string] $processor.output

foreach ($expected in @(
    'data:image/png;base64,iVBORw0KGgo=',
    'body { color: rgb(1, 2, 3); }',
	'max-width: 640px',
	'max-height: 480px',
    'meta charset',
	'<section>',
    'id="chapter"',
	'id="_fbh_annotation"',
	'class="fb2-table"',
	'colspan="2"',
	'name="description"',
	'class="note-back"'
)) {
    if ($html -notlike "*$expected*") {
        throw "В автономном HTML отсутствует: $expected"
    }
}

if ($html -match '_files/') {
    throw 'Автономный HTML не должен ссылаться на каталог _files.'
}

Write-Host 'Автономный ExportHTML прошёл проверку.'
