<#
.SYNOPSIS
Проверяет generated-строки ExportDOCX.

.DESCRIPTION
Скрипт страхует переход ExportDOCX на JSON→generated `.rc2`: проверяет, что
`ExportDOCX.rc` подключает generated-файл, ручная `STRINGTABLE` с runtime-
строками не вернулась, hardcoded tooltip-строки не используются, а
`ExportDOCXStrings.generated.rc2` синхронизирован с `localization/plugin-ui/catalog.json`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$rcPath = Join-Path $repoRoot "src\export-docx\ExportDOCX.rc"
$cppPath = Join-Path $repoRoot "src\export-docx\ExportDOCXPlugin.cpp"
$generatedRcPath = Join-Path $repoRoot "src\export-docx\ExportDOCXStrings.generated.rc2"

$rc = Get-Content -Raw -LiteralPath $rcPath
$cpp = Get-Content -Raw -LiteralPath $cppPath
if (-not (Test-Path -LiteralPath $generatedRcPath)) {
    throw "Сгенерированный файл строк ExportDOCX не найден: $generatedRcPath"
}
$generatedRc = Get-Content -Raw -LiteralPath $generatedRcPath

$imagesGroup = [regex]::Match($rc, 'GROUPBOX\s+"Изображения и структура",IDC_GRP_IMAGES,18,36,394,(?<height>\d+)')
$hyperlinks = [regex]::Match($rc, 'CONTROL\s+"Преобразовывать гиперссылки",IDC_EXPORT_HYPERLINKS,"Button",[^\r\n]*,30,(?<top>\d+),240,(?<height>\d+)')
if (-not $imagesGroup.Success -or -not $hyperlinks.Success) {
    throw "Не найдены элементы главной страницы настроек ExportDOCX для проверки разметки."
}
if (36 + [int]$imagesGroup.Groups["height"].Value -lt [int]$hyperlinks.Groups["top"].Value + [int]$hyperlinks.Groups["height"].Value + 4) {
    throw "Рамка «Изображения и структура» не оставляет нижнего отступа для настройки гиперссылок."
}

if ($rc -notmatch '#include\s+"ExportDOCXStrings\.generated\.rc2"') {
    throw "ExportDOCX.rc не подключает ExportDOCXStrings.generated.rc2."
}
if ($rc -match 'IDS_SAVE_FILE_FILTER\s+"') {
    throw "В ExportDOCX.rc вернулась ручная STRINGTABLE-строка; runtime-строки должны генерироваться из JSON."
}
if ($cpp -match 'AddTooltip\([^,]+,\s*L"') {
    throw "В ExportDOCXPlugin.cpp вернулась hardcoded tooltip-строка; подсказки должны загружаться из ресурсов."
}

