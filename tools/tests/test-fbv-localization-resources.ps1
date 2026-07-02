<#
.SYNOPSIS
Проверяет, что основные пользовательские строки FBV вынесены в ресурсы.

.DESCRIPTION
Скрипт страхует локализацию FBV: ищет возврат старых hardcoded UI-строк в
`src/fbv/FBV.cpp` и проверяет, что каждый языковой `STRINGTABLE` в
`src/fbv/FBV.rc` содержит полный набор runtime-строк, используемых через
`LoadString`. Диалоговый layout FBV пока остаётся в `.rc`, но runtime-сообщения
должны загружаться из ресурсов.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$cppPath = Join-Path $repoRoot "src\fbv\FBV.cpp"
$rcPath = Join-Path $repoRoot "src\fbv\FBV.rc"

$cpp = Get-Content -Raw -LiteralPath $cppPath
$rc = Get-Content -Raw -LiteralPath $rcPath

$forbiddenCppPatterns = @(
    '_T\("Done\."\)',
    '_T\("No errors\."\)',
    '_T\("File name"\)',
    '_T\("Scanning\.\.\."\)',
    '_T\("Re&validate"\)',
    '_T\("&Stop"\)',
    '_T\("E&xit"\)',
    'L"At line %d, column %d:'
)

foreach ($pattern in $forbiddenCppPatterns) {
    if ($cpp -match $pattern) {
        throw "В FBV.cpp найдена hardcoded UI-строка, которую нужно держать в ресурсах: $pattern"
    }
}

$requiredRuntimeResourceIds = @(
    "IDS_STATUS_DONE",
    "IDS_BUTTON_STOP",
    "IDS_BUTTON_REVALIDATE",
    "IDS_COLUMN_FILE_NAME",
    "IDS_VALIDATION_NO_ERRORS",
    "IDS_SAX_ERROR_LOCATION",
    "IDS_BUTTON_EXIT",
    "IDS_STATUS_SCANNING",
    "IDS_ERROR",
    "IDS_CANNOT_LOAD_SCHEMA",
    "IDS_COM_ERROR_FORMAT"
)

$requiredLanguageBlocks = [ordered]@{
    "LANG_RUSSIAN" = "Russian"
    "LANG_UKRAINIAN" = "Ukrainian"
    "LANG_GERMAN" = "German"
    "LANG_FRENCH" = "French"
    "LANG_SPANISH" = "Spanish"
    "LANG_ITALIAN" = "Italian"
    "LANG_POLISH" = "Polish"
    "LANG_CZECH" = "Czech"
    "LANG_BULGARIAN" = "Bulgarian"
    "LANG_PORTUGUESE" = "Portuguese"
    "LANG_DUTCH" = "Dutch"
    "LANG_ENGLISH" = "English"
}

foreach ($language in $requiredLanguageBlocks.Keys) {
    $languagePattern = "LANGUAGE\s+$([regex]::Escape($language))\b"
    $languageMatch = [regex]::Match($rc, $languagePattern)
    if (-not $languageMatch.Success) {
        throw "В FBV.rc отсутствует языковой блок: $language"
    }

    $nextLanguageMatch = [regex]::Match($rc.Substring($languageMatch.Index + $languageMatch.Length), "LANGUAGE\s+LANG_[A-Z_]+")
    if ($nextLanguageMatch.Success) {
        $block = $rc.Substring($languageMatch.Index, $languageMatch.Length + $nextLanguageMatch.Index)
    }
    else {
        $block = $rc.Substring($languageMatch.Index)
    }

    foreach ($id in $requiredRuntimeResourceIds) {
        if ($block -notmatch "\b$([regex]::Escape($id))\b") {
            throw "В языковом блоке $language отсутствует обязательная строка FBV: $id"
        }
    }
}

if ($cpp -notmatch "LoadFbString\(IDS_VALIDATION_NO_ERRORS\)" -or
    $cpp -notmatch "LoadFbString\(IDS_STATUS_DONE\)" -or
    $cpp -notmatch "LoadFbString\(IDS_SAX_ERROR_LOCATION\)") {
    throw "FBV.cpp не использует LoadFbString для ключевых runtime-сообщений."
}

Write-Host "Проверка локализации runtime-строк FBV прошла успешно."
Write-Host "  Языковых блоков: $($requiredLanguageBlocks.Count)"
Write-Host "  Runtime-строк на язык: $($requiredRuntimeResourceIds.Count)"
