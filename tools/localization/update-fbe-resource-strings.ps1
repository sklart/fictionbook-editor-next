<#
.SYNOPSIS
Генерирует подготовительные строковые ресурсы FBE из JSON-каталога локализации.

.DESCRIPTION
Скрипт читает `localization/app-ui/catalog.json`, выбирает строки компонента
`fbe.core` с существующими `IDS_*` идентификаторами и создаёт отдельные
`FBEStrings.generated.rc2` для русской и украинской resource DLL. Эти файлы
подключаются к локализованным `FBE.rc`; ручные дубли перенесённых `STRINGTABLE`
удаляются helper-скриптом `connect-fbe-generated-resource-strings.ps1`.
#>
[CmdletBinding()]
param(
    [string]$CatalogPath,
    [string]$RussianOutputPath,
    [string]$UkrainianOutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($CatalogPath)) {
    $CatalogPath = Join-Path $repoRoot "localization\app-ui\catalog.json"
}
if ([string]::IsNullOrWhiteSpace($RussianOutputPath)) {
    $RussianOutputPath = Join-Path $repoRoot "src\locales\res_rus\FBEStrings.generated.rc2"
}
if ([string]::IsNullOrWhiteSpace($UkrainianOutputPath)) {
    $UkrainianOutputPath = Join-Path $repoRoot "src\locales\res_ukr\FBEStrings.generated.rc2"
}

$languageResources = [ordered]@{
    "ru-RU" = @{
        Comment = "Russian FBE strings"
        Language = "LANG_RUSSIAN, SUBLANG_DEFAULT"
        CodePage = 1251
        OutputPath = $RussianOutputPath
    }
    "uk-UA" = @{
        Comment = "Ukrainian FBE strings"
        Language = "LANG_UKRAINIAN, SUBLANG_DEFAULT"
        CodePage = 1251
        OutputPath = $UkrainianOutputPath
    }
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

$catalog = Get-Content -Raw -LiteralPath $CatalogPath | ConvertFrom-Json -Depth 30
$entries = @(
    $catalog.seedStrings.PSObject.Properties |
        Where-Object {
            [string]$_.Value.component -eq "fbe.core" -and
                [string]$_.Value.resourceId -match '^IDS_[A-Z0-9_]+$'
        } |
        Sort-Object { [string]$_.Value.resourceId }
)

if ($entries.Count -eq 0) {
    throw "В $CatalogPath не найдены FBE-строки с существующими IDS_* resourceId."
}

$declaredLanguages = @($catalog.targetLanguages)
foreach ($language in $languageResources.Keys) {
    if ($declaredLanguages -notcontains $language) {
        throw "В targetLanguages отсутствует язык FBE: $language"
    }
}

foreach ($language in $languageResources.Keys) {
    $resource = $languageResources[$language]
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("// Автоматически сгенерировано из localization/app-ui/catalog.json.")
    $lines.Add("// Не редактируйте вручную: используйте tools/localization/update-fbe-resource-strings.ps1.")
    $lines.Add("// Подключается из локализованных FBE.rc как generated STRINGTABLE.")
    $lines.Add("")
    $lines.Add("/////////////////////////////////////////////////////////////////////////////")
    $lines.Add("// $($resource.Comment)")
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
        $lines.Add(("    {0} L""{1}""" -f $resourceId.PadRight(30), $text))
    }

    $lines.Add("END")
    $lines.Add("")

    $outputDirectory = Split-Path -Parent $resource.OutputPath
    if (-not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    }

    [IO.File]::WriteAllText($resource.OutputPath, ($lines -join "`r`n"), [Text.UnicodeEncoding]::new($false, $true))
    Write-Host "Строковые ресурсы FBE обновлены из JSON-каталога."
    Write-Host "  Язык: $language"
    Write-Host "  Файл: $($resource.OutputPath)"
    Write-Host "  Строк: $($entries.Count)"
}
