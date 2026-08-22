<#
.SYNOPSIS
Проверяет generated-строки ExportHTML.

.DESCRIPTION
Скрипт страхует переход ExportHTML на JSON→generated `.rc2`: проверяет, что
`ExportHTML.rc` подключает generated-файл, ручная `STRINGTABLE` с runtime-
строками не вернулась, а `ExportHTMLStrings.generated.rc2` синхронизирован с
`localization/plugin-ui/catalog.json` и содержит строки runtime/tooltip.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$rcPath = Join-Path $repoRoot "src\export-html\ExportHTML.rc"
$generatedRcPath = Join-Path $repoRoot "src\export-html\ExportHTMLStrings.generated.rc2"
$dialogPath = Join-Path $repoRoot "src\export-html\CustomFileSaveDialog.h"

$rc = Get-Content -Raw -LiteralPath $rcPath
if (-not (Test-Path -LiteralPath $generatedRcPath)) {
    throw "Сгенерированный файл строк ExportHTML не найден: $generatedRcPath"
}
$generatedRc = Get-Content -Raw -LiteralPath $generatedRcPath
$dialog = Get-Content -Raw -LiteralPath $dialogPath

if ($rc -notmatch '#include\s+"ExportHTMLStrings\.generated\.rc2"') {
    throw "ExportHTML.rc не подключает ExportHTMLStrings.generated.rc2."
}

if ($rc -match 'IDS_SAVE_FILE_FILTER\s+"') {
    throw "В ExportHTML.rc вернулась ручная STRINGTABLE-строка; runtime-строки должны генерироваться из JSON."
}

foreach ($controlId in @("IDC_TEMPLATE_LABEL", "IDC_TOC_DEPTH_LABEL")) {
    if ($rc -notmatch ("LTEXT\s+[^\r\n]*" + [regex]::Escape($controlId))) {
        throw "ExportHTML.rc должен использовать отдельный control ID $controlId вместо IDC_STATIC."
    }
}

foreach ($expectedCall in @(
    "SetDlgItemText(IDC_TEMPLATE_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_TEMPLATE_LABEL))",
    "SetDlgItemText(IDC_DOCINFO, LoadExportHtmlString(IDS_CUSTOM_SAVE_INCLUDE_DESC))",
    "SetDlgItemText(IDC_TOC_DEPTH_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_TOC_DEPTH))",
	"SetDlgItemText(IDC_CUSTOM_CSS_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_CUSTOM_CSS))",
	"SetDlgItemText(IDC_IMAGE_MAX_WIDTH_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH))",
	"SetDlgItemText(IDC_IMAGE_MAX_HEIGHT_LABEL, LoadExportHtmlString(IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT))",
    "LoadExportHtmlString(IDS_OPEN_TEMPLATE_FILTER)"
)) {
    if ($dialog -notmatch [regex]::Escape($expectedCall)) {
        throw "CustomFileSaveDialog.h не применяет локализованную строку: $expectedCall"
    }
}

$requiredResourceIds = @(
    "IDR_EXPORTHTML",
    "IDS_ERROR_OPEN_FILE",
    "IDS_ERROR_CREATE_DIRECTORY",
    "IDS_ERROR_WRITE_FILE",
    "IDS_ERROR_WRITE_FILE2",
	"IDS_ERROR_EMBEDDED_IMAGES_TEMPLATE",
    "IDS_WARNING_FILE_ALREADY_EXISTS",
    "IDS_SAVE_FILE_FILTER",
    "IDS_XML_PARSE_ERROR",
    "IDS_AT_LINE_COLUMN",
    "IDS_AT_S_S",
    "IDS_ERROR",
    "IDS_COM_ERROR",
    "IDS_TOOLTIP_TEMPLATE",
    "IDS_TOOLTIP_BROWSE_TEMPLATE",
    "IDS_TOOLTIP_DOCINFO",
    "IDS_TOOLTIP_TOC_DEPTH",
    "IDS_CUSTOM_SAVE_TEMPLATE_LABEL",
    "IDS_CUSTOM_SAVE_INCLUDE_DESC",
    "IDS_CUSTOM_SAVE_TOC_DEPTH",
    "IDS_OPEN_TEMPLATE_FILTER",
    "IDS_UNKNOWN_ERROR",
	"IDS_CUSTOM_SAVE_CUSTOM_CSS",
	"IDS_CUSTOM_SAVE_IMAGE_MAX_WIDTH",
	"IDS_CUSTOM_SAVE_IMAGE_MAX_HEIGHT"
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
        throw "В ExportHTMLStrings.generated.rc2 отсутствует языковой блок: $language"
    }

    $nextLanguageMatch = [regex]::Match($generatedRc.Substring($languageMatch.Index + $languageMatch.Length), "LANGUAGE\s+LANG_[A-Z_]+")
    if ($nextLanguageMatch.Success) {
        $block = $generatedRc.Substring($languageMatch.Index, $languageMatch.Length + $nextLanguageMatch.Index)
    } else {
        $block = $generatedRc.Substring($languageMatch.Index)
    }

    foreach ($id in $requiredResourceIds) {
        if ($block -notmatch "\b$([regex]::Escape($id))\b") {
            throw "В языковом блоке $language отсутствует обязательная строка ExportHTML: $id"
        }
    }
}

$tempDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-export-html-generated-strings-$PID"
try {
    New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
    $tempGeneratedPath = Join-Path $tempDirectory "ExportHTMLStrings.generated.rc2"
    & (Join-Path $repoRoot "tools\localization\update-export-html-resource-strings.ps1") -OutputPath $tempGeneratedPath | Out-Host

    $expected = [IO.File]::ReadAllBytes($tempGeneratedPath)
    $actual = [IO.File]::ReadAllBytes($generatedRcPath)
    if ($expected.Length -ne $actual.Length) {
        throw "ExportHTMLStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
    }
    for ($i = 0; $i -lt $expected.Length; $i++) {
        if ($expected[$i] -ne $actual[$i]) {
            throw "ExportHTMLStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
        }
    }
}
finally {
    Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

if ($generatedRc -cmatch '�|Ð.|Ñ.|Ã.|Â.') {
    throw "В ExportHTMLStrings.generated.rc2 обнаружены признаки mojibake."
}

Write-Host "Проверка локализации runtime/tooltip-строк ExportHTML прошла успешно."
Write-Host "  Языковых блоков: $($requiredLanguageBlocks.Count)"
Write-Host "  Строк на язык: $($requiredResourceIds.Count)"
