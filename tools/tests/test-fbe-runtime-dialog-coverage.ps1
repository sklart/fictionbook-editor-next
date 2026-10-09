<# Ensures every catalog text is either bound by the generic dialog layer or
   consumed by an existing explicit runtime-localization implementation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$bindingSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\RuntimeLocalization.cpp')
$mainFrameSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')

# Each DIALOGEX must be connected to a concrete initialization path.  Generic
# bindings are valid only when that dialog actually calls the generic helper;
# the remaining dialogs retain their existing specialised consumer.
$consumers = @{
    IDD_TABLE = @{ File = 'src\fbe\FBEview.h'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_TABLE\)' }
    IDD_INPUTBOX = @{ File = 'src\fbe\apputils.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_INPUTBOX\)' }
    IDD_ADDIMAGE = @{ File = 'src\fbe\FBEview.h'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_ADDIMAGE\)' }
    IDD_TOOLS_SETTINGS = @{ File = 'src\fbe\settings\ui\SettingsDlg.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_TOOLS_SETTINGS\)' }
    IDD_ABOUTBOX = @{ File = 'src\fbe\AboutBox.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_ABOUTBOX\)' }
    IDD_SETTINGS_WORDS = @{ File = 'src\fbe\settings\ui\SettingsWordsDlg.cpp'; Invocation = 'SetRuntimeSettingsWordsText' }
    IDD_HOTKEYS = @{ File = 'src\fbe\settings\ui\SettingsHotkeysDlg.cpp'; Invocation = 'SetRuntimeHotkeysText' }
    IDD_FIND = @{ File = 'src\fbe\SearchReplace.h'; AdditionalFiles = @('src\fbe\FBEview.cpp', 'src\fbe\search\SearchPresetCatalog.cpp', 'src\fbe\search\RegexQuickReference.h', 'src\fbe\search\ui\RegexQuickReferencePopup.cpp'); Invocation = 'SetRuntimeDialogTitle' }
    IDD_REPLACE = @{ File = 'src\fbe\SearchReplace.h'; Invocation = 'SetRuntimeDialogTitle' }
    IDD_FIND_RESULTS = @{ File = 'src\fbe\FindResultsPane.cpp'; Invocation = 'FbeLoadRuntimeStringByKey' }
    IDD_FB2_QUALITY_RESULTS = @{ File = 'src\fbe\Fb2QualityChecker.cpp'; AdditionalFiles = @('src\fbe\mainfrm.cpp'); Invocation = 'FbeLoadRuntimeStringByKey' }
    IDD_REGEX_HELP = @{ File = 'src\fbe\search\ui\RegexHelpDialog.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_REGEX_HELP\)' }
    IDD_SCRIPTS_TOOLBAR_CUSTOMIZE = @{ File = 'src\fbe\ScriptsToolbarCustomizeDlg.cpp'; AdditionalFiles = @('src\fbe\mainfrm.cpp'); Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD\)' }
    IDD_SCRIPT_TOOLBAR_MANAGER = @{ File = 'src\fbe\ScriptToolbarManagerDlg.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD\)' }
    IDD_SPELL_CHECK = @{ File = 'src\fbe\Speller.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SPELL_CHECK\)' }
    IDD_WORDS = @{ File = 'src\fbe\Words.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_WORDS\)' }
    IDD_SETTINGS_IMAGES = @{ File = 'src\fbe\settings\ui\SettingsImagesPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_IMAGES\)' }
    IDD_SETTINGS_GENERAL = @{ File = 'src\fbe\settings\ui\SettingsGeneralPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_GENERAL\)' }
    IDD_SETTINGS_EDITOR = @{ File = 'src\fbe\settings\ui\SettingsEditorPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_EDITOR\)' }
    IDD_SETTINGS_SPELLING = @{ File = 'src\fbe\settings\ui\SettingsSpellingPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_SPELLING\)' }
    IDD_SETTINGS_SOURCE = @{ File = 'src\fbe\settings\ui\SettingsSourcePage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_SOURCE\)' }
    IDD_SETTINGS_ADVANCED = @{ File = 'src\fbe\settings\ui\SettingsAdvancedPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_ADVANCED\)' }
    'runtime/main.js' = @{ File = 'runtime\main.js'; Invocation = 'GetLocalizedString' }
    IDS_REPL_ALL_CAPT = @{ File = 'src\fbe\FBEview.cpp'; Invocation = 'FbeLoadRuntimeStringByKey' }
    IDS_REPL_DONE_MSG = @{ File = 'src\fbe\FBEview.cpp'; Invocation = 'FbeLoadRuntimeStringByKey' }
}

foreach ($resource in $consumers.Keys) {
    $consumer = $consumers[$resource]
    $consumerPath = Join-Path $repoRoot $consumer.File
    $consumerText = Get-Content -Raw -LiteralPath $consumerPath
    if ($consumer.ContainsKey('AdditionalFiles')) {
        foreach ($additionalFile in $consumer.AdditionalFiles) {
            $consumerText += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $repoRoot $additionalFile))
        }
    }
    if ($consumerText -notmatch $consumer.Invocation) {
        throw "$resource does not invoke its declared runtime localization consumer: $($consumer.File)"
    }
    $consumers[$resource].Text = $consumerText
}

