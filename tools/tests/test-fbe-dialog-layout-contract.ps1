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
$rc = [Text.Encoding]::GetEncoding(1251).GetString([IO.File]::ReadAllBytes($rcPath))
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
    Assert-Contains $dialog 'FONT 8, "Tahoma", 400, 0, 0x1' "$dialogId must use Tahoma 8."
    if ($dialog -match 'DS_FIXEDSYS') { throw "$dialogId must not use DS_FIXEDSYS." }
}

$sourceDialog = Get-Dialog 'IDD_SETTINGS_SOURCE'
$generalDialog = Get-Dialog 'IDD_SETTINGS_GENERAL'
$advancedDialog = Get-Dialog 'IDD_SETTINGS_ADVANCED'
$hotkeysDialog = Get-Dialog 'IDD_HOTKEYS'
$imagesDialog = Get-Dialog 'IDD_SETTINGS_IMAGES'
$wordsDialog = Get-Dialog 'IDD_SETTINGS_WORDS'
Assert-Contains $sourceDialog 'IDD_SETTINGS_SOURCE DIALOGEX 0, 0, 300, 320' 'Source page must fit the Settings content area without exceeding it.'
foreach ($control in @(
    'IDC_WRAP,"Button".*?,14,38,266,10', 'IDC_SYNTAXHL,"Button".*?,14,52,266,10',
    'IDC_TAGHL,"Button".*?,14,66,266,10', 'IDC_SHOWEOL,"Button".*?,14,80,266,10',
    'IDC_SHOWWHITESPACE,"Button".*?,14,94,266,10', 'IDC_SHOWLINENUMBERS,"Button".*?,14,108,266,10',
    'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS,"Button".*?,14,122,266,10',
    'IDC_OPTIONS_SOURCE_SPECIAL_CHARS_STYLE,64,134,128,50',
    'IDC_OPTIONS_SOURCE_PREVIEW,"Static",SS_OWNERDRAW,14,257,272,56'
)) {
    Assert-Contains $sourceDialog $control 'Source checkboxes must use full-width independent rows.'
}
foreach ($control in @('IDC_SRCFONT', 'IDC_WRAP', 'IDC_SYNTAXHL', 'IDC_TAGHL', 'IDC_SHOWEOL', 'IDC_SHOWWHITESPACE', 'IDC_SHOWLINENUMBERS', 'IDC_OPTIONS_SOURCE_PALETTE', 'IDC_OPTIONS_SOURCE_PREVIEW')) {
    Assert-Contains $sourceDialog $control "Source control missing from IDD_SETTINGS_SOURCE: $control"
}
foreach ($control in @('IDC_CREATE_BACKUP_FILE', 'IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE', 'IDC_UPDATE_CHANNEL')) {
    if ($sourceDialog -match $control) { throw "General control must not be in IDD_SETTINGS_SOURCE: $control" }
}
foreach ($control in @('IDC_DEFAULT_SCRIPTS_FOLDER', 'IDC_SCRIPTS_FOLDER_PATH', 'IDC_SELECT_SCRIPTS_FOLDER_BUTTON', 'IDC_FAST_MODE')) {
    Assert-Contains $advancedDialog $control "Advanced control missing: $control"
    if ($generalDialog -match $control) { throw "Advanced control must not be in General: $control" }
}
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
Assert-Contains $generalDialog 'IDC_UPDATE_CHANNEL,82,223,180,55' 'Update-channel selector geometry changed in IDD_SETTINGS_GENERAL.'
Assert-Contains $wordsDialog 'IDD_SETTINGS_WORDS DIALOGEX 0, 0, 300, 227' 'Words page must use the Settings content width.'
Assert-Contains $wordsDialog 'IDC_STATIC_WORDS_NEW_WORD,18,162,65,9' 'Words new-word label must stay aligned with the input row.'
Assert-Contains $wordsDialog 'IDC_EDIT_NEW,86,160,130,14' 'Words input must follow its label on the same row.'
Assert-Contains $wordsDialog 'IDC_BUTTON_ADD,222,160,60,14' 'Words Add button must share the new-word row.'
Assert-Contains $wordsDialog 'IDC_CHECK_SELALL,"Button".*?,19,142,70,10' 'Words Select all checkbox must fit its localized caption.'
Assert-Contains $hotkeysDialog 'IDD_HOTKEYS DIALOGEX 0, 0, 300, 275' 'Hotkeys page must use the Settings content width.'
Assert-Contains $hotkeysDialog 'IDC_LIST_HOTKEYS,99,21,194,88' 'Hotkeys command list must use the available page width.'
Assert-Contains $hotkeysDialog 'IDC_KEYB_LAYOUT,92,245,194,30' 'Keyboard-layout combo must use the available page width.'
foreach ($control in @('IDC_CREATE_BACKUP_FILE', 'IDC_SHOW_FULL_PATH_IN_WINDOW_TITLE', 'IDC_UPDATE_CHANNEL')) {
    Assert-Contains $generalDialog $control "General control missing from IDD_SETTINGS_GENERAL: $control"
}
Assert-Contains $rc 'IDD_TOOLS_SETTINGS DIALOGEX 0, 0, 430, 360' 'Settings container dimensions changed.'
Assert-Contains $rc 'IDC_SETTINGS_NAV,10,10,100,298' 'Settings navigation geometry changed.'
Assert-Contains $rc 'IDD_ABOUTBOX DIALOGEX 0, 0, 420, 165' 'About dimensions changed.'
Assert-Contains $rc 'LTEXT           "Build",IDC_STATIC_BUILD,101,20,40,8' 'About build label must fit localized text.'
Assert-Contains $rc 'EDITTEXT        IDC_CONTRIBS,70,51,343,91' 'About contributors area must use increased height.'
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
