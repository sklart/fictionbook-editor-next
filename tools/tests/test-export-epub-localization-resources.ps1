<#
.SYNOPSIS
Проверяет generated-строки ExportEPUB.

.DESCRIPTION
Скрипт страхует переход всплывающих подсказок и подписей окна настроек ExportEPUB
на JSON→generated `.rc2`: проверяет, что `ExportEPUB.rc` подключает
generated-файл, в код окна настроек не вернулись hardcoded tooltip-строки, а
`ExportEPUBStrings.generated.rc2` синхронизирован с
`localization/plugin-ui/catalog.json`.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$rcPath = Join-Path $repoRoot "src\export-epub\ExportEPUB.rc"
$cppPath = Join-Path $repoRoot "src\export-epub\ExportEPUBPlugin.cpp"
$generatedRcPath = Join-Path $repoRoot "src\export-epub\ExportEPUBStrings.generated.rc2"

$rc = Get-Content -Raw -LiteralPath $rcPath
$cpp = Get-Content -Raw -LiteralPath $cppPath
if (-not (Test-Path -LiteralPath $generatedRcPath)) {
    throw "Сгенерированный файл строк ExportEPUB не найден: $generatedRcPath"
}
$generatedRc = Get-Content -Raw -LiteralPath $generatedRcPath

if ($rc -notmatch '#include\s+"ExportEPUBStrings\.generated\.rc2"') {
    throw "ExportEPUB.rc не подключает ExportEPUBStrings.generated.rc2."
}
if ($cpp -match 'AddTooltip\([^,]+,\s*[^,]+,\s*IDC_[A-Z0-9_]+,\s*L"') {
    throw "В ExportEPUBPlugin.cpp вернулась hardcoded tooltip-строка; подсказки должны загружаться из ресурсов."
}
if ($cpp -notmatch 'AddTooltipString\([^,]+,\s*[^,]+,\s*IDC_CHECK_NCX_FALLBACK,\s*IDS_TOOLTIP_NCX_FALLBACK') {
    throw "ExportEPUBPlugin.cpp не использует ресурсные tooltip-строки."
}

if ($cpp -notmatch 'ApplyExportOptionsDialogText\(hwnd\)') {
    throw "ExportEPUBPlugin.cpp не применяет ресурсные подписи окна настроек."
}

if ($cpp -notmatch 'customize->AddPushButton\(IDC_BUTTON_EXPORT_OPTIONS,\s*LoadExportEpubString\(IDS_SAVE_DIALOG_BUTTON_EXPORT_OPTIONS') {
    throw "ExportEPUBPlugin.cpp не использует ресурсную подпись кнопки параметров modern save-dialog."
}

if ($cpp -match 'result\.warnings\.emplace_back\(L"[А-Яа-яЁё]' -or
    $cpp -match 'w\s*<<\s*L"[А-Яа-яЁё][^"]*"' -or
    $cpp -match 'text\s*\+=\s*L"\\n[А-Яа-яЁё]') {
    throw "В ExportEPUBPlugin.cpp вернулась hardcoded русская строка preflight/summary UI."
}
if ($cpp -match 'error\.empty\(\) \? L"Unknown error"') {
    throw "В ExportEPUBPlugin.cpp вернулась hardcoded fallback-строка Unknown error."
}

$resourceHeaderPath = Join-Path (Join-Path $repoRoot "src\export-epub") "resource.h"
$resourceHeader = Get-Content -Raw -LiteralPath $resourceHeaderPath
$requiredResourceIds = @(
    [regex]::Matches($resourceHeader, '^#define\s+(IDS_[A-Z0-9_]+)\s+(\d+)', [System.Text.RegularExpressions.RegexOptions]::Multiline) |
        Where-Object { [int]$_.Groups[2].Value -ge 200 } |
        ForEach-Object { $_.Groups[1].Value }
)
if ($requiredResourceIds.Count -eq 0) {
    throw "В resource.h не найдены generated IDS_* строки ExportEPUB с ID >= 200."
}
$requiredResourceIds += "IDS_ERROR_UNKNOWN"

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
        throw "В ExportEPUBStrings.generated.rc2 отсутствует языковой блок: $language"
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
            throw "В языковом блоке $language отсутствует обязательная строка ExportEPUB: $id"
        }
    }
}

$tempDirectory = Join-Path ([IO.Path]::GetTempPath()) "fbe-export-epub-generated-strings-$PID"
try {
    New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
    $tempGeneratedPath = Join-Path $tempDirectory "ExportEPUBStrings.generated.rc2"
    & (Join-Path $repoRoot "tools\localization\update-export-epub-resource-strings.ps1") -OutputPath $tempGeneratedPath | Out-Host

    $expected = [IO.File]::ReadAllBytes($tempGeneratedPath)
    $actual = [IO.File]::ReadAllBytes($generatedRcPath)
    if ($expected.Length -ne $actual.Length) {
        throw "ExportEPUBStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
    }
    for ($i = 0; $i -lt $expected.Length; $i++) {
        if ($expected[$i] -ne $actual[$i]) {
            throw "ExportEPUBStrings.generated.rc2 не синхронизирован с localization/plugin-ui/catalog.json."
        }
    }
}
finally {
    Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}

if ($generatedRc -cmatch '�|Ð.|Ñ.|Ã.|Â.') {
    throw "В ExportEPUBStrings.generated.rc2 обнаружены признаки mojibake."
}

Write-Host "Проверка локализации ExportEPUB прошла успешно."
Write-Host "  Языковых блоков: $($requiredLanguageBlocks.Count)"
Write-Host "  Контрольных строк на язык: $($requiredResourceIds.Count)"