foreach ($entry in $catalog.strings.PSObject.Properties) {
    $key = $entry.Name
    $value = $entry.Value
    if ($value.targetId -eq 'IDC_STATIC') {
        throw "Runtime-localized control must have a stable ID, not IDC_STATIC: $key"
    }
    if (-not $consumers.ContainsKey($value.resource)) {
        throw "No runtime consumer is declared for $($value.resource), required by $key."
    }
    $consumerText = $consumers[$value.resource].Text
    $escapedKey = [regex]::Escape($key)
    $genericBindingPattern = '\{\s*' + [regex]::Escape($value.resource) + ',\s*[^,]+,\s*L"' + $escapedKey + '"\s*\}'
    $genericBinding = $bindingSource -match $genericBindingPattern
    # Built-in editor background labels are manifest-driven.  Their stable
    # localization keys are validated against backgrounds.json by the asset
    # contract; the page deliberately must not duplicate the preset list.
    $manifestDrivenBackgroundPreset = $key -like 'fbe.settings.editor_background.preset.*' -and
        $consumerText.Contains('m_builtInBackgrounds[i].localizationKey')
    # Search preset labels are generated from their stable IDs by SearchPresetCatalog.
    $dynamicSearchPresetKey = $value.resource -eq 'IDD_FIND' -and $key -like 'fbe.search_preset.*' -and
        $consumerText.Contains('StableBuiltInLocalizationKey') -and $consumerText.Contains('FbeLoadRuntimeStringByKey')
    $dynamicQualityRuleKey = $value.resource -eq 'IDD_FB2_QUALITY_RESULTS' -and $key -like 'fbe.quality.rule.*' -and
        $consumerText.Contains('L"fbe.quality.rule." + suffix') -and
        $consumerText.Contains('L"Q-' + $key.Substring('fbe.quality.rule.'.Length).ToUpperInvariant() + '"')
    $dynamicQualityHelpKey = $value.resource -eq 'IDD_FB2_QUALITY_RESULTS' -and
        ($key -like 'fbe.quality.detail.*' -or $key -like 'fbe.quality.action.*') -and
        $consumerText.Contains('L"' + $key.Substring(0, $key.LastIndexOf('.') + 1) + '"') -and
        $consumerText.Contains('suffix = L"' + $key.Substring($key.LastIndexOf('.') + 1) + '"')
    if (-not $genericBinding -and -not $manifestDrivenBackgroundPreset -and -not $dynamicSearchPresetKey -and
        -not $dynamicQualityRuleKey -and -not $dynamicQualityHelpKey -and -not $consumerText.Contains($key)) {
        throw "JSON dialog key has no binding in its concrete runtime consumer: $key ($($value.resource))."
    }
}

if (-not $bindingSource.Contains('FbeApplyRuntimeDialogLocalization')) {
    throw 'The generic runtime dialog binding is missing.'
}

# IDD_ADDIMAGE deliberately reuses IDCANCEL as the semantic "No" button.
# It must retain that identity after WM_INITDIALOG, rather than inherit the
# unrelated input-box Cancel caption from a later manual override.
$addImageSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$addImageBlock = [regex]::Match($addImageSource, '(?s)class CAddImageDlg.*?\r?\n\s*};').Value
if ([string]::IsNullOrEmpty($addImageBlock)) { throw 'CAddImageDlg definition is missing.' }
foreach ($required in @(
    'SetProp(m_hWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION", reinterpret_cast<HANDLE>(1))',
    'FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_ADDIMAGE)',
    'SetDlgItemText(m_hWnd, IDCANCEL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_addimage.no", L"No"))',
    'COMMAND_ID_HANDLER(IDCANCEL, OnBtnClicked)',
    'wID == IDYES ? true : false'
)) {
    if (-not $addImageBlock.Contains($required)) { throw "IDD_ADDIMAGE No-button runtime contract is missing: $required" }
}
if ($addImageBlock.Contains('fbe.dialog.idd_inputbox.cancel')) {
    throw 'CAddImageDlg must not replace its No button with the input-box Cancel key.'
}
if ($mainFrameSource -notmatch 'GetProp\(hChildWnd, L"FBE_SKIP_SYSTEM_DIALOG_LOCALIZATION"\) == NULL') {
    throw 'The system-dialog localization hook no longer honors the per-dialog skip property.'
}
$addImageNo = $catalog.strings.'fbe.dialog.idd_addimage.no'
if ($null -eq $addImageNo -or $addImageNo.resource -ne 'IDD_ADDIMAGE' -or $addImageNo.targetId -ne 'IDCANCEL') {
    throw 'IDD_ADDIMAGE + IDCANCEL must be bound to fbe.dialog.idd_addimage.no.'
}
if ($addImageNo.translations.'ru-RU' -ne 'Нет' -or $addImageNo.translations.'en-US' -ne 'No') {
    throw 'IDD_ADDIMAGE No translations are incorrect for ru-RU or en-US.'
}
foreach ($language in $catalog.targetLanguages) {
    if ([string]::IsNullOrWhiteSpace($addImageNo.translations.$language)) { throw "IDD_ADDIMAGE No translation is missing: $language" }
}
Write-Host "Runtime dialog coverage verified: $(@($catalog.strings.PSObject.Properties).Count) catalog keys."
