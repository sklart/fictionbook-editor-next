<#
.SYNOPSIS
Проверяет generated-строки окна настроек ImportEPUB.

.DESCRIPTION
Скрипт страхует переход окна настроек ImportEPUB на JSON→generated `.rc2`:
проверяет, что `ImportEPUB.rc` подключает generated-файл, в код окна не
вернулись hardcoded русские подписи/подсказки, а
`ImportEPUBStrings.generated.rc2` синхронизирован с
`localization/plugin-ui/catalog.json`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$rcPath = Join-Path $repoRoot "src\import-epub\ImportEPUB.rc"
$cppPath = Join-Path $repoRoot "src\import-epub\ImportOptionsDialog.cpp"
$pluginCppPath = Join-Path $repoRoot "src\import-epub\ImportEPUBPlugin.cpp"
$generatedRcPath = Join-Path $repoRoot "src\import-epub\ImportEPUBStrings.generated.rc2"

$rc = Get-Content -Raw -LiteralPath $rcPath
$cpp = Get-Content -Raw -LiteralPath $cppPath
$pluginCpp = Get-Content -Raw -LiteralPath $pluginCppPath
if (-not (Test-Path -LiteralPath $generatedRcPath)) {
    throw "Сгенерированный файл строк ImportEPUB не найден: $generatedRcPath"
}
$generatedRc = Get-Content -Raw -LiteralPath $generatedRcPath

if ($rc -notmatch '#include\s+"ImportEPUBStrings\.generated\.rc2"') {
    throw "ImportEPUB.rc не подключает ImportEPUBStrings.generated.rc2."
}
if ($cpp -match 'CreateWindowW\([^,]+,\s*L"[А-Яа-яЁё]') {
    throw "В ImportOptionsDialog.cpp вернулась hardcoded русская подпись окна."
}
if ($cpp -match 'AddTooltip\([^,]+,\s*L"') {
    throw "В ImportOptionsDialog.cpp вернулась hardcoded tooltip-строка; подсказки должны загружаться из ресурсов."
}
if ($cpp -match 'CB_ADDSTRING,\s*0,\s*reinterpret_cast<LPARAM>\(L"') {
    throw "В ImportOptionsDialog.cpp вернулась hardcoded строка combo box; варианты должны загружаться из ресурсов."
}
if ($pluginCpp -match 'SetTitle\(L"Импорт EPUB"\)' -or
    $pluginCpp -match 'SetOkButtonLabel\(L"Открыть\.\.\."\)' -or
    $pluginCpp -match 'SetFileNameLabel\(L"EPUB-файл:"\)' -or
    $pluginCpp -match 'AddPushButton\([^,]+,\s*L"Настройки импорта\.\.\."\)') {
    throw "В ImportEPUBPlugin.cpp вернулась hardcoded строка file-dialog; строки должны загружаться из ресурсов."
}
if ($pluginCpp -notmatch 'LoadPluginString\(\s*IDS_IMPORT_PLUGIN_FILEDLG_TITLE' -or
    $pluginCpp -notmatch 'LoadPluginString\(\s*IDS_IMPORT_PLUGIN_ERROR_COM') {
    throw "ImportEPUBPlugin.cpp не использует ресурсные строки основного COM-плагина."
}

$resourceHeaderPath = Join-Path (Join-Path $repoRoot "src\import-epub") "resource.h"
$resourceHeader = Get-Content -Raw -LiteralPath $resourceHeaderPath
$requiredResourceIds = @(
    [regex]::Matches($resourceHeader, '^#define\s+(IDS_IMPORT_[A-Z0-9_]+)\s+\d+', [System.Text.RegularExpressions.RegexOptions]::Multiline) |
        ForEach-Object { $_.Groups[1].Value }
)
if ($requiredResourceIds.Count -eq 0) {
    throw "В resource.h не найдены IDS_IMPORT_* строки для проверки ImportEPUB."
}

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
        throw "В ImportEPUBStrings.generated.rc2 отсутствует языковой блок: $language"
    }

    $nextLanguageMatch = [regex]::Match(
        $generatedRc.Substring($languageMatch.Index + $languageMatch.Length),
        "LANGUAGE\s+LANG_[A-Z_]+"
    )
    if ($nextLanguageMatch.Success) {
        $block = $generatedRc.Substring($languageMatch.Index, $languageMatch.Length + $nextLanguageMatch.Index)
    } else {
        $block = $generatedRc.Substring($languageMatch.Index)
    }

    foreach ($id in $requiredResourceIds) {
        if ($block -notmatch "\b$([regex]::Escape($id))\b") {
            throw "В языковом блоке $language отсутствует обязательная строка ImportEPUB: $id"
        }
    }
}

$tempDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-import-epub-generated-strings-$PID"
try {
    New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
    $tempGeneratedPath = Join-Path $tempDirectory "ImportEPUBStrings.generated.rc2"
    & (Join-Path $repoRoot "tools\localization\update-import-epub-resource-strings.ps1") -OutputPath $tempGeneratedPath | Out-Host

    $expected = [IO.File]::ReadAllBytes($tempGeneratedPath)
    $actual = [IO.File]::ReadAllBytes($generatedRcPath)
    if ($expected.Length -ne $actual.Length) {
        throw "ImportEPUBStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
    }
    for ($i = 0; $i -lt $expected.Length; $i++) {
        if ($expected[$i] -ne $actual[$i]) {
            throw "ImportEPUBStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
        }
    }
}
finally {
    Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

if ($generatedRc -cmatch '�|Ð.|Ñ.|Ã.|Â.') {
    throw "В ImportEPUBStrings.generated.rc2 обнаружены признаки mojibake."
}

Write-Host "Проверка локализации ImportEPUB прошла успешно."
Write-Host "  Языковых блоков: $($requiredLanguageBlocks.Count)"
Write-Host "  Контрольных строк на язык: $($requiredResourceIds.Count)"
