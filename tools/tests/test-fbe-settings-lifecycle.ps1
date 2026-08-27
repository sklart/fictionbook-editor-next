[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$settingsHost = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsDlg.cpp')
foreach($text in @('m_pageLifecycle', '->Validate()', '->Commit()', '->CancelChanges()')) { if($settingsHost -notlike "*$text*") { throw "Missing lifecycle host contract: $text" } }
if($settingsHost -match 'MAKELONG\(IDOK|MAKELONG\(IDCANCEL|m_initial_scripts_folder|SetScriptsFolder') { throw 'SettingsDlg retains legacy page apply or scripts rollback.' }
foreach($path in @('SettingsGeneralPage.cpp','SettingsEditorPage.cpp','SettingsImagesPage.cpp','SettingsAdvancedPage.cpp','SettingsSpellingPage.cpp','SettingsSourcePage.cpp','SettingsHotkeysDlg.cpp','SettingsWordsDlg.cpp')) { $text = Get-Content -Raw -LiteralPath (Join-Path $root ('src\fbe\' + $path)); foreach($method in @('Validate','Commit','CancelChanges')) { if($text -notmatch ($method + '\s*\(')) { throw "$path lacks $method" } } }
Write-Host 'Settings lifecycle contract passed.'
