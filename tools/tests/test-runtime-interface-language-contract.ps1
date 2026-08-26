# Проверяет, что runtime JSON-локализация использует общий выбранный язык интерфейса FBE.
# FBE Next публикует выбранный язык в FBE_NEXT_UI_LOCALE и interface-locale.txt, а FBV и плагины
# читают этот контракт перед fallback на язык Windows и en-US.
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$publisherChecks = @(
    @{ Path = "src\fbe\Settings.h"; Pattern = "GetInterfaceLocaleName"; Description = "CSettings объявляет locale-name для runtime JSON" },
    @{ Path = "src\fbe\Settings.cpp"; Pattern = 'return L"ru-RU"'; Description = "CSettings сопоставляет русский интерфейс с ru-RU" },
    @{ Path = "src\fbe\Settings.cpp"; Pattern = 'return L"uk-UA"'; Description = "CSettings сопоставляет украинский интерфейс с uk-UA" },
    @{ Path = "src\fbe\Settings.cpp"; Pattern = 'return L"en-US"'; Description = "CSettings сопоставляет английский/fallback интерфейс с en-US" },
    @{ Path = "src\fbe\RuntimeLocalization.h"; Pattern = "FbePublishRuntimeLocaleName"; Description = "FBE runtime-layer объявляет публикацию выбранного языка" },
    @{ Path = "src\fbe\RuntimeLocalization.h"; Pattern = "FbeIsRuntimeLocaleInstalled"; Description = "FBE runtime-layer объявляет проверку установленного языкового пакета" },
    @{ Path = "src\fbe\RuntimeLocalization.h"; Pattern = "FbeResetRuntimeLocalization"; Description = "FBE runtime-layer объявляет сброс runtime JSON-cache" },
    @{ Path = "src\fbe\FBE.cpp"; Pattern = "FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName())"; Description = "FBE публикует выбранный язык при старте" }
)

foreach ($check in $publisherChecks) {
    $path = Join-Path $repoRoot $check.Path
    $text = Get-Content -Raw -LiteralPath $path
    if ($text -notlike "*$($check.Pattern)*") {
        throw "Не выполнен контракт runtime-локализации: $($check.Description). Файл: $path"
    }
}