$requiredResourceIds = @(
    "IDR_EXPORTDOCX",
    "IDS_ERROR_OPEN_FILE",
    "IDS_ERROR_CREATE_DIRECTORY",
    "IDS_ERROR_WRITE_FILE",
    "IDS_ERROR_WRITE_FILE2",
    "IDS_WARNING_FILE_ALREADY_EXISTS",
    "IDS_SAVE_FILE_FILTER",
    "IDS_XML_PARSE_ERROR",
    "IDS_AT_LINE_COLUMN",
    "IDS_AT_S_S",
    "IDS_ERROR",
    "IDS_COM_ERROR",
    "IDS_UNKNOWN_ERROR",
    "IDS_TOOLTIP_EXPORT_IMAGES",
    "IDS_TOOLTIP_EXPORT_COVER",
    "IDS_TOOLTIP_LIMIT_IMAGE_WIDTH",
    "IDS_TOOLTIP_IMAGE_MAX_WIDTH_CM",
    "IDS_TOOLTIP_TITLE_PAGE",
    "IDS_TOOLTIP_TITLE_INCLUDE_ANNOTATION",
    "IDS_TOOLTIP_TITLE_INCLUDE_GENRES",
    "IDS_TOOLTIP_TITLE_INCLUDE_SERIES",
    "IDS_TOOLTIP_TITLE_INCLUDE_FB2_INFO",
    "IDS_TOOLTIP_NOTES_MODE",
    "IDS_TOOLTIP_ADD_TOC",
    "IDS_TOOLTIP_TOC_DEPTH",
    "IDS_TOOLTIP_CREATE_BOOKMARKS",
    "IDS_TOOLTIP_EXPORT_HYPERLINKS",
    "IDS_TOOLTIP_EXPORT_METADATA",
    "IDS_TOOLTIP_VALIDATE_DOCX",
    "IDS_TOOLTIP_CREATE_REPORT",
    "IDS_TOOLTIP_JUSTIFY_TEXT",
    "IDS_TOOLTIP_FIRST_LINE_INDENT",
    "IDS_TOOLTIP_CHAPTER_PAGE_BREAK",
    "IDS_TOOLTIP_ENHANCED_FB2_STYLES",
    "IDS_TOOLTIP_CUSTOM_FONT",
    "IDS_TOOLTIP_FONT_NAME",
    "IDS_TOOLTIP_FONT_SIZE",
    "IDS_TOOLTIP_PAGE_SIZE",
    "IDS_TOOLTIP_EMPTY_LINE_MODE",
    "IDS_TOOLTIP_ADD_HEADERS",
    "IDS_TOOLTIP_ADD_PAGE_NUMBERS",
    "IDS_TOOLTIP_NO_TITLE_PAGE_NUMBER",
    "IDS_TOOLTIP_RESTART_PAGE_NUMBERING",
    "IDS_TOOLTIP_AUTO_HYPHENATION",
    "IDS_TOOLTIP_OPEN_AFTER_EXPORT",
    "IDS_TOOLTIP_DOC_LANGUAGE",
    "IDS_TOOLTIP_RESET_DEFAULTS",
    "IDS_TOOLTIP_PRESET_BOOK",
    "IDS_TOOLTIP_PRESET_MINIMAL",
    "IDS_TOOLTIP_PRESET_EDITORIAL",
    "IDS_TOOLTIP_STYLE_PROFILE",
    "IDS_DOCX_TAB_MAIN",
    "IDS_DOCX_TAB_NOTES",
    "IDS_DOCX_TAB_FORMAT",
    "IDS_DOCX_TAB_ADVANCED",
    "IDS_DOCX_NOTES_FOOTNOTES",
    "IDS_DOCX_NOTES_ENDNOTES",
    "IDS_DOCX_NOTES_SECTION",
    "IDS_DOCX_EMPTY_LINE_IGNORE",
    "IDS_DOCX_EMPTY_LINE_PARAGRAPH",
    "IDS_DOCX_EMPTY_LINE_SPACING",
    "IDS_DOCX_EMPTY_LINE_SEPARATOR",
    "IDS_DOCX_LANGUAGE_RU",
    "IDS_DOCX_LANGUAGE_EN",
    "IDS_DOCX_LANGUAGE_AUTO",
    "IDS_DOCX_PROFILE_BOOK",
    "IDS_DOCX_PROFILE_COMPACT",
    "IDS_DOCX_PROFILE_MINIMAL",
    "IDS_DOCX_SAVE_BUTTON",
    "IDS_DOCX_TITLE_TRANSLATION",
    "IDS_DOCX_TITLE_ANNOTATION",
    "IDS_DOCX_TITLE_GENRES",
    "IDS_DOCX_TITLE_SERIES",
    "IDS_DOCX_TITLE_DATE",
    "IDS_DOCX_TITLE_LANGUAGE",
    "IDS_DOCX_TITLE_PUBLISH_INFO",
    "IDS_DOCX_TITLE_SOURCE_INFO",
    "IDS_DOCX_TITLE_FB2_VERSION",
    "IDS_DOCX_TITLE_PROGRAM",
    "IDS_DOCX_TITLE_HISTORY",
    "IDS_DOCX_REPORT_FILE",
    "IDS_DOCX_REPORT_TITLE",
    "IDS_DOCX_REPORT_AUTHORS",
    "IDS_DOCX_REPORT_WORD_LANGUAGE",
    "IDS_DOCX_REPORT_SOURCE_LANGUAGE",
    "IDS_DOCX_REPORT_FB2_SECTIONS",
    "IDS_DOCX_REPORT_DOCX_PARAGRAPHS",
    "IDS_DOCX_REPORT_TABLES",
    "IDS_DOCX_REPORT_IMAGES_REFERENCED",
    "IDS_DOCX_REPORT_IMAGES_EMBEDDED",
    "IDS_DOCX_REPORT_COVERS_INSERTED",
    "IDS_DOCX_REPORT_IMAGES_MISSING",
    "IDS_DOCX_REPORT_NOTE_LINKS",
    "IDS_DOCX_REPORT_NOTES_CREATED",
    "IDS_DOCX_REPORT_NOTES_MISSING",
    "IDS_DOCX_REPORT_EXTERNAL_LINKS",
    "IDS_DOCX_REPORT_INTERNAL_LINKS_TOTAL",
    "IDS_DOCX_REPORT_INTERNAL_LINKS_RESOLVED",
    "IDS_DOCX_REPORT_INTERNAL_LINKS_BROKEN",
    "IDS_DOCX_REPORT_INTERNAL_TARGETS",
    "IDS_DOCX_REPORT_DUP_BOOKMARKS_SKIPPED",
    "IDS_DOCX_REPORT_FB2_STYLESHEETS",
    "IDS_DOCX_REPORT_SMALL_IMAGES",
    "IDS_DOCX_REPORT_HEIGHT_LIMITED_IMAGES",
    "IDS_DOCX_REPORT_WORD_BOOKMARKS",
    "IDS_DOCX_REPORT_SETTINGS_HEADER",
    "IDS_DOCX_REPORT_TITLE_PAGE",
    "IDS_DOCX_REPORT_TOC",
    "IDS_DOCX_REPORT_IMAGES",
    "IDS_DOCX_REPORT_NOTES",
    "IDS_DOCX_REPORT_PAGE_SIZE",
    "IDS_DOCX_REPORT_TOC_DEPTH",
    "IDS_DOCX_REPORT_EMPTY_LINE",
    "IDS_DOCX_REPORT_OPEN_AFTER_EXPORT",
    "IDS_DOCX_REPORT_WARNINGS_HEADER",
    "IDS_DOCX_REPORT_YES",
    "IDS_DOCX_REPORT_NO",
    "IDS_DOCX_TOC_TITLE",
    "IDS_DOCX_TOC_PLACEHOLDER",
    "IDS_DOCX_TOC_UPDATE_HINT",
    "IDS_DOCX_WARNING_RECURSIVE_NOTE",
    "IDS_DOCX_WARNING_FB2_STYLESHEET",
    "IDS_DOCX_WARNING_INTERNAL_LINK_BOOKMARKS_DISABLED",
    "IDS_DOCX_WARNING_INTERNAL_LINK_MISSING_TARGET",
    "IDS_DOCX_WARNING_IMAGE_NOT_FOUND",
    "IDS_DOCX_WARNING_IMAGE_BASE64_FAILED",
    "IDS_DOCX_WARNING_IMAGE_UNKNOWN_CONTENT_TYPE",
    "IDS_DOCX_WARNING_IMAGE_SIZE_FAILED",
    "IDS_DOCX_WARNING_NOTE_MISSING",
    "IDS_DOCX_WARNING_TABLE_NO_ROWS",
    "IDS_DOCX_WARNING_TABLE_ROWSPAN_COLSPAN",
    "IDS_DOCX_WARNING_DOCUMENT_NO_BODY",
    "IDS_DOCX_WARNING_TOC_FIELD_MISSING",
    "IDS_DOCX_WARNING_EMPTY_IMAGE",
    "IDS_DOCX_WARNING_IMAGE_REL_MISSING",
    "IDS_DOCX_WARNING_EMPTY_ENDNOTE",
    "IDS_DOCX_WARNING_EMPTY_FOOTNOTE",
    "IDS_DOCX_WARNING_HYPERLINK_REL_MISSING",
    "IDS_DOCX_WARNING_HEADER_TITLE_MISSING",
    "IDS_DOCX_WARNING_TABLE_XML_MISSING",
    "IDS_DOCX_WARNING_REFERENCED_IMAGES_NOT_EMBEDDED",
    "IDS_DOCX_WARNING_AUTO_LANGUAGE_EMPTY",
    "IDS_DOCX_WARNING_DOCX_CREATED_WITH_WARNINGS",
    "IDS_DOCX_NOTES_TITLE"
)

