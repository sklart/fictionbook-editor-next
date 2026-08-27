[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$settingsHost = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsDlg.cpp')
foreach($text in @('m_pageLifecycle', '->Validate()', '->Commit()', '->CancelChanges()')) { if($settingsHost -notlike "*$text*") { throw "Missing lifecycle host contract: $text" } }
if($settingsHost -match 'MAKELONG\(IDOK|MAKELONG\(IDCANCEL|m_initial_scripts_folder|SetScriptsFolder') { throw 'SettingsDlg retains legacy page apply or scripts rollback.' }
$validateLoop = $settingsHost.IndexOf('->Validate()'); $commitLoop = $settingsHost.IndexOf('->Commit()'); $okEndDialog = $settingsHost.IndexOf('EndDialog(IDOK)')
if($validateLoop -lt 0 -or $commitLoop -lt $validateLoop -or $okEndDialog -lt $commitLoop) { throw 'Settings transaction order is not Validate ALL, Commit ALL, EndDialog.' }
if($settingsHost -notmatch '(?s)!?m_pageLifecycle\[i\]->Validate\(\).*?SelectPage\(.*?return 0' -or $settingsHost -notmatch '(?s)for\s*\([^)]*\).*?Validate\(.*?\}\s*for\s*\([^)]*\).*?Commit\(\)') { throw 'Settings validation failure no longer selects the page before any commit.' }
if($settingsHost -notmatch '(?s)hWndCtl\s*!=\s*globalOk.*?m_currentPage\s*==\s*SettingsPageId::Words.*?m_wordsPage->HandleDefaultAction\(\).*?return 0;\s*for\s*\([^)]*\).*?Validate\(\)') { throw 'Words local default action must return before the global transaction.' }
foreach($path in @('SettingsGeneralPage.cpp','SettingsEditorPage.cpp','SettingsImagesPage.cpp','SettingsAdvancedPage.cpp','SettingsSpellingPage.cpp','SettingsSourcePage.cpp','SettingsHotkeysDlg.cpp','SettingsWordsDlg.cpp')) { $text = Get-Content -Raw -LiteralPath (Join-Path $root ('src\fbe\' + $path)); foreach($method in @('Validate','Commit','CancelChanges')) { if($text -notmatch ($method + '\s*\(')) { throw "$path lacks $method" } } }
$words = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsWordsDlg.cpp')
$commit = [regex]::Match($words, 'void\s+CSettingsWordsDlg::Commit\s*\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
$validate = [regex]::Match($words, 'bool\s+CSettingsWordsDlg::Validate\s*\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if(-not $commit.Success -or $commit.Groups['body'].Value -match 'GetFocus\s*\(') { throw 'Words Commit must not depend on focus.' }
foreach($text in @('SetShowWordsExcls', '_Settings.m_words', 'SaveWords')) { if($commit.Groups['body'].Value -notlike "*$text*") { throw "Words Commit lacks $text." } }
if(-not $validate.Success -or $validate.Groups['body'].Value -notmatch 'FinishInlineEdit\(\)' -or $validate.Groups['body'].Value -notmatch 'FinishNewWord\(\)' -or $validate.Groups['body'].Value -match 'SaveWords|_Settings\.m_words') { throw 'Words Validate must finish staged edits without persisting words.' }
$defaultAction = [regex]::Match($words, 'bool\s+CSettingsWordsDlg::HandleDefaultAction\s*\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
$cancelChanges = [regex]::Match($words, 'bool\s+CSettingsWordsDlg::CancelChanges\s*\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if(-not $defaultAction.Success -or $words -notmatch '(?s)LRESULT\s+CSettingsWordsDlg::OnOK.*?HandleDefaultAction\(\)') { throw 'Words must keep Enter as a local default action.' }
if($defaultAction.Groups['body'].Value -match 'SaveWords|_Settings\.m_words|Commit\s*\(') { throw 'Words local default action must not persist settings.' }
if(-not $cancelChanges.Success -or $cancelChanges.Groups['body'].Value -notmatch '(?s)m_editActive.*?GetFocus\s*\(\)\s*==\s*m_edit.*?return false') { throw 'Words CancelChanges must preserve the inline-editor Esc veto.' }
$advanced = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsAdvancedPage.cpp')
if($advanced -notmatch 'm_initialScriptsFolder\s*=\s*_Settings\.GetScriptsFolder\(\)' -or $advanced -notmatch 'm_initialScriptsFolder\s*!=\s*_Settings\.GetScriptsFolder\(\)') { throw 'Advanced page must own its scripts-folder initial snapshot.' }
if($advanced -match 'm_initial_scripts_folder') { throw 'Advanced page still uses the global scripts-folder snapshot.' }
Write-Host 'Settings lifecycle contract passed.'
