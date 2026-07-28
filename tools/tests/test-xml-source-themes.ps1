<#
.SYNOPSIS
Проверяет контракт логических ролей оформления XML-редактора.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Read-ProjectFile([string]$relativePath) {
    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
}

$settingsHeader = Read-ProjectFile "src\fbe\Settings.h"
$settingsSource = Read-ProjectFile "src\fbe\Settings.cpp"
$mainFrame = Read-ProjectFile "src\fbe\mainfrm.cpp"
$documentation = Read-ProjectFile "docs\xml-source-themes.md"
$dialogLocalization = Read-ProjectFile "localization\app-ui\fbe-small-dialogs.json"

$requiredTokens = @(
    "EDITOR_BACKGROUND", "EDITOR_FOREGROUND", "SELECTION_BACKGROUND",
    "SELECTION_FOREGROUND", "CURRENT_LINE_BACKGROUND", "CARET", "LINE_NUMBER",
    "LINE_NUMBER_ACTIVE", "MATCHING_TAG_BACKGROUND", "MATCHING_TAG_BORDER",
    "XML_TEXT", "XML_TAG_NAME", "XML_TAG_DELIMITER", "XML_ATTRIBUTE_NAME",
    "XML_ATTRIBUTE_VALUE", "XML_NAMESPACE", "XML_COMMENT", "XML_ENTITY",
    "XML_CDATA", "XML_PROCESSING_INSTRUCTION", "XML_DOCTYPE", "XML_ERROR",
    "XML_WARNING"
)

foreach($token in $requiredTokens) {
    if($settingsHeader -notlike "*XML_SRC_STYLE_$token*") {
        throw "В Settings.h отсутствует логическая роль XML_SRC_STYLE_$token."
    }
}

foreach($requiredApi in @("GetXmlSrcThemeColor", "GetXmlSrcStyleColor")) {
    if($settingsHeader -notlike "*$requiredApi*") {
        throw "В Settings.h отсутствует API темы: $requiredApi."
    }
    if($settingsSource -notlike "*$requiredApi*") {
        throw "В Settings.cpp отсутствует реализация API темы: $requiredApi."
    }
}

foreach($requiredPalette in @(
    "XML_SRC_COLOR_PALETTE_SYSTEM",
    "XML_SRC_COLOR_PALETTE_FBE_LIGHT",
    "XML_SRC_COLOR_PALETTE_FBE_DARK",
    "XML_SRC_COLOR_PALETTE_HISTORICAL"
)) {
    if($settingsHeader -notlike "*$requiredPalette*") {
        throw "В Settings.h отсутствует встроенная палитра: $requiredPalette."
    }
}

foreach($requiredLocalizationKey in @(
    "source_palette.system",
    "source_palette.fbe_light",
    "source_palette.fbe_dark",
    "source_palette.historical"
)) {
    if($dialogLocalization -notlike "*$requiredLocalizationKey*") {
        throw "В JSON-локализации отсутствует название палитры: $requiredLocalizationKey."
    }
}
foreach($requiredMapping in @(
    "SCE_H_TAGUNKNOWN,             XML_SRC_STYLE_XML_TAG_NAME",
    "SCE_H_ATTRIBUTEUNKNOWN,       XML_SRC_STYLE_XML_ATTRIBUTE_NAME",
    "SCE_H_CDATA,                  XML_SRC_STYLE_XML_CDATA",
    "SCE_H_ENTITY,                 XML_SRC_STYLE_XML_ENTITY",
    "SCE_H_SGML_ERROR,             XML_SRC_STYLE_XML_ERROR"
)) {
    if($mainFrame -notlike "*$requiredMapping*") {
        throw "В mainfrm.cpp отсутствует сопоставление Lexilla: $requiredMapping"
    }
}

foreach($legacyPalette in @( "XML_SRC_COLOR_PALETTE_LEGACY_CONTRAST", "XML_SRC_COLOR_PALETTE_LEGACY_HIGH_CONTRAST_DARK" )) {
    if($settingsHeader -notlike "*$legacyPalette*") {
        throw "В Settings.h отсутствует совместимое значение палитры: $legacyPalette."
    }
}

if($mainFrame -like "*_Settings.GetXmlSrcColor(styles[i].*") {
    throw "mainfrm.cpp использует устаревшие группы цветов вместо логических ролей темы."
}
foreach($requiredText in @('HasDocumentStyleConfigurationChanged', 'ApplyConfChanges(bool applyDocumentStyles)', 'ApplyConfChanges(HasDocumentStyleConfigurationChanged', 'HasOnlySourceEditorConfigurationChanged', 'ApplyXmlSourceEditorChanges', 'sourceThemeId', 'activeView == BODY')) {
    if($mainFrame -notlike "*$requiredText*") {
        throw "В mainfrm.cpp отсутствует разделение применения XML-темы и стилей редактора Тело: $requiredText"
    }
}

foreach($requiredText in @(
    "пользовательское правило конкретного тега",
    "семантическая группа FB2",
    "историческая светлая схема"
)) {
    if($documentation -notlike "*$requiredText*") {
        throw "В документации тем отсутствует обязательное описание: $requiredText"
    }
}

Write-Host "Контракт логических ролей XML-подсветки прошёл проверку."

