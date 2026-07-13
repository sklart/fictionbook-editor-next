<#
.SYNOPSIS
Генерирует подготовительные Win32 resource-фрагменты из JSON-каталогов локализации.

.DESCRIPTION
Скрипт читает `localization/app-ui/catalog.json` и
`localization/plugin-ui/catalog.json`, создаёт общий заголовок с
детерминированными `IDS_L10N_*` идентификаторами и отдельные `.rc2` файлы
`STRINGTABLE` для каждого целевого языка. Эти файлы пока не подключаются к
основным `.rc`: они нужны как проверяемый мост от Weblate-friendly JSON к
будущей генерации Win32-ресурсов.
#>
[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "out\localization\win32-resource-fragments"
}

$catalogs = @(
    @{
        Scope = "app-ui"
        Prefix = "IDS_L10N_APP"
        Path = Join-Path $repoRoot "localization\app-ui\catalog.json"
        StringsProperty = "seedStrings"
        FirstId = 70000
    },
    @{
        Scope = "plugin-ui"
        Prefix = "IDS_L10N_PLUGIN"
        Path = Join-Path $repoRoot "localization\plugin-ui\catalog.json"
        StringsProperty = "strings"
        FirstId = 72000
    }
)

function ConvertTo-ResourceSymbol {
    param(
        [Parameter(Mandatory)]
        [string]$Prefix,

        [Parameter(Mandatory)]
        [string]$Key
    )

    $suffix = ($Key.ToUpperInvariant() -replace '[^A-Z0-9]+', '_').Trim('_')
    if ([string]::IsNullOrWhiteSpace($suffix)) {
        throw "Не удалось построить resource-symbol для ключа: $Key"
    }

    return "$Prefix`_$suffix"
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

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$utf8NoBom = [Text.UTF8Encoding]::new($false)
$utf16LeBom = [Text.UnicodeEncoding]::new($false, $true)
$headerLines = [Collections.Generic.List[string]]::new()
$manifestItems = [Collections.Generic.List[object]]::new()
$allLanguages = $null
$seenSymbols = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)

$headerLines.Add("// Автоматически сгенерировано из localization/*/catalog.json.")
$headerLines.Add("// Не редактируйте вручную: используйте tools/localization/export-win32-resource-fragments.ps1.")
$headerLines.Add("#pragma once")
$headerLines.Add("")

foreach ($catalogInfo in $catalogs) {
    $catalog = Get-Content -Raw -LiteralPath $catalogInfo.Path | ConvertFrom-Json -Depth 30
    $languages = @($catalog.targetLanguages)
    if ($null -eq $allLanguages) {
        $allLanguages = $languages
    } elseif ((Compare-Object -ReferenceObject $allLanguages -DifferenceObject $languages).Count -ne 0) {
        throw "Набор языков $($catalogInfo.Scope) отличается от предыдущих каталогов."
    }

    $stringsNode = $catalog.PSObject.Properties[$catalogInfo.StringsProperty]
    if (-not $stringsNode) {
        throw "В каталоге $($catalogInfo.Path) нет блока $($catalogInfo.StringsProperty)."
    }

    $entries = @($stringsNode.Value.PSObject.Properties)
    $id = [int]$catalogInfo.FirstId
    $symbolMap = [ordered]@{}

    $headerLines.Add("// $($catalogInfo.Scope)")
    foreach ($entry in $entries) {
        $symbol = ConvertTo-ResourceSymbol -Prefix $catalogInfo.Prefix -Key $entry.Name
        if (-not $seenSymbols.Add($symbol)) {
            throw "Дублирующийся resource-symbol: $symbol"
        }

        $symbolMap[$entry.Name] = [ordered]@{
            symbol = $symbol
            id = $id
        }
        $headerLines.Add(("#define {0} {1}" -f $symbol.PadRight(56), $id))
        $id++
    }
    $headerLines.Add("")

    foreach ($language in $languages) {
        $rcLines = [Collections.Generic.List[string]]::new()
        $rcLines.Add("// Автоматически сгенерировано из $($catalogInfo.Scope) для $language.")
        $rcLines.Add("// Не редактируйте вручную.")
        $rcLines.Add('#include "l10n_resource_ids.h"')
        $rcLines.Add("")
        $rcLines.Add("STRINGTABLE")
        $rcLines.Add("BEGIN")

        foreach ($entry in $entries) {
            $translation = $entry.Value.translations.PSObject.Properties[$language]
            if (-not $translation) {
                throw "У ключа $($entry.Name) отсутствует перевод для $language."
            }

            $text = ConvertTo-RcStringLiteral -Text ([string]$translation.Value)
            $symbol = $symbolMap[$entry.Name].symbol
            $rcLines.Add(("    {0} L""{1}""" -f $symbol.PadRight(56), $text))
        }

        $rcLines.Add("END")
        $rcLines.Add("")

        $rcPath = Join-Path $OutputDirectory "$($catalogInfo.Scope).$language.rc2"
        [IO.File]::WriteAllText($rcPath, ($rcLines -join "`r`n"), $utf16LeBom)
    }

    $manifestItems.Add([ordered]@{
        scope = $catalogInfo.Scope
        source = (Resolve-Path -LiteralPath $catalogInfo.Path).Path.Replace($repoRoot + "\", "")
        strings = $entries.Count
        firstId = [int]$catalogInfo.FirstId
        lastId = [int]$catalogInfo.FirstId + $entries.Count - 1
    })
}

$headerPath = Join-Path $OutputDirectory "l10n_resource_ids.h"
[IO.File]::WriteAllText($headerPath, ($headerLines -join "`r`n"), $utf8NoBom)

$manifest = [ordered]@{
    generatedAt = (Get-Date).ToString("s")
    outputKind = "win32-resource-fragments"
    languages = $allLanguages
    header = "l10n_resource_ids.h"
    catalogs = $manifestItems
    note = "Подготовительные .rc2-фрагменты. Пока не подключены к основным .rc."
}
$manifestPath = Join-Path $OutputDirectory "manifest.json"
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 10) + "`n", $utf8NoBom)

Write-Host "Win32 resource-фрагменты локализации подготовлены."
Write-Host "  Каталог: $OutputDirectory"
Write-Host "  Языков: $($allLanguages.Count)"
Write-Host "  Заголовок: $headerPath"
