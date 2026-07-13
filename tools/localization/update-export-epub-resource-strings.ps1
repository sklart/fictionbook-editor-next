<#
.SYNOPSIS
Обновляет встроенные строковые ресурсы ExportEPUB из JSON-каталога локализации.

.DESCRIPTION
Скрипт читает `localization/plugin-ui/catalog.json`, выбирает строки компонентов
`export-epub.*` с `IDS_*` идентификаторами и генерирует
`src/export-epub/ExportEPUBStrings.generated.rc2`. Файл подключается из
`ExportEPUB.rc`, поэтому окно настроек EPUB и его всплывающие подсказки получают
встроенный fallback из того же JSON-источника, который позже можно отдавать в
Weblate.
#>
[CmdletBinding()]
param(
    [string]$CatalogPath,
    [string]$OutputPath,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($CatalogPath)) {
    $CatalogPath = Join-Path $repoRoot "localization\plugin-ui\catalog.json"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "src\export-epub\ExportEPUBStrings.generated.rc2"
}

$languageResources = [ordered]@{
    "ru-RU" = @{ Comment = "Russian (Russia)"; Language = "LANG_RUSSIAN, SUBLANG_DEFAULT"; CodePage = 1251 }
    "uk-UA" = @{ Comment = "Ukrainian"; Language = "LANG_UKRAINIAN, SUBLANG_DEFAULT"; CodePage = 1251 }
    "de-DE" = @{ Comment = "German"; Language = "LANG_GERMAN, SUBLANG_DEFAULT"; CodePage = 1252 }
    "fr-FR" = @{ Comment = "French"; Language = "LANG_FRENCH, SUBLANG_DEFAULT"; CodePage = 1252 }
    "es-ES" = @{ Comment = "Spanish"; Language = "LANG_SPANISH, SUBLANG_DEFAULT"; CodePage = 1252 }
    "it-IT" = @{ Comment = "Italian"; Language = "LANG_ITALIAN, SUBLANG_DEFAULT"; CodePage = 1252 }
    "pl-PL" = @{ Comment = "Polish"; Language = "LANG_POLISH, SUBLANG_DEFAULT"; CodePage = 1250 }
    "cs-CZ" = @{ Comment = "Czech"; Language = "LANG_CZECH, SUBLANG_DEFAULT"; CodePage = 1250 }
    "bg-BG" = @{ Comment = "Bulgarian"; Language = "LANG_BULGARIAN, SUBLANG_DEFAULT"; CodePage = 1251 }
    "pt-PT" = @{ Comment = "Portuguese"; Language = "LANG_PORTUGUESE, SUBLANG_PORTUGUESE"; CodePage = 1252 }
    "nl-NL" = @{ Comment = "Dutch"; Language = "LANG_DUTCH, SUBLANG_DUTCH"; CodePage = 1252 }
    "en-US" = @{ Comment = "English (United States)"; Language = "LANG_ENGLISH, SUBLANG_ENGLISH_US"; CodePage = 1252 }
}

function ConvertTo-RcStringLiteral {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) { return "" }
    $value = $Text -replace "`r`n", "\r\n"
    $value = $value -replace "`r", "\r"
    $value = $value -replace "`n", "\n"
    $value = $value.Replace('"', '""')
    return $value
}

$catalog = Get-Content -Raw -LiteralPath $CatalogPath | ConvertFrom-Json
$entries = @(
    $catalog.strings.PSObject.Properties |
        Where-Object {
            [string]$_.Value.component -like "export-epub.*" -and
                [string]$_.Value.resourceId -match '^IDS_[A-Z0-9_]+$'
        } |
        Sort-Object { [string]$_.Value.resourceId }
)

if ($entries.Count -eq 0) {
    throw "В $CatalogPath не найдены ExportEPUB строки с resourceId."
}

$declaredLanguages = @($catalog.targetLanguages)
foreach ($language in $languageResources.Keys) {
    if ($declaredLanguages -notcontains $language) {
        throw "В targetLanguages отсутствует язык ExportEPUB: $language"
    }
}

$lines = [Collections.Generic.List[string]]::new()
$lines.Add("// Автоматически сгенерировано из localization/plugin-ui/catalog.json.")
$lines.Add("// Не редактируйте вручную: используйте tools/localization/update-export-epub-resource-strings.ps1.")
$lines.Add("")

foreach ($language in $languageResources.Keys) {
    $resource = $languageResources[$language]
    $lines.Add("/////////////////////////////////////////////////////////////////////////////")
    $lines.Add("// $($resource.Comment) ExportEPUB strings")
    $lines.Add("")
    $lines.Add("LANGUAGE $($resource.Language)")
    $lines.Add("#pragma code_page($($resource.CodePage))")
    $lines.Add("")
    $lines.Add("STRINGTABLE")
    $lines.Add("BEGIN")

    foreach ($entry in $entries) {
        $resourceId = [string]$entry.Value.resourceId
        $translation = $entry.Value.translations.PSObject.Properties[$language]
        if (-not $translation) {
            throw "У ключа $($entry.Name) отсутствует перевод для $language."
        }
        $text = ConvertTo-RcStringLiteral -Text ([string]$translation.Value)
        $lines.Add(("    {0} L""{1}""" -f $resourceId.PadRight(44), $text))
    }

    $lines.Add("END")
    $lines.Add("")
}

[IO.File]::WriteAllText($OutputPath, ($lines -join "`r`n"), [Text.UnicodeEncoding]::new($false, $true))
if (-not $Quiet) {
    Write-Host "Строковые ресурсы ExportEPUB обновлены из JSON-каталога."
    Write-Host "  Файл: $OutputPath"
    Write-Host "  Языков: $($languageResources.Count)"
    Write-Host "  Строк на язык: $($entries.Count)"
}
