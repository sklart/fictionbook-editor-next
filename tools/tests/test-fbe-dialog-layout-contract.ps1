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

foreach ($dialogId in @('IDD_TOOLS_SETTINGS', 'IDD_OPTIONS', 'IDD_SETTING_OTHER', 'IDD_HOTKEYS', 'IDD_SETTINGS_WORDS', 'IDD_SETTING_NEXT')) {
    $dialog = Get-Dialog $dialogId
    Assert-Contains $dialog 'FONT 8, "Tahoma", 400, 0, 0x1' "$dialogId must use Tahoma 8."
    if ($dialog -match 'DS_FIXEDSYS') { throw "$dialogId must not use DS_FIXEDSYS." }
}

Assert-Contains $rc 'IDD_SETTING_NEXT DIALOGEX 0, 0, 330, 320' 'Settings Next dimensions changed.'
Assert-Contains $rc 'IDC_UPDATE_CHANNEL,88,124,184,50' 'Update-channel selector geometry changed.'
Assert-Contains $rc 'IDD_TOOLS_SETTINGS DIALOGEX 0, 0, 340, 371' 'Settings container dimensions changed.'
Assert-Contains $rc 'IDC_TAB_CTRL,"SysTabControl32",0x0,0,0,339,342' 'Settings tab geometry changed.'
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
