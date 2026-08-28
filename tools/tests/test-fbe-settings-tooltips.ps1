[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$tooltips = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsTooltips.h')

foreach($contract in @(
	'm_tooltips\.Create\(owner\)',
	'ATLASSERT\(m_tooltips\.IsWindow\(\)\)',
	'm_tooltips\.Activate\(TRUE\)',
	'CToolInfo\s+toolInfo\(TTF_SUBCLASS,\s*control,\s*0,\s*NULL,\s*text\)',
	'm_tooltips\.AddTool\(&toolInfo\)'))
{
	if($tooltips -notmatch $contract) { throw "Missing Settings tooltip delivery contract: $contract" }
}

foreach($page in @(
	'SettingsGeneralPage.cpp', 'SettingsEditorPage.cpp', 'SettingsSourcePage.cpp',
	'SettingsImagesPage.cpp', 'SettingsSpellingPage.cpp', 'SettingsHotkeysDlg.cpp',
	'SettingsWordsDlg.cpp', 'SettingsAdvancedPage.cpp'))
{
	$pageText = Get-Content -Raw -LiteralPath (Join-Path $root (Join-Path 'src\fbe' $page))
	if($pageText -notmatch 'tooltips\.Add\(') { throw "Settings tooltip coverage is missing for $page" }
}

Write-Host 'Settings tooltip delivery contract passed.'
