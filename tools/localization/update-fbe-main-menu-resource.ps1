<#
.SYNOPSIS
Генерирует Win32 MENU-ресурс главного меню FBE из JSON-каталога локализации.

.DESCRIPTION
Скрипт читает `localization/app-ui/fbe-idr-mainframe-menu.json` и создаёт
`FBEIdrMainframeMenu.generated.rc2` для русской и украинской resource DLL.
Пока generated-файл не подключается автоматически вместо ручного `IDR_MAINFRAME`
в `FBE.rc`: это подготовительный шаг, который позволяет проверить структуру
меню, кодировку и полноту переводов до безопасной замены ручного MENU-блока.
#>
[CmdletBinding()]
param(
    [string[]]$Languages = @("ru-RU", "uk-UA")
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-idr-mainframe-menu.json"
$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 40

if ($catalog.resource -ne "IDR_MAINFRAME" -or $catalog.resourceType -ne "MENU") {
    throw "Каталог $catalogPath описывает не IDR_MAINFRAME MENU."
}

$outputByLanguage = @{
    "ru-RU" = Join-Path $repoRoot "src\locales\res_rus\FBEIdrMainframeMenu.generated.rc2"
    "uk-UA" = Join-Path $repoRoot "src\locales\res_ukr\FBEIdrMainframeMenu.generated.rc2"
}

$template = @(
    @{ Type = "POPUP"; Key = "popup.file"; Children = @(
        @{ Type = "MENUITEM"; Key = "file.new" },
        @{ Type = "MENUITEM"; Key = "file.open" },
        @{ Type = "MENUITEM"; Key = "file.save" },
        @{ Type = "MENUITEM"; Key = "file.save_as" },
        @{ Type = "MENUITEM"; Key = "file.validate" },
        @{ Type = "SEPARATOR" },
        @{ Type = "POPUP"; Key = "popup.import"; Children = @(
            @{ Type = "MENUITEM"; Key = "plugins.none.import" }
        ) },
        @{ Type = "POPUP"; Key = "popup.export"; Children = @(
            @{ Type = "MENUITEM"; Key = "plugins.none.export" }
        ) },
        @{ Type = "SEPARATOR" },
        @{ Type = "POPUP"; Key = "popup.recent_documents"; Children = @(
            @{ Type = "MENUITEM"; Key = "recent.empty"; Extra = "INACTIVE" }
        ) },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "file.exit" }
    ) },
    @{ Type = "POPUP"; Key = "popup.edit"; Children = @(
        @{ Type = "MENUITEM"; Key = "edit.undo" },
        @{ Type = "MENUITEM"; Key = "edit.redo" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "edit.cut" },
        @{ Type = "MENUITEM"; Key = "edit.copy" },
        @{ Type = "MENUITEM"; Key = "edit.paste" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "edit.find" },
        @{ Type = "MENUITEM"; Key = "edit.find_next" },
        @{ Type = "MENUITEM"; Key = "edit.replace" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "edit.goto_footnote" },
        @{ Type = "MENUITEM"; Key = "edit.goto_matching_tag" },
        @{ Type = "MENUITEM"; Key = "edit.goto_wrong_tag" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "edit.clone" },
        @{ Type = "MENUITEM"; Key = "edit.split" },
        @{ Type = "MENUITEM"; Key = "edit.merge" },
        @{ Type = "MENUITEM"; Key = "edit.remove_outer_section" }
    ) },
    @{ Type = "POPUP"; Key = "popup.view"; Children = @(
        @{ Type = "MENUITEM"; Key = "view.toolbar" },
        @{ Type = "MENUITEM"; Key = "view.scripts_bar" },
        @{ Type = "MENUITEM"; Key = "view.links_bar" },
        @{ Type = "MENUITEM"; Key = "view.tables_bar" },
        @{ Type = "MENUITEM"; Key = "view.status_bar" },
        @{ Type = "MENUITEM"; Key = "view.tree" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "view.description" },
        @{ Type = "MENUITEM"; Key = "view.body" },
        @{ Type = "MENUITEM"; Key = "view.source" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "view.fast_mode" }
    ) },
    @{ Type = "POPUP"; Key = "popup.insert"; Children = @(
        @{ Type = "MENUITEM"; Key = "insert.body" },
        @{ Type = "MENUITEM"; Key = "insert.title" },
        @{ Type = "MENUITEM"; Key = "insert.epigraph" },
        @{ Type = "MENUITEM"; Key = "insert.annotation" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "insert.text_author" },
        @{ Type = "MENUITEM"; Key = "insert.image" },
        @{ Type = "MENUITEM"; Key = "insert.inline_image" },
        @{ Type = "MENUITEM"; Key = "insert.poem" },
        @{ Type = "MENUITEM"; Key = "insert.cite" },
        @{ Type = "MENUITEM"; Key = "insert.table" },
        @{ Type = "MENUITEM"; Key = "insert.section_image" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "insert.binary" }
    ) },
    @{ Type = "POPUP"; Key = "popup.style"; Children = @(
        @{ Type = "MENUITEM"; Key = "style.normal" },
        @{ Type = "MENUITEM"; Key = "style.text_author" },
        @{ Type = "MENUITEM"; Key = "style.subtitle" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "style.link" },
        @{ Type = "MENUITEM"; Key = "style.note" },
        @{ Type = "MENUITEM"; Key = "style.remove_link" }
    ) },
    @{ Type = "POPUP"; Key = "popup.tools"; Children = @(
        @{ Type = "MENUITEM"; Key = "tools.words" },
        @{ Type = "MENUITEM"; Key = "tools.options" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "tools.spellcheck" },
        @{ Type = "SEPARATOR" },
        @{ Type = "MENUITEM"; Key = "tools.diagnostic_trace" }
    ) },
    @{ Type = "POPUP"; Key = "popup.scripts"; Children = @(
        @{ Type = "MENUITEM"; Key = "scripts.empty"; Extra = "INACTIVE" }
    ) },
    @{ Type = "POPUP"; Key = "popup.help"; Children = @(
        @{ Type = "MENUITEM"; Key = "help.about" }
    ) }
)

