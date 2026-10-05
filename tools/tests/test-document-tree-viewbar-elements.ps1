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
if($source -notmatch 'void CTreeWithToolBar::FinalizeViewBarTheme\(\)[\s\S]*?RefreshViewBarElementText\(elements\);[\s\S]*?EnsureViewBarElementTextWidth\(\);[\s\S]*?RedrawWindow\(m_view_bar') { throw 'После native theme processing нет явной нормализации текста, ширины и redraw view-bar.' }
if($source -notmatch 'SetWindowTheme\(window, dark \? L"DarkMode_Explorer" : L"Explorer", NULL\);[\s\S]{0,300}WM_SETTINGCHANGE[\s\S]{0,300}FinalizeViewBarTheme\(\)') { throw 'View-bar должен нормализоваться только после SetWindowTheme и WM_SETTINGCHANGE.' }
$popupStart = $source.IndexOf('bool ShowNativeDocumentTreeViewBarPopup')
$popupEnd = $source.IndexOf('LRESULT CALLBACK DocumentTreeViewBarWindowThemeProc', $popupStart)
if($popupStart -lt 0 -or $popupEnd -lt 0) { throw 'Не найден popup Элементы.' }
$popup = $source.Substring($popupStart, $popupEnd - $popupStart)
if([string]::IsNullOrWhiteSpace($popup) -or $popup -notmatch 'WM_INITMENUPOPUP' -or $popup -notmatch 'ThemeManager::TrackPopupMenu' -or $popup -match 'TrackPopupMenuEx') { throw 'Popup Элементы должен использовать подготовку command bar и ThemeManager::TrackPopupMenu.' }

$checkmarksStart = $source.IndexOf('void CTreeWithToolBar::RefreshStructureMenuCheckmarks()')
$checkmarksEnd = $source.IndexOf('void CTreeWithToolBar::RefreshLocalizedMenuCaptions()', $checkmarksStart)
if($checkmarksStart -lt 0 -or $checkmarksEnd -lt 0) { throw 'Не найдена настройка checkmark-битмапов меню Элементы.' }
$checkmarks = $source.Substring($checkmarksStart, $checkmarksEnd - $checkmarksStart)
foreach($required in @('MIIM_CHECKMARKS', 'hbmpChecked', 'hbmpUnchecked', 'ThemeManager::IsDark()', 'ThemeManager::IsHighContrast()', 'UiMetrics::ScaleForDpi(16, dpi)', 'SetMenuItemInfoW(m_st_menu, index, TRUE, &info)')) {
    if(-not $source.Contains($required)) { throw "Не хватает dark checkmark-контракта: $required" }
}
if($checkmarks -notmatch 'if\(!dark\)[\s\S]{0,220}ClearStructureMenuCheckmarks\(\)') { throw 'При возврате в Light должны удаляться custom checkmark-битмапы.' }
if($source -match 'MFT_OWNERDRAW|MF_OWNERDRAW') { throw 'Меню Элементы не должно переводиться в owner-draw.' }
if($source -notmatch 'bool CTreeWithToolBar::GetStructureMenuCheckmarkProbe\(bool expectCustomBitmaps\)' -or $source -notmatch 'm_structureMenuCheckmarkDpi == UiMetrics::DpiForWindow\(m_hWnd\)') { throw 'Runtime probe должен проверять custom checkmarks и их DPI.' }
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestPortableState.inl')
foreach($required in @('GetStructureMenuCheckmarkProbe(false)', 'GetStructureMenuCheckmarkProbe(true)', 'viewbar-checkmarks-light-before', 'viewbar-checkmarks-dark', 'viewbar-checkmarks-light-after')) {
    if(-not $runtime.Contains($required)) { throw "Runtime Light-Dark-Light не проверяет checkmarks: $required" }
}

Write-Host "Document Tree Elements view-bar regression passed (longest=$($longest[0].Language): $($longest[0].Text))."
