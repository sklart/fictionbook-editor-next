<#
.SYNOPSIS
Проверяет JSON-каталог главного меню FBE IDR_MAINFRAME.

.DESCRIPTION
Тест страхует подготовительный Weblate-friendly каталог меню FBE: проверяет набор
языков, наличие переводов для каждого пункта, непустые строки и сохранение
клавиатурного акселератора `&` хотя бы в базовых языках ru-RU/uk-UA/en-US.
Каталог пока не подключён к runtime, но нужен как следующий слой миграции меню
из Win32 .rc в JSON/generation pipeline.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$catalogPath = Join-Path $repoRoot "localization\app-ui\fbe-idr-mainframe-menu.json"
if (-not (Test-Path -LiteralPath $catalogPath)) {
    throw "Каталог главного меню FBE не найден: $catalogPath"
}

$catalog = Get-Content -Raw -LiteralPath $catalogPath -Encoding UTF8 | ConvertFrom-Json
$expectedLanguages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
$languages = @($catalog.targetLanguages)
if ((Compare-Object -ReferenceObject $expectedLanguages -DifferenceObject $languages).Count -ne 0) {
    throw "Набор языков каталога главного меню FBE не совпадает с ожидаемым."
}

if ($catalog.resource -ne 'IDR_MAINFRAME' -or $catalog.resourceType -ne 'MENU') {
    throw "Каталог описывает не IDR_MAINFRAME MENU."
}

$entries = @($catalog.strings.PSObject.Properties)
if ($entries.Count -lt 1) {
    throw "Каталог главного меню FBE не содержит строк."
}

foreach ($entry in $entries) {
    foreach ($language in $expectedLanguages) {
        $translation = $entry.Value.translations.PSObject.Properties[$language]
        if (-not $translation -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) {
            throw "У пункта $($entry.Name) нет перевода для $language."
        }
    }

    if ($entry.Value.kind -eq 'POPUP') {
        foreach ($language in @('en-US','ru-RU','uk-UA')) {
            $value = [string]$entry.Value.translations.PSObject.Properties[$language].Value
            if ($value -notmatch '&') {
                throw "У POPUP-пункта $($entry.Name) в $language потерян menu accelerator &: $value"
            }
        }
    }
}

$privacyPhrases = @{
    'en-US' = 'Book text, XML, HTML, Base64, and user-script contents are not recorded; paths are redacted.'
    'ru-RU' = 'Текст книги, XML, HTML, Base64 и содержимое пользовательских сценариев не записываются; пути обезличиваются.'
}
foreach ($language in $privacyPhrases.Keys) {
    $value = [string]$catalog.strings.'fbe.trace.enable.question'.translations.PSObject.Properties[$language].Value
    if ($value -notlike ('*' + $privacyPhrases[$language] + '*')) {
        throw "В предупреждении о диагностической трассировке нет обязательного privacy-текста для $language."
    }
}