$requiredLanguageBlocks = @(
    "LANG_RUSSIAN",
    "LANG_UKRAINIAN",
    "LANG_GERMAN",
    "LANG_FRENCH",
    "LANG_SPANISH",
    "LANG_ITALIAN",
    "LANG_POLISH",
    "LANG_CZECH",
    "LANG_BULGARIAN",
    "LANG_PORTUGUESE",
    "LANG_DUTCH",
    "LANG_ENGLISH"
)

foreach ($language in $requiredLanguageBlocks) {
    $languagePattern = "LANGUAGE\s+$([regex]::Escape($language))\b"
    $languageMatch = [regex]::Match($generatedRc, $languagePattern)
    if (-not $languageMatch.Success) {
        throw "В ExportDOCXStrings.generated.rc2 отсутствует языковой блок: $language"
    }

    $nextLanguageMatch = [regex]::Match($generatedRc.Substring($languageMatch.Index + $languageMatch.Length), "LANGUAGE\s+LANG_[A-Z_]+")
    if ($nextLanguageMatch.Success) {
        $block = $generatedRc.Substring($languageMatch.Index, $languageMatch.Length + $nextLanguageMatch.Index)
    } else {
        $block = $generatedRc.Substring($languageMatch.Index)
    }

    foreach ($id in $requiredResourceIds) {
        if ($block -notmatch "\b$([regex]::Escape($id))\b") {
            throw "В языковом блоке $language отсутствует обязательная строка ExportDOCX: $id"
        }
    }
}

$tempDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-export-docx-generated-strings-$PID"
try {
    New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
    $tempGeneratedPath = Join-Path $tempDirectory "ExportDOCXStrings.generated.rc2"
    & (Join-Path $repoRoot "tools\localization\update-export-docx-resource-strings.ps1") -OutputPath $tempGeneratedPath | Out-Host

    $expected = [IO.File]::ReadAllBytes($tempGeneratedPath)
    $actual = [IO.File]::ReadAllBytes($generatedRcPath)
    if ($expected.Length -ne $actual.Length) {
        throw "ExportDOCXStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
    }
    for ($i = 0; $i -lt $expected.Length; $i++) {
        if ($expected[$i] -ne $actual[$i]) {
            throw "ExportDOCXStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
        }
    }
}
finally {
    Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

if ($generatedRc -cmatch '�|Ð.|Ñ.|Ã.|Â.') {
    throw "В ExportDOCXStrings.generated.rc2 обнаружены признаки mojibake."
}

Write-Host "Проверка локализации runtime/tooltip-строк ExportDOCX прошла успешно."
Write-Host "  Языковых блоков: $($requiredLanguageBlocks.Count)"
Write-Host "  Контрольных строк на язык: $($requiredResourceIds.Count)"
