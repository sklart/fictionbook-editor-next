<#
.SYNOPSIS
Проверяет language-neutral layout-контракт базовых DIALOGEX FBE.

.DESCRIPTION
Runtime JSON меняет только тексты. Геометрия, шрифт и стабильные ID остаются
в английских шаблонах src/fbe/FBE.rc, поэтому эти инварианты не должны были
исчезнуть вместе с legacy RU/UK generated RC2-файлами.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$rcPath = Join-Path $repoRoot 'src\fbe\FBE.rc'
$resourceHeaderPath = Join-Path $repoRoot 'src\fbe\resource.h'
$rc = (New-Object Text.UTF8Encoding($false, $true)).GetString([IO.File]::ReadAllBytes($rcPath))
$resourceHeader = Get-Content -Raw -LiteralPath $resourceHeaderPath

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) { throw "Layout contract failed: $Description" }
}

function Get-Dialog([string]$DialogId) {
    $dialog = [regex]::Match($rc, "(?ms)^$DialogId DIALOGEX.*?^END")
    if (-not $dialog.Success) { throw "В базовом FBE.rc не найден диалог $DialogId." }
    return $dialog.Value
}

Assert-Contains $rc 'COMBOBOX\s+IDC_LANG,\d+,\d+,1[0-9]{2},\d+' 'IDC_LANG must remain wide enough for translated locale names.'

foreach ($dialogId in @('IDD_TOOLS_SETTINGS', 'IDD_HOTKEYS', 'IDD_SETTINGS_WORDS', 'IDD_SETTINGS_GENERAL', 'IDD_SETTINGS_SOURCE', 'IDD_SETTINGS_IMAGES', 'IDD_SETTINGS_ADVANCED')) {
    $dialog = Get-Dialog $dialogId
    Assert-Contains $dialog 'FONT 8, "MS Shell Dlg 2", 400, 0, 0x1' "$dialogId must use the Windows dialog font alias."
    if ($dialog -match 'DS_FIXEDSYS') { throw "$dialogId must not use DS_FIXEDSYS." }
}

if ($rc -match '"Tahoma"') { throw 'FBE resource dialogs must not hard-code Tahoma.' }
if ($rc -match 'DS_FIXEDSYS') { throw 'FBE resource dialogs must use the Windows dialog font, not a fixed system font.' }

