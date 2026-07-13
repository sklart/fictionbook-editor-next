<#
.SYNOPSIS
Обновляет встроенные строковые ресурсы ExportHTML из JSON-каталога локализации.

.DESCRIPTION
Скрипт читает `localization/plugin-ui/catalog.json`, выбирает строки компонентов
`export-html.*` с существующими `IDR_*`/`IDS_*` идентификаторами и генерирует
`src/export-html/ExportHTMLStrings.generated.rc2`. Файл подключается из
`ExportHTML.rc`, поэтому runtime-сообщения и всплывающие подсказки плагина
получают встроенный fallback из того же JSON-источника, который позже можно
отдавать в Weblate.
#>
[CmdletBinding()]
param(
    [string]$CatalogPath,
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($CatalogPath)) {
    $CatalogPath = Join-Path $repoRoot "localization\plugin-ui\catalog.json"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "src\export-html\ExportHTMLStrings.generated.rc2"
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
    param(
        [AllowNull()]
        [string]$Text
    )

    if ($null -eq $Text) {
        return ""
    }

    $value = $Text -replace "`r`n", "\r\n"
    $value = $value -replace "`r", "\r"
    $value = $value -replace "`n", "\n"
    $value = $value.Replace('"', '""')
    return $value
}

$catalog = Get-Content -Raw -LiteralPath $CatalogPath | ConvertFrom-Json -Depth 50
$entries = @(
    $catalog.strings.PSObject.Properties |
        Where-Object {
            [string]$_.Value.component -like "export-html.*" -and
                [string]$_.Value.resourceId -match '^(IDR|IDS)_[A-Z0-9_]+$'
        } |
        Sort-Object { [string]$_.Value.resourceId }
)

if ($entries.Count -eq 0) {
    throw "В $CatalogPath не найдены ExportHTML runtime-строки с resourceId."
}

$declaredLanguages = @($catalog.targetLanguages)
foreach ($language in $languageResources.Keys) {
    if ($declaredLanguages -notcontains $language) {
        throw "В targetLanguages отсутствует язык ExportHTML: $language"
    }
}

$lines = [Collections.Generic.List[string]]::new()
$lines.Add("// Автоматически сгенерировано из localization/plugin-ui/catalog.json.")
$lines.Add("// Не редактируйте вручную: используйте tools/localization/update-export-html-resource-strings.ps1.")
$lines.Add("")

foreach ($language in $languageResources.Keys) {
    $resource = $languageResources[$language]
    $lines.Add("/////////////////////////////////////////////////////////////////////////////")
    $lines.Add("// $($resource.Comment) ExportHTML strings")
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
        $lines.Add(("    {0} L""{1}""" -f $resourceId.PadRight(36), $text))
    }

    $lines.Add("END")
    $lines.Add("")
}

[IO.File]::WriteAllText($OutputPath, ($lines -join "`r`n"), [Text.UnicodeEncoding]::new($false, $true))
Write-Host "Строковые ресурсы ExportHTML обновлены из JSON-каталога."
Write-Host "  Файл: $OutputPath"
Write-Host "  Языков: $($languageResources.Count)"
Write-Host "  Строк на язык: $($entries.Count)"
