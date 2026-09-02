[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Read([string]$p) { Get-Content -Raw -LiteralPath (Join-Path $root $p) }
$spelling = Read 'src\fbe\SettingsSpellingPage.cpp'
if ($spelling -match 'OPENFILENAME|GetOpenFileName') { throw 'Spelling dictionary picker must use ModernFileDialog.' }
if ($spelling -notmatch 'ModernFileDialog::Request') { throw 'Spelling picker request is missing.' }
$image = Read 'src\fbe\FBEview.cpp'
if ($image -notmatch 'fbe\.image\.open_button') { throw 'Image picker must localize its Open button.' }
$rc = Read 'src\fbe\FBE.rc'
if ($rc -notmatch 'IDD_ADDIMAGE DIALOGEX 0, 0, 220, 78') { throw 'Empty-image dialog must have a wider layout.' }
$catalog = Read 'localization\app-ui\catalog.json'
$smallDialogs = Read 'localization\app-ui\fbe-small-dialogs.json'
if ($catalog -notmatch '"fbe\.image\.open_button"') { throw 'Image Open localization key is missing.' }
$hotkeys = Read 'src\fbe\SettingsHotkeysDlg.cpp'
$settings = Read 'src\fbe\Settings.cpp'
if ($hotkeys -match 'CompareNoCase\(L"Add to dictionary"|CompareNoCase\(L"Ignore All"') { throw 'Hotkey display names must not depend on registration-name comparisons.' }
if ($settings -notmatch 'ToolsSpellAddToDict\(L"Add to dictionary",\s*IDS_HOTKEY_TOOLS_ADD_TO_DICTIONARY' -or $settings -notmatch 'ToolsSpellIgnore\(L"Ignore",\s*IDS_HOTKEY_TOOLS_IGNORE_ALL') { throw 'Spelling hotkeys must have dedicated localization resource IDs.' }
if (($catalog + $smallDialogs) -notmatch 'fbe\.spelling\.menu\.add_to_dictionary' -or ($catalog + $smallDialogs) -notmatch 'fbe\.dialog\.idd_spell_check\.ignore_all') { throw 'Spelling hotkey localization keys are missing.' }
Write-Host 'Modern dialog UI localization contract passed.'