function ConvertTo-RcStringLiteral {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) { return "" }
    return $Text.Replace('"', '""')
}

function Get-MenuEntry {
    param([Parameter(Mandatory)][string]$Key)
    $fullKey = "fbe.menu.idr_mainframe.$Key"
    $property = $catalog.strings.PSObject.Properties[$fullKey]
    if (-not $property) {
        throw "В каталоге нет пункта главного меню: $fullKey"
    }
    return $property.Value
}

function Get-MenuText {
    param(
        [Parameter(Mandatory)][string]$Key,
        [Parameter(Mandatory)][string]$Language
    )
    $entry = Get-MenuEntry -Key $Key
    $translation = $entry.translations.PSObject.Properties[$Language]
    if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) {
        throw "У пункта $Key нет перевода для $Language."
    }
    return ConvertTo-RcStringLiteral ([string]$translation.Value)
}

function Add-MenuNode {
    param(
        [Parameter(Mandatory)]$Lines,
        [Parameter(Mandatory)]$Node,
        [Parameter(Mandatory)][string]$Language,
        [Parameter(Mandatory)][int]$Indent
    )

    $pad = " " * $Indent
    switch ($Node.Type) {
        "SEPARATOR" {
            $Lines.Add("${pad}MENUITEM SEPARATOR")
        }
        "MENUITEM" {
            $entry = Get-MenuEntry -Key $Node.Key
            $text = Get-MenuText -Key $Node.Key -Language $Language
            $targetId = [string]$entry.targetId
            $line = "${pad}MENUITEM `"$text`", $targetId"
            if (-not [string]::IsNullOrWhiteSpace($Node.Extra)) {
                $line += ", $($Node.Extra)"
            }
            $Lines.Add($line)
        }
        "POPUP" {
            $text = Get-MenuText -Key $Node.Key -Language $Language
            $Lines.Add("${pad}POPUP `"$text`"")
            $Lines.Add("${pad}BEGIN")
            foreach ($child in @($Node.Children)) {
                Add-MenuNode -Lines $Lines -Node $child -Language $Language -Indent ($Indent + 4)
            }
            $Lines.Add("${pad}END")
        }
        default {
            throw "Неизвестный тип узла меню: $($Node.Type)"
        }
    }
}

$utf16LeBom = [Text.UnicodeEncoding]::new($false, $true)
foreach ($language in $Languages) {
    if (-not $outputByLanguage.ContainsKey($language)) {
        throw "Для языка $language не задан путь generated MENU-ресурса."
    }

    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("// Автоматически сгенерировано из localization/app-ui/fbe-idr-mainframe-menu.json.")
    $lines.Add("// Не редактируйте вручную: используйте tools/localization/update-fbe-main-menu-resource.ps1.")
    $lines.Add("")
    $lines.Add("IDR_MAINFRAME MENU")
    $lines.Add("BEGIN")
    foreach ($node in $template) {
        Add-MenuNode -Lines $lines -Node $node -Language $language -Indent 4
    }
    $lines.Add("END")
    $lines.Add("")

    $outPath = $outputByLanguage[$language]
    [IO.File]::WriteAllText($outPath, ($lines -join "`r`n"), $utf16LeBom)
    Write-Host "Главное меню FBE сгенерировано."
    Write-Host "  Язык: $language"
    Write-Host "  Файл: $outPath"
}