$requiredInterfaceLanguages = @(
    @{ Locale = "en-US"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_ENGLISH"; ResourceId = "IDS_LANG_ENGLISH"; CatalogKey = "fbe.language.english" },
    @{ Locale = "ru-RU"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_RUSSIAN"; ResourceId = "IDS_LANG_RUSSIAN"; CatalogKey = "fbe.language.russian" },
    @{ Locale = "uk-UA"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_UKRAINIAN"; ResourceId = "IDS_LANG_UKRAINIAN"; CatalogKey = "fbe.language.ukrainian" },
    @{ Locale = "de-DE"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_GERMAN"; ResourceId = "IDS_LANG_GERMAN"; CatalogKey = "fbe.language.german" },
    @{ Locale = "fr-FR"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_FRENCH"; ResourceId = "IDS_LANG_FRENCH"; CatalogKey = "fbe.language.french" },
    @{ Locale = "es-ES"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_SPANISH"; ResourceId = "IDS_LANG_SPANISH"; CatalogKey = "fbe.language.spanish" },
    @{ Locale = "it-IT"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_ITALIAN"; ResourceId = "IDS_LANG_ITALIAN"; CatalogKey = "fbe.language.italian" },
    @{ Locale = "pl-PL"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_POLISH"; ResourceId = "IDS_LANG_POLISH"; CatalogKey = "fbe.language.polish" },
    @{ Locale = "pt-PT"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_PORTUGUESE"; ResourceId = "IDS_LANG_PORTUGUESE"; CatalogKey = "fbe.language.portuguese" },
    @{ Locale = "nl-NL"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_DUTCH"; ResourceId = "IDS_LANG_DUTCH"; CatalogKey = "fbe.language.dutch" },
    @{ Locale = "cs-CZ"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_CZECH"; ResourceId = "IDS_LANG_CZECH"; CatalogKey = "fbe.language.czech" },
    @{ Locale = "bg-BG"; LanguageSymbol = "FBE_INTERFACE_LANGUAGE_BULGARIAN"; ResourceId = "IDS_LANG_BULGARIAN"; CatalogKey = "fbe.language.bulgarian" }
)

$settingsText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\Settings.cpp")
$settingsHeaderText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\Settings.h")
$optDlgText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\OptDlg.cpp")
$sharedRuntimeHelperPath = Join-Path $repoRoot "src\common\RuntimeLocalizationCommon.h"
$sharedRuntimeHelperText = Get-Content -Raw -LiteralPath $sharedRuntimeHelperPath
$appCatalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "localization\app-ui\catalog.json") | ConvertFrom-Json -Depth 30
$appCatalogKeys = @($appCatalog.seedStrings.PSObject.Properties.Name)

if ($settingsHeaderText -notlike "*FBE_INTERFACE_LANGUAGE_AUTO*") {
    throw "В Settings.h отсутствует специальное значение FBE_INTERFACE_LANGUAGE_AUTO для режима 'Определяется системой'."
}
if ($optDlgText -notlike "*FBE_INTERFACE_LANGUAGE_AUTO, IDS_LANG_SYSTEM_DEFAULT*") {
    throw "В списке языков настроек FBE отсутствует пункт 'Определяется системой'."
}
if ("fbe.language.system_default" -notin $appCatalogKeys) {
    throw "В app-ui catalog отсутствует ключ fbe.language.system_default."
}
if ($optDlgText -notlike "*FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName())*") {
    throw "OptDlg.cpp не публикует новую runtime-локаль после смены языка интерфейса."
}
if ($optDlgText -notlike "*FbeResetRuntimeLocalization()*") {
    throw "OptDlg.cpp не сбрасывает FBE runtime JSON-cache после смены языка интерфейса."
}
if ($optDlgText -notmatch 'FbeIsRuntimeLocaleInstalled\s*\(\s*kInterfaceLanguages\[i\]\.localeName\s*\)') {
    throw "OptDlg.cpp должен скрывать языки, для которых отсутствует Lang/<locale>/fbe.json."
}
if ($sharedRuntimeHelperText -notlike "*RuntimeStringFileExists*") {
    throw "RuntimeLocalizationCommon.h должен проверять наличие JSON-файла выбранного языкового пакета."
}
if ($optDlgText -like "*new_lang != _Settings.GetInterfaceLanguageID()*SetNeedRestart()*") {
    throw "Смена только языка интерфейса не должна требовать перезапуска: главное окно и новые диалоги обновляются runtime-слоем."
}

$buildScriptText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\build\build.ps1")
if ($buildScriptText -notlike "*export-runtime-lang.ps1*" -or $buildScriptText -notlike "*Export-RuntimeLanguageFiles -OutputDirectory*") {
    throw "build.ps1 должен экспортировать Lang рядом с out\\<Configuration>, иначе ручной запуск out\\Release\\FBE.exe не видит runtime JSON-локализацию."
}

$fbeRcText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\FBE.rc")
if ($fbeRcText -match 'IDC_LANG[^\r\n]*CBS_SORT') {
    throw "ComboBox выбора языка не должен использовать CBS_SORT: пункт 'Определяется системой' обязан оставаться первым."
}
if ($optDlgText -notmatch "m_lang\.SetDroppedWidth\((3[0-9]{2}|[4-9][0-9]{2,})\)") {
    throw "OptDlg.cpp должен расширять выпадающий список языка минимум до 300 px, чтобы 'Определяется системой' не обрезался."
}

$mainFrameText = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\mainfrm.cpp")) + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\DocumentTree.cpp")) + "`n" + (Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\DocumentTree.h"))
foreach ($pattern in @(
    "RefreshLocalizedMainFrameUi()",
    "RefreshLocalizedToolbarCaptions()",
    "RefreshLocalizedToolbarButtonTexts(m_CmdToolbar)",
    "RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar)",
    "OnRuntimeToolTipTextW",
    "OnRuntimeToolTipTextA",
    "RefreshBundledPluginMenuTexts",
    "FillMenuWithHkeys(m_MenuBar.GetMenu())",
	"m_MenuBar.SetButtonInfo(index, &buttonInfo)",
	"TB_DELETEBUTTON",
    "m_status.SetPaneText(ID_PANE_INS, m_last_sci_ovr ? strOVR : strINS)",
    "m_document_tree.RefreshLocalizedTitle()",
    "RefreshLocalizedMenuCaptions()",
    "m_Speller->EndDocumentCheck()"
)) {
    if ($mainFrameText -notlike "*$pattern*") {
        throw "mainfrm.cpp не содержит обязательный элемент live-refresh контракта языка: $pattern"
    }
}
foreach ($pattern in @(
    "EditorConfigurationSnapshot",
    "CaptureEditorConfigurationSnapshot()",
	"sourceColorPalette",
	"snapshot.sourceColorPalette = _Settings.GetXmlSrcColorPalette()",
	"SCE_H_SGML_BLOCK_DEFAULT",
	"SCE_H_XCCOMMENT",
    "previousInterfaceLanguage != _Settings.GetInterfaceLanguageID()",
    "_Settings.Save()",
    "_Settings.SaveWords()"
)) {
    if ($mainFrameText -notlike "*$pattern*") {
        throw "mainfrm.cpp не содержит обязательный элемент безопасной live-смены языка: $pattern"
    }
}

$refreshMatch = [regex]::Match($mainFrameText, 'void\s+CMainFrame::RefreshLocalizedMainFrameUi\s*\(\s*\)\s*\{(?<body>.*?)\n\}', [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $refreshMatch.Success) {
    throw "Не удалось выделить RefreshLocalizedMainFrameUi для проверки безопасного обновления меню."
}
foreach ($forbiddenPattern in @("LoadMenu(", "m_MenuBar.AttachMenu", "InitPlugins()", "m_import_plugins.RemoveAll", "m_export_plugins.RemoveAll", "m_scripts.RemoveAll", "m_scripts_images.RemoveAll")) {
    if ($refreshMatch.Groups['body'].Value -like "*$forbiddenPattern*") {
        throw "RefreshLocalizedMainFrameUi не должен повторно создавать меню, плагины, скрипты или toolbar: $forbiddenPattern"
    }
}
if ($mainFrameText -notmatch "acceleratorSeparator\s*=\s*text\.Find\(L'\\t'\)") {
    throw "FillMenuWithHkeys должен заменять прежнюю подпись горячей клавиши, а не добавлять её повторно при каждой смене языка."
}

foreach ($language in $requiredInterfaceLanguages) {
    if ($optDlgText -notlike "*$($language.LanguageSymbol), $($language.ResourceId)*") {
        throw "В OptDlg.cpp отсутствует язык интерфейса $($language.Locale) ($($language.LanguageSymbol) / $($language.ResourceId))."
    }
    $localeReturnPattern = "return\s+L`"$([regex]::Escape($language.Locale))`""
    if ($settingsText -notmatch $localeReturnPattern) {
        throw "В Settings.cpp отсутствует mapping языка интерфейса в locale $($language.Locale)."
    }
    if ($sharedRuntimeHelperText -notlike "*$($language.Locale)*") {
        throw "В общем runtime whitelist отсутствует locale $($language.Locale). Файл: $sharedRuntimeHelperPath"
    }
    if ($language.CatalogKey -notin $appCatalogKeys) {
        throw "В app-ui catalog отсутствует ключ подписи языка: $($language.CatalogKey)."
    }
}

$runtimeConsumers = @(
    "src\fbe\RuntimeLocalization.cpp",
    "src\fbv\FBV.cpp",
    "src\export-html\RuntimeLocalization.cpp",
    "src\export-docx\RuntimeLocalization.cpp",
    "src\export-epub\RuntimeLocalization.cpp",
    "src\import-epub\RuntimeLocalization.cpp"
)

foreach ($pattern in @("FBE_NEXT_UI_LOCALE", "FBE Next", "interface-locale.txt", "GetPreferredRuntimeLocaleName", "LoadRuntimeStringFiles")) {
    if ($sharedRuntimeHelperText -notlike "*$pattern*") {
        throw "Общий runtime localization helper не содержит обязательный элемент контракта: $pattern. Файл: $sharedRuntimeHelperPath"
    }
}

foreach ($relativePath in $runtimeConsumers) {
    $path = Join-Path $repoRoot $relativePath
    $text = Get-Content -Raw -LiteralPath $path
    $usesLocalContract = $true
    foreach ($pattern in @("FBE_NEXT_UI_LOCALE", "FBE Next", "interface-locale.txt", "GetPreferredRuntimeLocaleName")) {
        if ($text -notlike "*$pattern*") {
            $usesLocalContract = $false
            break
        }
    }

    $usesSharedHelper = $text -like "*RuntimeLocalizationCommon.h*" -and $text -like "*FbeRuntimeLocalization::*"
    if (-not $usesLocalContract -and -not $usesSharedHelper) {
        throw "Файл $path не читает общий runtime-контракт языка напрямую и не использует RuntimeLocalizationCommon.h."
    }
}

$legacyLoadStringMatches = Get-ChildItem -LiteralPath (Join-Path $repoRoot "src\fbe") -Recurse -Include *.cpp,*.h |
    Select-String -Pattern "\.LoadString\(" -SimpleMatch
if ($legacyLoadStringMatches) {
    $first = $legacyLoadStringMatches | Select-Object -First 1
    throw "В FBE остался обход runtime JSON через CString::LoadString: $($first.Path):$($first.LineNumber)"
}

$approvedRuntimeLoadStringFiles = @(
    "src\fbv\FBV.cpp",
    "src\export-html\RuntimeLocalization.cpp",
    "src\export-docx\RuntimeLocalization.cpp",
    "src\export-epub\RuntimeLocalization.cpp",
    "src\import-epub\RuntimeLocalization.cpp"
)
$runtimeLoadStringSearchRoots = @(
    "src\fbv",
    "src\export-html",
    "src\export-docx",
    "src\export-epub",
    "src\import-epub"
)
$runtimeLoadStringMatches = foreach ($root in $runtimeLoadStringSearchRoots) {
    Get-ChildItem -LiteralPath (Join-Path $repoRoot $root) -Recurse -Include *.cpp,*.h |
        Select-String -Pattern "\.LoadString\(|LoadStringW\(|LoadString\("
}
foreach ($match in $runtimeLoadStringMatches) {
    $relativePath = [IO.Path]::GetRelativePath($repoRoot, $match.Path) -replace "/", "\"
    if ($relativePath -in $approvedRuntimeLoadStringFiles) {
        continue
    }

    # Старый getuname.dll-код в Utils.cpp оставлен под #if 0 и не участвует в runtime.
    if ($relativePath -in @("src\export-html\Utils.cpp", "src\export-docx\Utils.cpp")) {
        $fileLines = Get-Content -LiteralPath $match.Path
        $before = $fileLines[0..([Math]::Max(0, $match.LineNumber - 2))]
        $after = $fileLines[($match.LineNumber - 1)..([Math]::Min($fileLines.Count - 1, $match.LineNumber + 20))]
        $insideDisabledBlock = ($before -match "^\s*#if\s+0\s*$") -and ($after -match "^\s*#endif\s*$")
        if ($insideDisabledBlock) {
            continue
        }
    }

    throw "В FBV/плагинах найден прямой LoadString вне разрешённого runtime fallback: ${relativePath}:$($match.LineNumber)"
}
$pluginRefreshChecks = @(
    @{ Path = "src\export-html\ExportHTMLPlugin.cpp"; Pattern = "InitExportHtmlRuntimeStrings();"; Description = "ExportHTML перечитывает runtime JSON при запуске экспорта" },
    @{ Path = "src\export-docx\ExportDOCXPlugin.cpp"; Pattern = "InitExportDocxRuntimeStrings();"; Description = "ExportDOCX перечитывает runtime JSON при запуске экспорта" },
    @{ Path = "src\export-epub\ExportEPUBPlugin.cpp"; Pattern = "InitExportEpubRuntimeStrings(_AtlBaseModule.GetModuleInstance());"; Description = "ExportEPUB перечитывает runtime JSON при запуске экспорта" },
    @{ Path = "src\import-epub\ImportEPUBPlugin.cpp"; Pattern = "InitImportEpubRuntimeStrings();"; Description = "ImportEPUB перечитывает runtime JSON при запуске импорта" }
)

foreach ($check in $pluginRefreshChecks) {
    $path = Join-Path $repoRoot $check.Path
    $text = Get-Content -Raw -LiteralPath $path
    if ($text -notlike "*$($check.Pattern)*") {
        throw "Не выполнен контракт runtime-локализации: $($check.Description). Файл: $path"
    }
}

Write-Host "Контракт выбранного языка интерфейса для runtime JSON-локализации прошёл проверку."
Write-Host "  Потребителей:" $runtimeConsumers.Count
Write-Host "  Источник: существующая настройка языка FBE"
Write-Host "  Языков в настройке:" $requiredInterfaceLanguages.Count
Write-Host "  Общий helper:" $sharedRuntimeHelperPath