# Проверка встроенных .fbetheme: формат, обязательные поля и уникальные id.
$themeDirectory = Join-Path $repoRoot "runtime\Themes"
$themeFiles = @(Get-ChildItem -LiteralPath $themeDirectory -File -Filter "*.fbetheme")
if($themeFiles.Count -eq 0) { throw "В runtime\Themes нет встроенных .fbetheme." }
if(@(Get-ChildItem -LiteralPath $themeDirectory -File -Filter "*.json").Count -ne 0) {
    throw "Встроенные темы должны использовать .fbetheme, а не .json."
}
$themeIds = @{}
foreach($themeFile in $themeFiles) {
    try { $theme = Get-Content -Raw -LiteralPath $themeFile.FullName | ConvertFrom-Json } catch { throw "Некорректный JSON темы $($themeFile.Name): $_" }
    if($theme.formatVersion -ne 1) { throw "Тема $($themeFile.Name) не использует formatVersion: 1." }
    if([string]::IsNullOrWhiteSpace($theme.id) -or [string]::IsNullOrWhiteSpace($theme.name)) { throw "В теме $($themeFile.Name) нет id или name." }
    if($themeIds.ContainsKey($theme.id)) { throw "Повторяющийся id встроенной темы: $($theme.id)." }
    $themeIds[$theme.id] = $true
    $requiredThemeColors = @(
        'editor.background','editor.foreground','editor.selection.background','editor.selection.foreground',
        'editor.currentLine.background','editor.caret','editor.lineNumber','editor.lineNumber.active',
        'editor.matchingTag.background','editor.matchingTag.border','xml.text','xml.tag.name',
        'xml.tag.delimiter','xml.attribute.name','xml.attribute.value','xml.namespace','xml.comment',
        'xml.entity','xml.cdata','xml.processingInstruction','xml.doctype','xml.error','xml.warning'
    )
    foreach($colorName in $requiredThemeColors) {
        $value = $theme.colors.$colorName
        if($value -notmatch '^#[0-9A-Fa-f]{6}$') { throw "Некорректный обязательный цвет $colorName в $($themeFile.Name)." }
    }
}

$themeSource = Read-ProjectFile "src\fbe\XmlSourceThemes.cpp"
foreach($requiredText in @('GetUserThemeDirectory', 'LoadThemesFromDirectory(GetThemeDirectory(), false', 'LoadThemesFromDirectory(userDirectory, true')) {
    if($themeSource -notlike "*$requiredText*") { throw "В XmlSourceThemes.cpp отсутствует разделение встроенных и пользовательских тем: $requiredText" }
}
# Сценарии из диалога настроек, которые нельзя надёжно проверить без GUI,
# фиксируем статическим контрактом: многофайловый импорт, сохранение/удаление
# пользовательской темы, обновление подсказок и экспорт фактической системной
# палитры вместо условной светлой схемы.
$settingsDialog = Read-ProjectFile "src\fbe\SettingsNextDlg.cpp"
foreach($requiredText in @(
    'OFN_ALLOWMULTISELECT',
    'SaveThemeAsUser',
    'DeleteUserTheme',
    'UpdateSourceColorTooltips',
    'UpdateSourceThemeDisplay',
    '\x0438\x0437\x043C\x0435\x043D\x0435\x043D\x0430',
    'color_automatic',
    'color_more',
    'GetXmlSrcThemeColor(XML_SRC_COLOR_PALETTE_SYSTEM'
)) {
    if($settingsDialog -notlike "*$requiredText*") {
        throw "В SettingsNextDlg.cpp отсутствует обязательный сценарий тем: $requiredText"
    }
}

foreach($requiredText in @('MakeAvailableUserThemeId', 'FindExternalTheme(base)', 'ReadLegacyAnsiThemeFile', 'ParseThemeFile(sourcePath, record, true)', 'FILE_ATTRIBUTE_READONLY', 'ExportThemeFile', 'MoveFileExW')) {
    if($themeSource -notlike "*$requiredText*") {
        throw "В XmlSourceThemes.cpp отсутствует безопасная операция с пользовательской темой: $requiredText"
    }
}

# Комментарии Lexilla пока не переживают полный цикл визуального редактирования.
# Поэтому не допускаем показа неработающего цвета комментариев или комментария в примере.
foreach($requiredText in @(
    'IDC_OPTIONS_SOURCE_COLOR_COMMENT_LABEL), SW_HIDE',
    'IDC_OPTIONS_SOURCE_COLOR_COMMENT), SW_HIDE'
)) {
    if($settingsDialog -notlike "*$requiredText*") {
        throw "Настройка XML-комментариев не скрыта: $requiredText"
    }
}
if($settingsDialog -like '*<!--*') {
    throw 'В предпросмотре не должен отображаться XML-комментарий до подтверждения сохранности модели документа.'
}

foreach($fixture in @('tools\tests\fb2-xml-comments-smoke.fb2', 'tools\tests\test-fb2-xml-comments.ps1')) {
    if(-not (Test-Path -LiteralPath (Join-Path $repoRoot $fixture))) {
        throw "Отсутствует проверочный материал XML-комментариев: $fixture"
    }
}

Write-Host "Формат .fbetheme и операции пользовательских тем прошли проверку."
