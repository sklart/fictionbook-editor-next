[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\DocumentTree.cpp')
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json') | ConvertFrom-Json

$start = $source.IndexOf('void CTreeWithToolBar::EnsureViewBarElementTextWidth()')
if($start -lt 0) { throw 'Не найдена процедура измерения кнопки Элементы.' }
$end = $source.IndexOf('void CTreeWithToolBar::UpdateViewBarMode', $start)
if($end -lt 0) { throw 'Не найден конец процедуры измерения кнопки Элементы.' }
$measure = $source.Substring($start, $end - $start)

foreach($required in @(
    'TBIF_TEXT | TBIF_SIZE | TBIF_BYINDEX',
    'TB_GETBUTTONINFOW',
    'GetTextExtentPoint32W',
    'UiMetrics::ScaleForDpi(16, UiMetrics::DpiForWindow(m_view_bar))',
    'TBIF_SIZE | TBIF_BYINDEX',
    'TB_SETBUTTONINFOW')) {
    if(-not $measure.Contains($required)) { throw "Не хватает контракта ширины кнопки Элементы: $required" }
}
if($measure -match 'writeButton\.dwMask\s*=\s*TBIF_TEXT') { throw 'TB_SETBUTTONINFOW ширины не должен передавать TBIF_TEXT.' }
if($measure -notmatch 'TBBUTTONINFOW readButton' -or $measure -notmatch 'TBBUTTONINFOW writeButton') { throw 'Чтение и запись TBBUTTONINFO должны быть разделены.' }
if($source -notmatch 'RefreshViewBarElementText\(elsMenuItem\);[\s\S]{0,300}EnsureViewBarElementTextWidth\(\);') { throw 'После локализации не обновляются текст и ширина кнопки Элементы.' }

$translations = $catalog.seedStrings.'fbe.document_tree.menu.elements'.translations
if($translations.'ru-RU' -ne 'Элементы') { throw 'Русская кнопка Элементы должна сохранять полный текст.' }
if([string]::IsNullOrWhiteSpace($translations.'en-US')) { throw 'Отсутствует английский текст кнопки Elements.' }
$longest = @($translations.psobject.Properties | ForEach-Object { [pscustomobject]@{ Language=$_.Name; Text=[string]$_.Value; Length=([string]$_.Value).Length } } | Sort-Object Length -Descending | Select-Object -First 1)
if($longest[0].Length -le $translations.'en-US'.Length) { throw 'В каталоге не найден перевод, требующий расширенной ширины.' }
if($source -notmatch 'RefreshViewBarElementText\(LPCWSTR text\)' -or $source -notmatch 'TBIF_TEXT \| TBIF_BYINDEX') { throw 'Локализация должна явно записывать новый текст первой кнопки.' }

Write-Host "Document Tree Elements view-bar regression passed (longest=$($longest[0].Language): $($longest[0].Text))."