$sourceDialog = Get-Dialog 'IDD_SETTINGS_SOURCE'
$generalDialog = Get-Dialog 'IDD_SETTINGS_GENERAL'
$advancedDialog = Get-Dialog 'IDD_SETTINGS_ADVANCED'
$hotkeysDialog = Get-Dialog 'IDD_HOTKEYS'
$imagesDialog = Get-Dialog 'IDD_SETTINGS_IMAGES'
$wordsDialog = Get-Dialog 'IDD_SETTINGS_WORDS'
$sourceSize = [regex]::Match($sourceDialog, 'IDD_SETTINGS_SOURCE DIALOGEX 0, 0, (?<width>\d+), (?<height>\d+)')
if (-not $sourceSize.Success -or [int]$sourceSize.Groups['width'].Value -lt 300) {
    throw 'Layout contract failed: Source page must retain at least 300 dialog units for localized labels and preview.'
}
$preview = [regex]::Match($sourceDialog, 'CONTROL\s+"",IDC_OPTIONS_SOURCE_PREVIEW,"Static",[^\r\n]*?,(?<x>\d+),(?<y>\d+),(?<w>\d+),(?<h>\d+),WS_EX_CLIENTEDGE')
if (-not $preview.Success -or ([int]$preview.Groups['x'].Value + [int]$preview.Groups['w'].Value) -gt [int]$sourceSize.Groups['width'].Value -or
    ([int]$preview.Groups['y'].Value + [int]$preview.Groups['h'].Value) -gt [int]$sourceSize.Groups['height'].Value) {
    throw 'Layout contract failed: Source preview must fit inside its page.'
}
$settingsLayout = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\settings\ui\SettingsDlg.cpp')
Assert-Contains $settingsLayout 'GWL_STYLE\) \| WS_VSCROLL \| WS_HSCROLL' 'Long Settings pages must remain scrollable inside the window.'
foreach ($control in @('IDC_WRAP', 'IDC_SYNTAXHL', 'IDC_TAGHL', 'IDC_SHOWEOL',
    'IDC_SHOWWHITESPACE', 'IDC_SHOWLINENUMBERS', 'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS',
    'IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE', 'IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_MODE',
    'IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ATTRIBUTES', 'IDC_OPTIONS_SOURCE_TAG_HIGHLIGHT_ERRORS',
    'IDC_EDITOR_CARET_GROUP', 'IDC_OPTIONS_SOURCE_PREVIEW_GROUP', 'IDC_OPTIONS_SOURCE_PREVIEW')) {
    Assert-Contains $sourceDialog $control "Source control missing: $control"
}
if ($sourceDialog -match 'SS_OWNERDRAW') { throw 'Source preview must be a live Scintilla host.' }
$editorDialog = Get-Dialog 'IDD_SETTINGS_EDITOR'
foreach ($control in @('IDC_OPTIONS_BODY_GROUP', 'IDC_EDITOR_BACKGROUND_GROUP',
    'IDC_EDITOR_PREVIEW_GROUP', 'IDC_EDITOR_BACKGROUND_PREVIEW', 'IDC_NBSP_CHAR')) {
    Assert-Contains $editorDialog $control "Editor control missing: $control"
}
if ($editorDialog -match 'IDC_EDITOR_CARET_GROUP') { throw 'Caret controls must be on the Source page.' }
foreach ($control in @('IDC_SRCFONT', 'IDC_WRAP', 'IDC_SYNTAXHL', 'IDC_TAGHL', 'IDC_SHOWEOL', 'IDC_SHOWWHITESPACE', 'IDC_SHOWLINENUMBERS', 'IDC_OPTIONS_SOURCE_PALETTE', 'IDC_OPTIONS_SOURCE_PREVIEW')) {
    Assert-Contains $sourceDialog $control "Source control missing from IDD_SETTINGS_SOURCE: $control"
}
foreach ($control in @('IDC_CREATE_BACKUP_FILE', 'IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE', 'IDC_UPDATE_CHANNEL')) {
    if ($sourceDialog -match $control) { throw "General control must not be in IDD_SETTINGS_SOURCE: $control" }
}
foreach ($control in @('IDC_DEFAULT_SCRIPTS_FOLDER', 'IDC_SCRIPTS_FOLDER_PATH', 'IDC_SELECT_SCRIPTS_FOLDER_BUTTON', 'IDC_FAST_MODE', 'IDC_CHECK_SCRIPTS_ON_STARTUP')) {
    Assert-Contains $advancedDialog $control "Advanced control missing: $control"
    if ($generalDialog -match $control) { throw "Advanced control must not be in General: $control" }
}
Assert-Contains $generalDialog 'IDC_RESTORE_POS,"Button".*?,14,83,268,10' 'Restore-position checkbox must fit localized text.'
Assert-Contains $advancedDialog 'IDC_DEFAULT_SCRIPTS_FOLDER,"Button".*?,14,19,278,15' 'Default-scripts-folder checkbox must fit localized text.'
foreach ($control in @('IDC_SETTINGS_OTHER_KEYBOARD', 'IDC_CHANGE_KEYB', 'IDC_SETTINGS_OTHER_CHANGE_TO', 'IDC_KEYB_LAYOUT')) {
    Assert-Contains $hotkeysDialog $control "Keyboard-layout control missing from IDD_HOTKEYS: $control"
    if ($imagesDialog -match $control) { throw "Keyboard-layout control must not be in IDD_SETTINGS_IMAGES: $control" }
}
foreach ($control in @('IDC_SETTINGS_ASKIMAGE', 'IDC_OPTIONS_CLEARIMGS', 'IDC_SETTINGS_OTHER_PASTE', 'IDC_SETTINGS_OTHER_FORMAT', 'IDC_IMAGETYPE', 'IDC_SETTINGS_OTHER_QUALITY', 'IDC_JPEGQUALITY', 'IDC_JPEGSPIN', 'IDC_SETTINGS_OTHER_IMPORT', 'IDC_SETTINGS_OTHER_OUTPUT', 'IDC_IMAGE_IMPORT_FORMAT', 'IDC_SETTINGS_OTHER_IMPORT_QUALITY', 'IDC_IMAGE_IMPORT_JPEG_QUALITY', 'IDC_IMAGE_IMPORT_JPEG_SPIN', 'IDC_IMAGE_IMPORT_KEEP_SUPPORTED')) {
    Assert-Contains $imagesDialog $control "Image control missing from IDD_SETTINGS_IMAGES: $control"
}
foreach ($control in @('IDC_KEEP', 'IDC_DEFAULT_ENC', 'IDC_RESTORE_POS', 'IDC_SETTINGS_OTHER_NBSP', 'IDC_SETTINGS_OTHER_NBSP_LABEL', 'IDC_NBSP_CHAR')) {
    if ($imagesDialog -match $control) { throw "General or Editor control must not be in IDD_SETTINGS_IMAGES: $control" }
}
if ($rc -match ('(?m)^IDD_SETTING' + '_OTHER DIALOGEX')) { throw 'Legacy Other dialog must be removed.' }
$comboGrid = @('IDC_LANG', 'IDC_GENRE_CATALOG', 'IDC_INTERFACE_THEME', 'IDC_DEFAULT_ENC', 'IDC_UPDATE_CHANNEL') | ForEach-Object {
    $control = $_
    $match = [regex]::Match($generalDialog, "(?m)^\s*COMBOBOX\s+$control,(\d+),\d+,(\d+),\d+")
    if (-not $match.Success) { throw "General ComboBox missing: $control" }
    [pscustomobject]@{ Id = $control; Left = [int]$match.Groups[1].Value; Right = [int]$match.Groups[1].Value + [int]$match.Groups[2].Value }
}
if (($comboGrid.Left | Select-Object -Unique).Count -ne 1 -or
    ($comboGrid.Right | Select-Object -Unique).Count -ne 1) {
    throw 'General ComboBox controls must share one left and right grid.'
}
Assert-Contains $wordsDialog 'IDD_SETTINGS_WORDS DIALOGEX 0, 0, 300, 227' 'Words page must use the Settings content width.'
Assert-Contains $wordsDialog 'IDC_STATIC_WORDS_NEW_WORD,18,162,90,16' 'Words new-word label must stay aligned with the input row.'
Assert-Contains $wordsDialog 'IDC_EDIT_NEW,112,160,104,14' 'Words input must follow its label on the same row.'
Assert-Contains $wordsDialog 'IDC_BUTTON_ADD,222,160,60,14' 'Words Add button must share the new-word row.'
Assert-Contains $wordsDialog 'IDC_CHECK_SELALL,"Button".*?,19,142,95,13' 'Words Select all checkbox must fit its localized caption.'
Assert-Contains $hotkeysDialog 'IDD_HOTKEYS DIALOGEX 0, 0, 300, 292' 'Hotkeys page must use the Settings content width and fit the export action.'
Assert-Contains $hotkeysDialog 'IDC_LIST_HOTKEYS,99,21,194,88' 'Hotkeys command list must use the available page width.'
Assert-Contains $hotkeysDialog 'IDC_KEYB_LAYOUT,92,245,194,30' 'Keyboard-layout combo must use the available page width.'
Assert-Contains $hotkeysDialog 'IDC_BUTTON_HOTKEY_EXPORT,227,270,59,14' 'Hotkeys export action must remain inside the Settings content area.'
foreach ($control in @('IDC_CREATE_BACKUP_FILE', 'IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE', 'IDC_UPDATE_CHANNEL')) {
    Assert-Contains $generalDialog $control "General control missing from IDD_SETTINGS_GENERAL: $control"
}
Assert-Contains $rc 'IDD_TOOLS_SETTINGS DIALOGEX 0, 0, 430, 360' 'Settings container dimensions changed.'
Assert-Contains $rc 'IDC_SETTINGS_NAV,10,10,100,298' 'Settings navigation geometry changed.'
Assert-Contains $rc 'IDD_ABOUTBOX DIALOGEX 0, 0, 420, 165' 'About dimensions changed.'
Assert-Contains $rc 'LTEXT           "Build",IDC_STATIC_BUILD,101,21,50,12' 'About build label must fit localized text without overlapping the name.'
Assert-Contains $rc 'EDITTEXT        IDC_CONTRIBS,70,51,343,91' 'About contributors area must use increased height.'
Assert-Contains $rc 'IDC_CONTRIBS,70,51,343,91,ES_MULTILINE[^\r\n]*\| WS_BORDER \|' 'About contributors frame must have a visible border.'
Assert-Contains $rc 'IDC_TEXT_STATUS,"Static",SS_LEFT \| WS_GROUP,24,147,210,20' 'About status geometry changed.'
Assert-Contains $rc 'IDC_WHATS_NEW,240,150,70,14' 'About What''s New button geometry changed.'
Assert-Contains $rc 'IDC_UPDATE,315,150,49,14' 'About update button geometry changed.'

foreach ($control in @(
    'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS\s+1145',
    'IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE\s+1153',
    'IDC_IMAGE_IMPORT_FORMAT\s+1104',
    'IDC_IMAGE_IMPORT_JPEG_QUALITY\s+1105',
    'IDC_IMAGE_IMPORT_JPEG_SPIN\s+1106',
    'IDC_IMAGE_IMPORT_KEEP_SUPPORTED\s+1107',
    'IDC_UPDATE_CHANNEL\s+1156',
    'IDC_FBE_NEXT_UPDATES_GROUP\s+1157',
    'IDC_UPDATE_CHANNEL_LABEL\s+1158',
    'IDC_WHATS_NEW\s+1159'
)) {
    Assert-Contains $resourceHeader ('#define\s+' + $control) "Stable control ID missing: $control"
}

Write-Host 'FBE base dialog layout contract passed.'