foreach ($language in $expectedLanguages) {
    $value = [string]$catalog.strings.'fbe.trace.enable.question'.translations.PSObject.Properties[$language].Value
    foreach ($technicalToken in @('XML', 'HTML', 'Base64')) {
        if ($value -notlike ('*' + $technicalToken + '*')) {
            throw "В privacy-предупреждении для $language отсутствует $technicalToken."
        }
    }
    $completed = [string]$catalog.strings.'fbe.trace.enable.completed'.translations.PSObject.Properties[$language].Value
    if ($completed -match '(?i)diagnostic log') {
        throw "В локализованном сообщении о включении осталась англоязычная вставка diagnostic log: $language"
    }
}
foreach ($requiredKey in @(
    'fbe.menu.idr_mainframe.popup.file',
    'fbe.menu.idr_mainframe.file.open',
    'fbe.menu.idr_mainframe.popup.edit',
    'fbe.menu.idr_mainframe.popup.view',
    'fbe.menu.idr_mainframe.popup.insert',
    'fbe.menu.idr_mainframe.table.make_header_cells',
    'fbe.menu.idr_mainframe.table.make_normal_cells',
    'fbe.menu.idr_mainframe.popup.style',
    'fbe.menu.idr_mainframe.popup.tools',
    'fbe.menu.idr_mainframe.popup.diagnostics',
    'fbe.menu.idr_mainframe.tools.diagnostic_trace',
    'fbe.menu.idr_mainframe.tools.open_diagnostic_log',
    'fbe.menu.idr_mainframe.tools.open_diagnostic_folder',
    'fbe.menu.idr_mainframe.tools.copy_diagnostic_log_path',
    'fbe.menu.idr_mainframe.tools.clear_diagnostic_logs',    'fbe.trace.caption',
    'fbe.trace.enable.question',
    'fbe.trace.enable.completed',
    'fbe.trace.disable.completed',
    'fbe.trace.warning',
    'fbe.trace.already_enabled',
    'fbe.trace.title_suffix',
    'fbe.trace.change_failed',
    'fbe.trace.open_failed',
    'fbe.menu.idr_mainframe.popup.help'
)) {
    if (-not $catalog.strings.PSObject.Properties[$requiredKey]) {
        throw "В каталоге нет обязательного пункта: $requiredKey"
    }
}

