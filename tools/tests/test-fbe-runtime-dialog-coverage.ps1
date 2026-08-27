<# Ensures every catalog text is either bound by the generic dialog layer or
   consumed by an existing explicit runtime-localization implementation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$bindingSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\RuntimeLocalization.cpp')

# Each DIALOGEX must be connected to a concrete initialization path.  Generic
# bindings are valid only when that dialog actually calls the generic helper;
# the remaining dialogs retain their existing specialised consumer.
$consumers = @{
    IDD_TABLE = @{ File = 'src\fbe\FBEview.h'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_TABLE\)' }
    IDD_INPUTBOX = @{ File = 'src\fbe\apputils.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_INPUTBOX\)' }
    IDD_ADDIMAGE = @{ File = 'src\fbe\FBEview.h'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_ADDIMAGE\)' }
    IDD_TOOLS_SETTINGS = @{ File = 'src\fbe\SettingsDlg.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_TOOLS_SETTINGS\)' }
    IDD_ABOUTBOX = @{ File = 'src\fbe\AboutBox.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_ABOUTBOX\)' }
    IDD_CUSTOMSAVEDLG = @{ File = 'src\fbe\mainfrm.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(hWnd,\s*IDD_CUSTOMSAVEDLG\)' }
    IDD_SETTINGS_WORDS = @{ File = 'src\fbe\SettingsWordsDlg.cpp'; Invocation = 'SetRuntimeSettingsWordsText' }
    IDD_HOTKEYS = @{ File = 'src\fbe\SettingsHotkeysDlg.cpp'; Invocation = 'SetRuntimeHotkeysText' }
    IDD_FIND = @{ File = 'src\fbe\SearchReplace.h'; Invocation = 'SetRuntimeDialogTitle' }
    IDD_REPLACE = @{ File = 'src\fbe\SearchReplace.h'; Invocation = 'SetRuntimeDialogTitle' }
    IDD_SPELL_CHECK = @{ File = 'src\fbe\Speller.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SPELL_CHECK\)' }
    IDD_WORDS = @{ File = 'src\fbe\Words.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_WORDS\)' }
    IDD_SETTING_OTHER = @{ File = 'src\fbe\SettingsOtherDlg.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTING_OTHER\)' }
    IDD_SETTINGS_GENERAL = @{ File = 'src\fbe\SettingsGeneralPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_GENERAL\)' }
    IDD_SETTINGS_EDITOR = @{ File = 'src\fbe\SettingsEditorPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_EDITOR\)' }
    IDD_SETTINGS_SPELLING = @{ File = 'src\fbe\SettingsSpellingPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_SPELLING\)' }
    IDD_SETTINGS_SOURCE = @{ File = 'src\fbe\SettingsSourcePage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_SOURCE\)' }
    IDD_SETTINGS_ADVANCED = @{ File = 'src\fbe\SettingsAdvancedPage.cpp'; Invocation = 'FbeApplyRuntimeDialogLocalization\(m_hWnd,\s*IDD_SETTINGS_ADVANCED\)' }
}

foreach ($resource in $catalog.resources) {
    if (-not $consumers.ContainsKey($resource)) { throw "No runtime consumer is declared for $resource." }
    $consumer = $consumers[$resource]
    $consumerPath = Join-Path $repoRoot $consumer.File
    $consumerText = Get-Content -Raw -LiteralPath $consumerPath
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
    $consumerText = $consumers[$value.resource].Text
    $escapedKey = [regex]::Escape($key)
    $genericBindingPattern = '\{\s*' + [regex]::Escape($value.resource) + ',\s*[^,]+,\s*L"' + $escapedKey + '"\s*\}'
    $genericBinding = $bindingSource -match $genericBindingPattern
    if (-not $genericBinding -and -not $consumerText.Contains($key)) {
        throw "JSON dialog key has no binding in its concrete runtime consumer: $key ($($value.resource))."
    }
}

if (-not $bindingSource.Contains('FbeApplyRuntimeDialogLocalization')) {
    throw 'The generic runtime dialog binding is missing.'
}
Write-Host "Runtime dialog coverage verified: $(@($catalog.strings.PSObject.Properties).Count) catalog keys."
