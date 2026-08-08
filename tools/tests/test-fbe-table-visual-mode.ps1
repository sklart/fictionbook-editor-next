<#
.SYNOPSIS
Проверяет HTML-представление FB2-таблиц для визуального режима FBE.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$xslPath = Join-Path $repoRoot 'runtime\fb2.xsl'
$sourcePath = Join-Path $repoRoot 'src\fbe\FBDoc.cpp'
$viewPath = Join-Path $repoRoot 'src\fbe\FBEview.cpp'

$input = @'
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <body><section><table><tr><th align="center">Header</th><td colspan="2" rowspan="1" align="right" valign="bottom"><strong>Cell</strong></td></tr></table></section></body>
</FictionBook>
'@

$transform = [System.Xml.Xsl.XslCompiledTransform]::new()
$transform.Load($xslPath, [System.Xml.Xsl.XsltSettings]::Default, [System.Xml.XmlUrlResolver]::new())
$reader = [System.Xml.XmlReader]::Create([System.IO.StringReader]::new($input))
$output = [System.IO.StringWriter]::new()
$transform.Transform($reader, $null, $output)
$html = $output.ToString()

foreach($fragment in @('<table class="table"', '<tr class="tr"', '<th class="th"', '<td class="td"', 'colspan="2"', 'rowspan="1"', 'align="right"', 'valign="bottom"', '<strong>Cell</strong>')) {
    if($html -notlike "*$fragment*") { throw "Преобразование таблицы не содержит: $fragment" }
}

$source = Get-Content -Raw -LiteralPath $sourcePath
foreach($fragment in @('U::scmp(name,L"TABLE")', 'U::scmp(name,L"TR")', 'U::scmp(name,L"TD")', 'U::scmp(name,L"TH")', 'U::scmp(name,L"TBODY")')) {
    if($source -notlike "*$fragment*") { throw "Обратная сериализация не поддерживает: $fragment" }
}

$viewSource = Get-Content -Raw -LiteralPath $viewPath
foreach($fragment in @('Native tables have a deliberately different content model', 'U::scmp(nodeName, L"TABLE") == 0', 'U::scmp(name,L"TBODY")')) {
    if($viewSource -notlike "*$fragment*") { throw "Нормализация визуального редактора не сохраняет таблицы: $fragment" }
}

Write-Host 'Визуальное представление и обратная сериализация таблиц FBE прошли проверку.'