$resourceText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$resourceIds = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\resource.h')
$frameSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($placeholder in @(
    @{ Key = 'fbe.menu.idr_mainframe.plugins.none.import'; Id = 'ID_FILE_IMPORT_PLACEHOLDER' },
    @{ Key = 'fbe.menu.idr_mainframe.plugins.none.export'; Id = 'ID_FILE_EXPORT_PLACEHOLDER' }
)) {
    $entry = $catalog.strings.PSObject.Properties[$placeholder.Key]
    if(-not $entry -or $entry.Value.targetId -ne $placeholder.Id) { throw "Неверный targetId для $($placeholder.Key)." }
    if($resourceText -notmatch ('MENUITEM\s+"[^"]+",\s*{0},\s*INACTIVE' -f [regex]::Escape($placeholder.Id)) -or
       $resourceIds -notmatch "(?m)^#define\s+$($placeholder.Id)\s+\d+" -or
       $frameSource -notmatch ([regex]::Escape("GetMenuItemID(importMenu, 0) == ID_FILE_IMPORT_PLACEHOLDER")) -or
       $frameSource -notmatch ([regex]::Escape("GetMenuItemID(exportMenu, 0) == ID_FILE_EXPORT_PLACEHOLDER"))) {
        throw "Заглушка $($placeholder.Id) не согласована с ресурсами и runtime-локализацией."
    }
}
$pluginUi = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\plugins\PluginUiController.cpp')
if($pluginUi -notmatch 'if\(list\.GetSize\(\) > 0\) ::RemoveMenu\(menu, 0, MF_BYPOSITION\)') {
    throw 'Инициализация плагинов должна удалять заглушку только при наличии команд.'
}
$commandIds = @{}
foreach($name in @('ID_FILE_RESTART', 'ID_FILE_IMPORT_PLACEHOLDER', 'ID_FILE_EXPORT_PLACEHOLDER')) {
    $match = [regex]::Match($resourceIds, "(?m)^#define\s+$name\s+(\d+)")
    if(-not $match.Success) { throw "Нет служебного ID меню: $name" }
    $value = [int]$match.Groups[1].Value
    if($value -ge 57600 -and $value -lt 57856) { throw "$name пересекается со стандартным диапазоном WTL 0xE100-0xE1FF." }
    if($commandIds.ContainsKey($value)) { throw "Служебные ID меню совпадают: $name." }
    $commandIds[$value] = $name
}
$helpDiagnostics = '(?s)POPUP "&Help"\s+BEGIN.*?POPUP "&Diagnostics"\s+BEGIN.*?ID_TOOLS_DIAGNOSTIC_TRACE.*?ID_TOOLS_OPEN_DIAGNOSTIC_LOG.*?ID_TOOLS_OPEN_DIAGNOSTIC_FOLDER.*?ID_TOOLS_COPY_DIAGNOSTIC_LOG_PATH.*?ID_TOOLS_CLEAR_DIAGNOSTIC_LOGS.*?END.*?END'
if ($resourceText -notmatch $helpDiagnostics) {
    throw 'Пять команд диагностики должны быть в подменю Справка → Диагностика.'
}
if ([regex]::Matches($resourceText, 'ID_TOOLS_(DIAGNOSTIC_TRACE|OPEN_DIAGNOSTIC_LOG|OPEN_DIAGNOSTIC_FOLDER|COPY_DIAGNOSTIC_LOG_PATH|CLEAR_DIAGNOSTIC_LOGS)').Count -ne 5) {
    throw 'Каждая команда диагностики должна присутствовать в главном меню ровно один раз.'
}
foreach($required in @('static bool MenuContainsScriptCommand(HMENU menu)', 'commandId >= ID_SCRIPT_BASE && commandId < ID_SCRIPT_BASE + SCRIPT_COMMAND_COUNT', 'GetSubMenu(mainMenu, 6)', 'ApplyRuntimeMainFrameMenuLocalization(mainMenu);', 'FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.undo", L"&Undo")', 'FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.edit.redo", L"&Redo")')) {
    if($frameSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Runtime main-menu localization is missing: $required" }
}
$tableRuntimeBindings = @(
    @{ Command = 'ID_TABLE_INSERT_ROW_ABOVE'; Key = 'fbe.menu.idr_mainframe.table.insert_row_above' },
    @{ Command = 'ID_TABLE_INSERT_ROW_BELOW'; Key = 'fbe.menu.idr_mainframe.table.insert_row_below' },
    @{ Command = 'ID_TABLE_DELETE_ROW'; Key = 'fbe.menu.idr_mainframe.table.delete_row' },
    @{ Command = 'ID_TABLE_INSERT_COLUMN_LEFT'; Key = 'fbe.menu.idr_mainframe.table.insert_column_left' },
    @{ Command = 'ID_TABLE_INSERT_COLUMN_RIGHT'; Key = 'fbe.menu.idr_mainframe.table.insert_column_right' },
    @{ Command = 'ID_TABLE_DELETE_COLUMN'; Key = 'fbe.menu.idr_mainframe.table.delete_column' },
    @{ Command = 'ID_TABLE_MAKE_HEADER_CELLS'; Key = 'fbe.menu.idr_mainframe.table.make_header_cells' },
    @{ Command = 'ID_TABLE_MAKE_NORMAL_CELLS'; Key = 'fbe.menu.idr_mainframe.table.make_normal_cells' }
)
foreach($binding in $tableRuntimeBindings) {
    $expectedBinding = '{{ {0}, L"{1}" }}' -f $binding.Command, $binding.Key
    if($frameSource -notmatch ([regex]::Escape($expectedBinding))) { throw "Table command is not runtime-localized: $($binding.Command)" }
}
if($frameSource -notmatch 'FindTopLevelMenuPositionByCommand\(insertMenu, ID_TABLE_INSERT_ROW_ABOVE\)' -or $frameSource -notmatch 'fbe\.menu\.idr_mainframe\.table\.structure') {
    throw 'Table structure popup is not runtime-localized.'
}
if($frameSource.IndexOf('::EnableMenuItem(scripts, ID_SCRIPT_BASE', [StringComparison]::Ordinal) -ge 0) {
    throw 'Idle processing must not mutate script popup items while the command bar can be tracking the menu.'
}
Write-Host "Каталог главного меню FBE прошёл проверку."
Write-Host "  Файл: $catalogPath"
Write-Host "  Пунктов: $($entries.Count)"
Write-Host "  Языков: $($languages.Count)"
