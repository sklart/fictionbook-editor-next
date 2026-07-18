# Проверяет экспорт будущих runtime JSON-файлов локализации Lang/<язык>/<модуль>.json.
[CmdletBinding()]
param(
    [string] $RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fbe-runtime-lang-export-" + $PID)
$langRoot = Join-Path $tempRoot 'Lang'

try {
    & (Join-Path $RepositoryRoot 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $RepositoryRoot -OutputDirectory $langRoot -Clean

    $contract = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'localization\runtime\contract.json') -Encoding UTF8 | ConvertFrom-Json -AsHashtable
    $appCatalog = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'localization\app-ui\catalog.json') -Encoding UTF8 | ConvertFrom-Json -AsHashtable
    $pluginCatalog = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'localization\plugin-ui\catalog.json') -Encoding UTF8 | ConvertFrom-Json -AsHashtable

    $fallbackLanguage = [string] $contract['fallbackLanguage']
    $moduleFiles = @{}
foreach ($moduleEntry in @($contract['modules'])) {
    $moduleFiles[[string] $moduleEntry['module']] = [string] $moduleEntry['file']
}
$modules = @($moduleFiles.Keys | Sort-Object)
    $languages = @($appCatalog['targetLanguages'])
    foreach ($language in @($pluginCatalog['targetLanguages'])) {
        if ($languages -notcontains $language) {
            $languages += $language
        }
    }
    $languages = @($languages | Sort-Object)

    foreach ($language in $languages) {
        foreach ($module in $modules) {
            $path = Join-Path (Join-Path $langRoot $language) $moduleFiles[$module]
            if (-not (Test-Path -LiteralPath $path)) {
                throw "Не создан runtime JSON: $path"
            }
            $json = Get-Content -Raw -LiteralPath $path -Encoding UTF8 | ConvertFrom-Json -AsHashtable
            if ([int] $json['formatVersion'] -ne 1) {
                throw "Некорректная версия формата в $path"
            }
            if ([string] $json['module'] -ne $module) {
                throw "Некорректный module в $path"
            }
            if ([string] $json['locale'] -ne $language) {
                throw "Некорректный locale в $path"
            }
            if ([string] $json['fallbackLocale'] -ne $fallbackLanguage) {
                throw "Некорректный fallbackLocale в $path"
            }
            if (-not ($json['strings'] -is [hashtable])) {
                throw "В $path отсутствует объект strings."
            }
        }
    }

    $checks = @(
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.about.caption' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.settings.next.caption'; Expected = 'Настройки FBE Next' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.dialog.idd_setting_next.saving'; Expected = 'Сохранение' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.dialog.idd_setting_next.show_full_path_in_window_title'; Expected = 'Показывать полный путь к файлу в заголовке окна' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.dialog.idd_setting_next.source_code'; Expected = 'Исходный код' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.dialog.idd_setting_next.source_palette'; Expected = 'Цветовая схема:' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'fbe.json'; Key = 'fbe.dialog.idd_setting_next.source_palette.classic'; Expected = 'Классическая' },
        @{ Path = Join-Path (Join-Path $langRoot 'en-US') 'fbv.json'; Key = 'fbv.validation.no_errors' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.menu.idr_mainframe.popup.file'; Expected = '&Datei' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.menu.idr_document_tree.view'; Expected = '&Zum Element gehen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.menu.idr_toolbar_menu.customize'; Expected = 'Symbolleiste anpassen...' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_options.wrap_lines'; Expected = 'Zeilen umbrechen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_setting_other.restore_position'; Expected = 'Fensterposition wiederherstellen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_settings_words.select_all'; Expected = 'Alle auswählen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_hotkeys.assign'; Expected = 'Zuweisen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_find.caption'; Expected = 'Suchen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_replace.caption'; Expected = 'Ersetzen' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'fbe.json'; Key = 'fbe.dialog.idd_spell_check.ignore_all'; Expected = 'Alle ignorieren' },
        @{ Path = Join-Path (Join-Path $langRoot 'ru-RU') 'export-epub.json'; Key = 'export_epub.content.navigation_title' },
        @{ Path = Join-Path (Join-Path $langRoot 'uk-UA') 'import-epub.json'; Key = 'import_epub.options.title' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'export-docx.json'; Key = 'export_docx.dialog.settings.export_cover'; Expected = 'Cover exportieren' },
        @{ Path = Join-Path (Join-Path $langRoot 'fr-FR') 'export-docx.json'; Key = 'export_docx.dialog.file_options.settings_button'; Expected = "Paramètres d’export DOCX..." },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'export-docx.json'; Key = 'export_docx.dialog.settings.doc_language'; Expected = 'Dokumentsprache:' },
        @{ Path = Join-Path (Join-Path $langRoot 'pl-PL') 'export-docx.json'; Key = 'export_docx.dialog.settings.font_points'; Expected = 'pkt' },
        @{ Path = Join-Path (Join-Path $langRoot 'de-DE') 'export-docx.json'; Key = 'common.button.defaults' }
    )

    foreach ($check in $checks) {
        $json = Get-Content -Raw -LiteralPath $check.Path -Encoding UTF8 | ConvertFrom-Json -AsHashtable
        $strings = $json['strings']
        if (-not $strings.ContainsKey($check.Key)) {
            throw "В $($check.Path) отсутствует ключ $($check.Key)."
        }
        if ([string]::IsNullOrWhiteSpace([string] $strings[$check.Key])) {
            throw "В $($check.Path) ключ $($check.Key) пустой."
        }
        if ($check.ContainsKey('Expected') -and [string] $strings[$check.Key] -ne [string] $check.Expected) {
            throw "В $($check.Path) ключ $($check.Key) имеет значение '$($strings[$check.Key])', ожидалось '$($check.Expected)'."
        }
    }

    Write-Host "Экспорт runtime JSON-локализации прошёл проверку."
    Write-Host "  Каталог: $langRoot"
    Write-Host "  Языков: $($languages.Count)"
    Write-Host "  Модулей: $($modules.Count)"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

