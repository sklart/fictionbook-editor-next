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
$normalizerPath = Join-Path $repoRoot 'src\fbe\view\VisualDomNormalizer.cpp'
$gridPath = Join-Path $repoRoot 'src\fbe\table\TableGrid.cpp'
$structuralPath = Join-Path $repoRoot 'src\fbe\table\TableStructuralEditor.cpp'

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
$normalizerSource = Get-Content -Raw -LiteralPath $normalizerPath
$gridSource = Get-Content -Raw -LiteralPath $gridPath
$structuralSource = Get-Content -Raw -LiteralPath $structuralPath
foreach($fragment in @('IsNativeTableBlockName', 'L"TABLE"', 'L"TBODY"', 'L"TR"', 'L"TD"', 'L"TH"')) {
    if($normalizerSource -notlike "*$fragment*") { throw "Нормализатор визуального редактора не сохраняет таблицы: $fragment" }
}

foreach($fragment in @('void PackText(', '!IsNativeTableBlockName(name)', 'IsNativeTableBlockName(name)')) {
    if($normalizerSource -notlike "*$fragment*") { throw "PackText может вложить TABLE в автоматически созданный P: $fragment" }
}

foreach($fragment in @('void SetSpan(', 'cell->setAttribute(fbName, attributeValue, 0)', 'cell->setAttribute(htmlName, attributeValue, 0)', 'cell->removeAttribute(fbName, 0)', 'cell->removeAttribute(htmlName, 0)', 'L"fbcolspan", L"colspan"', 'L"fbrowspan", L"rowspan"')) {
    if($gridSource -notlike "*$fragment*") { throw "Span metadata и HTML layout не синхронизированы: $fragment" }
}

foreach($fragment in @('TagAt(', 'TagAt(grid, boundary - 1, col, fallbackTag)', 'TagAt(grid, row, before ? column : column - 1, fallbackTag)')) {
    if($structuralSource -notlike "*$fragment*") { throw "Новые ячейки таблицы не наследуют тип локального соседа: $fragment" }
}

foreach($fragment in @('SnapshotNativeTables', 'tablesBeforeNormalize', 'D224', 'SnapshotSerializedTables', 'D225', 'm_serialization_unsafe', 'drop-row-after-normalize')) {
    if($source -notlike "*$fragment*") { throw "Нет защиты сохранения от внутренней потери TABLE: $fragment" }
}

Write-Host 'Визуальное представление и обратная сериализация таблиц FBE прошли проверку.'
