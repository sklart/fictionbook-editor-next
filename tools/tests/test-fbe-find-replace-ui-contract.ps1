<#
.SYNOPSIS
Guards the common Find/Replace controls, their localized labels, and Results
Pane painting contracts without materializing virtual ListView rows.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$rc = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SearchReplace.h')
$pane = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.cpp')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$sourceView = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$presetCatalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\SearchPresetCatalog.cpp')
$regexHelp = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\ui\RegexHelpDialog.cpp')
$regexHelpMarkdown = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$settings = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.h')
$settingsSerialization = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\settings\SettingsSerialization.cpp')
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json

function Require([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}
function DialogBlock([string]$id) {
    $match = [regex]::Match($rc, "(?s)$id DIALOGEX.*?^END", [Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) { throw "Dialog $id was not found." }
    return $match.Value
}
function Get-ControlRect([string]$dialogBlock, [string]$control) {
    $escapedControl = [regex]::Escape($control)
    $match = [regex]::Match($dialogBlock, ('(?m)^[^\r\n]*\b{0}\b[^\r\n]*?,(\d+),(\d+),(\d+),(\d+)' -f $escapedControl))
    if (-not $match.Success) { throw "Rectangle for $control was not found." }
    return @{ left = [int]$match.Groups[1].Value; top = [int]$match.Groups[2].Value; width = [int]$match.Groups[3].Value; height = [int]$match.Groups[4].Value }
}
function Assert-NoOverlap($rectA, $rectB, [string]$description) {
    $overlap = $rectA.left -lt ($rectB.left + $rectB.width) -and $rectB.left -lt ($rectA.left + $rectA.width) -and $rectA.top -lt ($rectB.top + $rectB.height) -and $rectB.top -lt ($rectA.top + $rectA.height)
    if ($overlap) { throw "Controls overlap: $description." }
}
function RequireLocalized([string]$key) {
    $entry = $catalog.strings.$key
    if ($null -eq $entry -or [string]::IsNullOrWhiteSpace($entry.translations.'en-US') -or [string]::IsNullOrWhiteSpace($entry.translations.'ru-RU')) {
        throw "Missing English or Russian runtime localization for $key."
    }
}

$find = DialogBlock 'IDD_FIND'
$replace = DialogBlock 'IDD_REPLACE'
foreach ($control in @('IDC_WHOLE', 'IDC_MATCHCASE', 'IDC_REGEXP', 'IDC_FIND_SCOPE_LABEL', 'IDC_FIND_SCOPE', 'IDC_FIND_UNICODE_PROPERTIES', 'IDC_UP', 'IDC_DOWN', 'IDC_FIND_FROM_START')) {
    Require $find $control "Find control $control"
    Require $replace $control "Replace control $control"
}
Require $find 'IDC_FIND_SCOPE[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Find UCP immediately follows Scope'
Require $replace 'IDC_FIND_SCOPE[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Replace UCP immediately follows Scope'
Require $find 'IDC_FIND_LABEL_TEXT,7,9,50,8[\s\S]*?IDC_TEXT,60,7,170,62' 'Find label and input use the shared horizontal grid'
Require $replace 'IDC_REPLACE_LABEL_TEXT,7,9,50,8[\s\S]*?IDC_TEXT,60,7,170,62' 'Replace label and input use the shared horizontal grid'
foreach ($dialogBlock in @($find, $replace)) {
    Require $dialogBlock 'IDC_WHOLE,"Button",BS_AUTOCHECKBOX \| WS_TABSTOP,7,' 'common options start at x=7'
    Require $dialogBlock 'IDC_FIND_SCOPE_LABEL,122,' 'Scope uses the shared middle column'
    Require $dialogBlock 'DIRECTION_GROUP,194,' 'Direction has a dedicated column'
    Require $dialogBlock 'ID_FIND_NEXT,254,7,64,14' 'Find Next uses the shared action column'
    Require $dialogBlock 'IDCANCEL,254,61,64,14' 'Cancel uses the shared action slot'
}
Require $find 'IDC_FIND_ALL' 'Find All action'
Require $replace 'IDC_REPLACE_ONE[\s\S]*?IDC_REPLACE_ALL' 'Replace-specific actions'
if ($replace -match 'IDC_FIND_ALL') { throw 'Replace must not add a duplicate Find All action.' }
Require $dialog 'GetDlgItem\(IDC_FIND_SCOPE\) != NULL[\s\S]*?PopulateFindScopes' 'common scope initialization'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnFindFromStart[\s\S]*?DoSearchFromScopeStart' 'Replace From start action'
Require $dialog 'class CReplaceDlgBase[\s\S]*?OnScopeChanged[\s\S]*?ResetSearchScope' 'Replace preserves a stable scope until the user changes it'
Require $dialog 'EnableWindow\(unicode, SearchContext\(\) == FbeSearchPresets::SearchUiContext::Design && ::IsDlgButtonChecked' 'UCP is RegExp-gated in Design and unavailable in Source'
Require $pane 'm_list\.GetItemState\(item, LVIS_SELECTED\) & LVIS_SELECTED' 'authoritative ListView selection painting'
Require $pane 'ThemeManager::Brush\(selected \? THEME_COLOR_SELECTION_BACKGROUND : THEME_COLOR_WINDOW\)' 'complete selected and unselected cell repaint'
Require $pane 'L" \\x2014 \\x00AB" \+ query \+ L"\\x00BB \\x2014 "' 'Unicode-safe Results Pane header punctuation'
if ($pane -match 'title \+= L" —') { throw 'Results Pane header must not depend on a source-code-page em dash literal.' }

$caption = $catalog.strings.'fbe.dialog.idd_find_results.caption'.translations
$count = $catalog.strings.'fbe.dialog.idd_find_results.count'.translations
if ($caption.'ru-RU' -ne 'Результаты поиска' -or $count.'ru-RU' -ne 'Найдено: %Iu') { throw 'Russian Results Pane header localization is not canonical.' }
if ($caption.'en-US' -ne 'Find results' -or $count.'en-US' -ne 'Found: %Iu') { throw 'English Results Pane header localization is not canonical.' }

foreach ($key in @(
    'fbe.dialog.idd_find.unicode_properties', 'fbe.dialog.idd_find.scope', 'fbe.dialog.idd_find.from_start',
    'fbe.replace.preview.completed', 'fbe.tooltip.find.unicode_properties')) {
    RequireLocalized $key
}
foreach ($key in @('fbe.dialog.idd_replace.unicode_properties', 'fbe.dialog.idd_replace.scope', 'fbe.dialog.idd_replace.from_start')) {
    if ($null -ne $catalog.strings.$key) { throw "Duplicate Replace localization key $key must not exist." }
}
Require $dialog 'SyncSearchOptionsToOpenDialogs\(this\)' 'immediate Find/Replace common-option synchronization'
Require $dialog 'SyncSearchOptionsFromView' 'peer dialog control synchronization'
Require $dialog 'm_tooltips\.Add\(GetDlgItem\(IDC_FIND_UNICODE_PROPERTIES\), ucpKey, ucpFallback\)[\s\S]*?m_tooltips\.AddDisabledControlArea\(GetDlgItem\(IDC_FIND_UNICODE_PROPERTIES\), ucpKey, ucpFallback\)' 'shared enabled and disabled UCP tooltip delivery'
Require $dialog 'Use Unicode properties for \\\\w, \\\\d, \\\\s and word boundaries \\\\b/\\\\B \(for example with Cyrillic text\)\. Available only when Regular expression is enabled\.' 'informative portable UCP tooltip fallback'
Require $pane 'fbe\.dialog\.idd_find_results\.count", L"Found: %Iu"' 'neutral portable Results Pane count fallback'
Require $view 'fbe\.replace\.preview\.message", L"Number of replacements: %Iu\. Continue\?"' 'neutral portable Replace All confirmation fallback'
if ($dialog -match 'idd_replace\.(unicode_properties|scope|from_start)') { throw 'Replace must use shared Find localization keys for common controls.' }
if ($catalog.strings.'fbe.replace.preview.message'.translations.'ru-RU' -ne 'Количество замен: %Iu. Продолжить?') { throw 'Russian Replace All confirmation is not canonical.' }
if ($catalog.strings.'fbe.replace.preview.message'.translations.'en-US' -ne 'Number of replacements: %Iu. Continue?') { throw 'English Replace All confirmation is not canonical.' }
foreach ($language in @('en-US', 'ru-RU', 'uk-UA', 'de-DE', 'fr-FR', 'es-ES', 'it-IT', 'pl-PL', 'pt-PT', 'nl-NL', 'cs-CZ', 'bg-BG')) {
    if ([string]::IsNullOrWhiteSpace($count.$language) -or [string]::IsNullOrWhiteSpace($catalog.strings.'fbe.replace.preview.message'.translations.$language)) {
        throw "Neutral search count or Replace All confirmation is missing for $language."
    }
}
if ($null -ne $catalog.strings.'fbe.replace.preview.ready') { throw 'Obsolete second-click Replace All prompt must not remain localized.' }
if ($catalog.strings.'fbe.tooltip.find.unicode_properties'.translations.'ru-RU' -ne 'Использовать Unicode-свойства для \w, \d, \s и границ слов \b/\B (например, для кириллицы). Доступно только при включённом «Регулярное выражение».') { throw 'Russian UCP tooltip is not canonical.' }
if ($catalog.strings.'fbe.tooltip.find.unicode_properties'.translations.'en-US' -ne 'Use Unicode properties for \w, \d, \s and word boundaries \b/\B (for example with Cyrillic text). Available only when Regular expression is enabled.') { throw 'English UCP tooltip is not canonical.' }

foreach ($dialogBlock in @($find, $replace)) {
    foreach ($control in @('IDC_FIND_TEMPLATES', 'IDC_FIND_REGEX_HELP', 'IDC_FIND_PRESETS_LABEL', 'IDC_FIND_PRESETS_TREE', 'IDC_FIND_PRESET_DESCRIPTION', 'IDC_FIND_PRESET_APPLY', 'IDC_FIND_PRESET_SAVE', 'IDC_FIND_PRESET_UPDATE', 'IDC_FIND_PRESET_RENAME', 'IDC_FIND_PRESET_DELETE', 'IDC_FIND_PRESETS_PIN')) {
        Require $dialogBlock $control "Find/Replace template control $control"
    }
    Require $dialogBlock 'DIALOGEX 0, 0, 326,' 'base compact dialog width provides a shared grid'
}
Require $regexHelp 'FbeRegexHelp::LoadMarkdown' 'Regex help loads Markdown at dialog creation'
Require $regexHelp 'FbeRegexHelp::RenderMarkdown' 'Regex help renders parsed Markdown'
Require $regexHelp 'ThemeManager::ApplyToWindow\(m_hWnd\)' 'Regex help theme integration'
Require $regexHelp 'FbeApplyRuntimeDialogLocalization\(m_hWnd, IDD_REGEX_HELP\)' 'Regex help runtime localization'
Require $regexHelpMarkdown 'EM_SETSEL, 0, 0' 'Regex help clears its initial selection'
Require $regexHelpMarkdown 'EM_SCROLLCARET' 'Regex help scrolls to its beginning'
Require $regexHelp 'SetFocus\(GetDlgItem\(IDC_REGEX_HELP_CLOSE\)\)' 'Regex help focuses Close'
foreach ($token in @('ReadUtf8File', 'ParseMarkdown', 'ParseInlineCode', 'HelpPathForLocale', 'GetPreferredRuntimeLocaleName', 'EM_REPLACESEL', 'WM_GETTEXTLENGTH', 'CFM_BOLD', 'dwEffects = bold ? CFE_BOLD : 0', 'Consolas')) {
    if ($regexHelpMarkdown -notmatch [regex]::Escape($token)) { throw "Missing Markdown Regex Help behavior: $token" }
}
if ($regexHelpMarkdown -match 'JoinHelpBlocks|starts\[' -or $regexHelp -match 'BuildHelpBlocks|AddQuickReferenceSyntax') { throw 'Regex Help must not keep the embedded joined-text implementation.' }Require $regexHelp 'IDC_REGEX_HELP_CLOSE' 'Regex help uses its dedicated Close control for layout and dispatch'
Require $regexHelp 'MonitorFromRect' 'Regex help restores its saved normal position on a valid monitor'
Require $regexHelp 'WM_FBE_THEMECHANGED' 'Regex help reapplies its Rich Edit palette on app theme changes'
Require $regexHelp 'WM_GETMINMAXINFO' 'Regex help enforces a minimum resizable size'
Require $regexHelp 'WM_SIZE' 'Regex help lays out controls while resizing'
Require $regexHelp '_Settings\.GetRegexHelpPlacement' 'Regex help restores its saved size'
Require $regexHelp '_Settings\.SetRegexHelpPlacement' 'Regex help persists its size'
Require $regexHelp 'LoadLibraryW\(L"Msftedit\.dll"\)' 'Regex help loads the Rich Edit control before creation'
Require $settings 'm_regex_help_placement' 'Regex help placement setting storage'
Require $settings 'GetRegexHelpPlacement' 'Regex help placement setting getter'
Require $settings 'SetRegexHelpPlacement' 'Regex help placement setting setter'
Require $settingsSerialization 'REGEX_HELP_PLACEMENT_KEY' 'Regex help placement serialized setting'
Require $rc '(?s)IDD_REGEX_HELP.*?WS_THICKFRAME' 'Regex help dialog is resizable'
Require $rc '(?s)IDD_REGEX_HELP.*?RICHEDIT50W' 'Regex help uses Rich Edit formatting'
Require $pane 'UiMetrics::CreateDialogFontForDpi' 'Results Pane owns a DPI-specific dialog font'
Require $pane 'SelectObject\(dc,\s*m_font\)' 'Results Pane custom draw selects its owned font'
Require $rc 'IDD_REGEX_HELP[\s\S]*?DEFPUSHBUTTON\s+"Close",IDC_REGEX_HELP_CLOSE' 'Regex help uses Close rather than Cancel'
Require $dialog 'fbe\.tooltip\.find\.templates' 'Templates button tooltip'
Require $dialog 'fbe\.tooltip\.find\.regex_help_design' 'Design regex-help tooltip'
Require $dialog 'fbe\.tooltip\.find\.regex_help_source' 'Source regex-help tooltip'
Require $dialog 'MakePresetPreviewValue' 'safe preset preview formatter'
Require $dialog 'fbe\.search_preset\.preview\.find' 'Find preset preview localization'
Require $dialog 'fbe\.search_preset\.preview\.replace' 'Replace preset preview localization'
Require $dialog 'SetPresetPanelVisible\(!collapse\)' 'template panel expand/collapse integration'
Require $dialog 'm_compactDialogHeight' 'separate compact dialog height'
Require $dialog 'LayoutPresetPanel' 'templates panel uses a dedicated downward layout'
Require $dialog 'struct PresetPanelMetrics' 'templates panel uses shared layout metrics'
Require $dialog 'GetPresetPanelMetrics' 'templates panel height and layout share metrics'
Require $dialog 'metrics\.treeHeight = \(std::max\)\(metrics\.lineHeight \* 10' 'templates panel keeps ten tree rows at minimum'
Require $dialog 'LocalizedButtonWidth' 'templates Apply width measures its localized caption'
Require $dialog 'GetTextExtentPoint32W' 'templates Apply width is measured from the active button font'
Require $dialog 'applyMinimum' 'templates Apply width has a DPI-aware minimum'
Require $dialog 'applyMaximum' 'templates Apply width has a bounded maximum'
Require $dialog 'PreviewHeightForCurrentSelection' 'templates preview height follows its content'
Require $dialog 'DT_CALCRECT \| DT_WORDBREAK' 'templates preview measures wrapped localized text'
Require $dialog 'lineHeight \* 4' 'templates preview is bounded to four lines'
Require $dialog 'PresetPinMaskResource' 'templates pin selects an authored bitmap mask'
Require $dialog 'GetDIBits' 'templates pin derives coverage from its bitmap mask'
Require $dialog 'struct BitmapInfo1Bit' 'templates pin reserves a two-entry monochrome bitmap info'
Require $dialog 'maskInfo.header.biBitCount = 1' 'templates pin uses a monochrome authored mask'
Require $dialog 'maskBits\[' 'templates pin uses mask bits instead of RGB coverage'
if($dialog -match 'DrawIconEx\(|GetIconInfo\(|maskPixels\[index\]') { throw 'Templates pin must not recover alpha from an ICO.' }
Require $dialog 'MakePresetPreviewValue\(preset->findText, 168\)' 'templates preview retains a useful clipped length'
Require $rc 'IDC_FIND_PRESETS_PIN,"Button",BS_OWNERDRAW' 'templates pin uses an owner-drawn glyph rather than a text-only checkbox'
Require $dialog 'SetSearchTemplatesPanelPinned' 'templates pin is persisted'
Require $settings 'm_search_templates_panel_pinned' 'templates pin setting storage'
Require $settingsSerialization 'SEARCH_TEMPLATES_PANEL_PINNED_KEY' 'templates pin serialized setting'
Require $dialog 'const int width = m_compactDialogWidth' 'expanded templates keep compact width'
Require $dialog 'm_presetPanelHeight = GetPresetPanelMetrics\(availablePanelHeight\)\.totalHeight' 'expanded templates fit monitor work area'
Require $dialog 'MonitorFromWindow\(dialog, MONITOR_DEFAULTTONEAREST\)' 'expanded dialog is constrained to its current monitor'
Require $dialog 'GetMonitorInfo\(monitor, &monitorInfo\)' 'expanded dialog uses monitor work area'
Require $dialog 'UpdatePresetToggleCaption' 'template toggle caption changes by state'
Require $dialog 'fbe\.search_preset\.expand' 'collapsed template caption localization'
Require $dialog 'fbe\.search_preset\.collapse' 'expanded template caption localization'
Require $dialog 'ShowWindow\(GetDlgItem\(controls\[index\]\), visible \? SW_SHOW : SW_HIDE\)' 'hidden panel controls leave tab navigation'
Require $dialog 'virtual void InvalidateSearchSelectionState\(\) \{\}' 'search selection invalidation hook'
Require $dialog 'virtual void InvalidateSearchSelectionState\(\) \{ m_selvalid = false; \}' 'Replace preset application invalidates m_selvalid'
Require $dialog 'InvalidateSearchSelectionState\(\);[\s\S]*?m_view->m_startMatch = m_view->m_endMatch = 0;[\s\S]*?m_view->m_fo\.ClearMatch\(\);[\s\S]*?m_view->m_design_search\.ClearReplacePreview\(\);' 'preset application clears stale Replace state'
Require $dialog 'LoadUserPresetsForMutation' 'failed user-preset load blocks mutations'
Require $dialog 'fbe\.search_preset\.load_failed' 'distinct localized preset-load failure'
Require $dialog 'if \(!IsReplaceDialog\(\)\)[\s\S]*?preset\.hasReplacement = existing\.hasReplacement;[\s\S]*?preset\.replacementText = existing\.replacementText;' 'Find update preserves hidden replacement'
Require $dialog 'EnableWindow\(GetDlgItem\(IDC_FIND_PRESET_SAVE\), findTextLength > 0\);' 'empty Find cannot be saved'
Require $dialog 'NotifyOpenPresetPanels\(\)' 'all Design and Source template panels refresh after mutation'
Require $dialog 'OpenPresetPanels\(\)' 'template panel notification has a shared registry'
Require $dialog 'SearchContext\(\) == FbeSearchPresets::SearchUiContext::Design' 'explicit Design/Source UCP behavior'
Require $dialog 'GetBuiltInPresets\(SearchContext\(\), IsReplaceDialog\(\)' 'context-filtered built-in presets'
Require $presetCatalog 'if\s*\(definition\.context\s*!=\s*context\s*\|\|\s*\(forReplace\s*&&\s*!definition\.hasReplacement\)\)' 'Replace excludes find-only presets'
Require $sourceView 'class CSciFindDlg[\s\S]*?SearchContext\(\) const \{ return FbeSearchPresets::SearchUiContext::Source;' 'CSciFindDlg Source context'
Require $sourceView 'class CSciReplaceDlg[\s\S]*?SearchContext\(\) const \{ return FbeSearchPresets::SearchUiContext::Source;' 'CSciReplaceDlg Source context'
Require $dialog 'class CViewFindDlg[\s\S]*?SearchContext\(\) const \{ return FbeSearchPresets::SearchUiContext::Design;' 'CViewFindDlg Design context'
Require $view 'class CViewReplaceDlg[\s\S]*?SearchContext\(\) const \{ return FbeSearchPresets::SearchUiContext::Design;' 'CViewReplaceDlg Design context'
foreach ($key in @('fbe.search_preset.expand', 'fbe.search_preset.collapse', 'fbe.search_preset.caption', 'fbe.search_preset.apply', 'fbe.search_preset.save_current', 'fbe.search_preset.update', 'fbe.search_preset.rename', 'fbe.search_preset.delete', 'fbe.regex_help.design.caption', 'fbe.regex_help.source.caption', 'fbe.tooltip.find.unicode_properties_source',
    'fbe.search_preset.normalize_spaces.name', 'fbe.search_preset.normalize_spaces.description',
    'fbe.search_preset.duplicate_word.name', 'fbe.search_preset.duplicate_word.description',
    'fbe.search_preset.source_repeated_punctuation.name', 'fbe.search_preset.source_repeated_punctuation.description',
    'fbe.search_preset.preview.find', 'fbe.search_preset.preview.replace', 'fbe.search_preset.preview.empty',
    'fbe.search_preset.load_failed', 'fbe.search_preset.pin',
    'fbe.tooltip.find.templates', 'fbe.tooltip.find.regex_help_design', 'fbe.tooltip.find.regex_help_source')) { RequireLocalized $key }
foreach ($key in @('fbe.search_preset.expand', 'fbe.search_preset.collapse', 'fbe.tooltip.find.regex_help_design', 'fbe.tooltip.find.regex_help_source')) {
    $entry = $catalog.strings.$key
    foreach ($language in $catalog.targetLanguages) { if ([string]::IsNullOrWhiteSpace([string]$entry.translations.$language)) { throw "Missing $language localization for $key." } }
}
foreach ($dialogBlock in @($find, $replace)) {
    Require $dialogBlock 'IDC_FIND_PRESETS_TREE,"SysTreeView32"[^\r\n]*,333,18,198,74' 'template tree has a readable multi-row height'
    Require $dialogBlock 'IDC_FIND_PRESET_DESCRIPTION,333,96,198,34' 'template description has a readable multi-line height'
    Require $dialogBlock 'IDC_FIND_PRESET_SAVE,381,135,92,14' 'Russian Save current caption has room'
    Require $dialogBlock 'IDC_FIND_PRESET_RENAME,382,153,83,14' 'Russian Rename caption has room'
}
Require $find 'IDC_FIND_TEMPLATES,254,82,64,14' 'Find Templates toggle is last in the action column'
Require $replace 'IDC_FIND_TEMPLATES,254,100,64,14' 'Replace Templates toggle follows the common row offset'
Require $find 'IDC_FIND_REGEX_HELP,106,60,12,13' 'Find regex help sits beside RegExp'
Require $replace 'IDC_FIND_REGEX_HELP,106,78,12,13' 'Replace regex help follows the common row offset'
foreach($control in @('IDC_TEXT,60,7,170', 'IDC_FIND_SCOPE,122', 'IDC_FIND_UNICODE_PROPERTIES,"Button"', 'ID_FIND_NEXT,254,7,64,14')) { Require $find $control "Find common grid: $control"; Require $replace $control "Replace common grid: $control" }
foreach($dialogName in @('Find', 'Replace')) {
    $dialogBlock = if($dialogName -eq 'Find') { $find } else { $replace }
    $directionGroup = if($dialogName -eq 'Find') { 'IDC_FIND_DIRECTION_GROUP' } else { 'IDC_REPLACE_DIRECTION_GROUP' }
    $pairs = @(@('IDC_REGEXP', 'IDC_FIND_REGEX_HELP'), @('IDC_FIND_REGEX_HELP', 'IDC_FIND_UNICODE_PROPERTIES'), @('IDC_WHOLE', 'IDC_FIND_SCOPE_LABEL'), @('IDC_MATCHCASE', 'IDC_FIND_SCOPE'), @($directionGroup, 'ID_FIND_NEXT'), @('IDC_TEXT', 'ID_FIND_NEXT'))
    if($dialogName -eq 'Find') { $pairs += ,@('IDC_FIND_TEMPLATES', 'IDC_FIND_STATUS') }
    foreach($pair in $pairs) {
        Assert-NoOverlap (Get-ControlRect $dialogBlock $pair[0]) (Get-ControlRect $dialogBlock $pair[1]) "$dialogName $($pair[0]) / $($pair[1])"
    }
}
Require $find 'IDD_FIND DIALOGEX 0, 0, 326, 116' 'Find compact dimensions provide room for Scope and UCP'
Require $replace 'IDD_REPLACE DIALOGEX 0, 0, 326, 134' 'Replace height equals Find plus one row offset'
if ($presetCatalog -match 'L"\\x\{00A0\}"') { throw 'NBSP preset must use literal U+00A0 for production normalization.' }
Require $presetCatalog 'L"\\u00A0"' 'NBSP preset uses literal U+00A0'
Require $view 'NormalizeSearchPatternNbsp[\s\S]*?pattern\.Replace\(L"\\u00A0", _Settings\.GetNBSPChar\(\)\)' 'production NBSP normalization replaces the literal preset character'
$customNbsp = [string][char]0x25AB
$normalizedNbspPreset = ([string][char]0x00A0).Replace([char]0x00A0, [char]0x25AB)
if ($normalizedNbspPreset -ne $customNbsp -or 'x' + $normalizedNbspPreset + 'y' -notmatch ('x' + [regex]::Escape($customNbsp) + 'y')) { throw 'Built-in NBSP preset must find the configured U+25AB character.' }
Require $presetCatalog 'L"\\u00A0", true, L" "' 'NBSP preset replacement remains an ordinary space'
$builtInLocalizationKeys = @(
    'fbe.search_preset.normalize_spaces.name', 'fbe.search_preset.normalize_spaces.description',
    'fbe.search_preset.trim_before_punctuation.name', 'fbe.search_preset.trim_before_punctuation.description',
    'fbe.search_preset.trim_leading.name', 'fbe.search_preset.trim_leading.description',
    'fbe.search_preset.trim_trailing.name', 'fbe.search_preset.trim_trailing.description',
    'fbe.search_preset.tabs_to_spaces.name', 'fbe.search_preset.tabs_to_spaces.description',
    'fbe.search_preset.nbsp_to_space.name', 'fbe.search_preset.nbsp_to_space.description',
    'fbe.search_preset.duplicate_word.name', 'fbe.search_preset.duplicate_word.description',
    'fbe.search_preset.repeated_punctuation.name', 'fbe.search_preset.repeated_punctuation.description',
    'fbe.search_preset.source_repeated_punctuation.name', 'fbe.search_preset.source_repeated_punctuation.description')
foreach ($key in $builtInLocalizationKeys) {
    RequireLocalized $key
    Require $presetCatalog 'StableBuiltInLocalizationKey' "built-in preset localization lookup $key"
}
Write-Host 'Find/Replace common UI and Results Pane contract passed.'
