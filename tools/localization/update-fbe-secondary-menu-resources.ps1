<#
.SYNOPSIS
Генерирует Win32 MENU-ресурсы малых меню FBE из JSON-каталога локализации.

.DESCRIPTION
Скрипт читает `localization/app-ui/fbe-secondary-menus.json` и создаёт
`FBESecondaryMenus.generated.rc2` для русской и украинской resource DLL.
В generated-файл входят `IDR_DOCUMENT_TREE MENU` и `IDR_TOOLBAR_MENU MENU`.
#>
[CmdletBinding()]
param(
    [string[]]$Languages = @("ru-RU", "uk-UA")
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-secondary-menus.json"
$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -Depth 40
$outputByLanguage = @{
    "ru-RU" = Join-Path $repoRoot "src\locales\res_rus\FBESecondaryMenus.generated.rc2"
    "uk-UA" = Join-Path $repoRoot "src\locales\res_ukr\FBESecondaryMenus.generated.rc2"
}

function ConvertTo-RcStringLiteral {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) { return "" }
    return $Text.Replace('"', '""')
}

function Get-Entry {
    param([Parameter(Mandatory)][string]$Key)
    $fullKey = "fbe.menu.$Key"
    $property = $catalog.strings.PSObject.Properties[$fullKey]
    if (-not $property) { throw "В каталоге нет пункта меню: $fullKey" }
    return $property.Value
}

function Add-MenuItem {
    param($Lines, [string]$Key, [string]$Language, [int]$Indent)
    $entry = Get-Entry -Key $Key
    $translation = $entry.translations.PSObject.Properties[$Language]
    if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) {
        throw "У пункта $Key нет перевода для $Language."
    }
    $pad = " " * $Indent
    $text = ConvertTo-RcStringLiteral ([string]$translation.Value)
    $Lines.Add("${pad}MENUITEM `"$text`", $($entry.targetId)")
}

$utf16LeBom = [Text.UnicodeEncoding]::new($false, $true)
foreach ($language in $Languages) {
    if (-not $outputByLanguage.ContainsKey($language)) { throw "Для языка $language не задан путь generated MENU-ресурса." }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("// Автоматически сгенерировано из localization/app-ui/fbe-secondary-menus.json.")
    $lines.Add("// Не редактируйте вручную: используйте tools/localization/update-fbe-secondary-menu-resources.ps1.")
    $lines.Add("")
    $lines.Add("IDR_DOCUMENT_TREE MENU")
    $lines.Add("BEGIN")
    $lines.Add("    POPUP `"DT`"")
    $lines.Add("    BEGIN")
    Add-MenuItem -Lines $lines -Key "idr_document_tree.view" -Language $language -Indent 8
    Add-MenuItem -Lines $lines -Key "idr_document_tree.view_source" -Language $language -Indent 8
    $lines.Add("        MENUITEM SEPARATOR")
    Add-MenuItem -Lines $lines -Key "idr_document_tree.move_right" -Language $language -Indent 8
    Add-MenuItem -Lines $lines -Key "idr_document_tree.make_child" -Language $language -Indent 8
    Add-MenuItem -Lines $lines -Key "idr_document_tree.move_left" -Language $language -Indent 8
    $lines.Add("        MENUITEM SEPARATOR")
    Add-MenuItem -Lines $lines -Key "idr_document_tree.delete" -Language $language -Indent 8
    $lines.Add("    END")
    $lines.Add("END")
    $lines.Add("")
    $lines.Add("IDR_TOOLBAR_MENU MENU")
    $lines.Add("BEGIN")
    $lines.Add("    POPUP `"CUSTOMIZE`"")
    $lines.Add("    BEGIN")
    Add-MenuItem -Lines $lines -Key "idr_toolbar_menu.customize" -Language $language -Indent 8
    $lines.Add("    END")
    $lines.Add("END")
    $lines.Add("")

    $outPath = $outputByLanguage[$language]
    [IO.File]::WriteAllText($outPath, ($lines -join "`r`n"), $utf16LeBom)
    Write-Host "Малые меню FBE сгенерированы."
    Write-Host "  Язык: $language"
    Write-Host "  Файл: $outPath"
}