[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$tooltips = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SettingsTooltips.h')

foreach($contract in @(
	'm_tooltips\.Create\(owner\)',
	'ATLASSERT\(m_tooltips\.IsWindow\(\)\)',
	'm_tooltips\.Activate\(TRUE\)',
	'CToolInfo\s+toolInfo\(TTF_SUBCLASS,\s*control,\s*0,\s*NULL,\s*text\.GetBuffer\(\)\)',
	'm_tooltips\.AddTool\(&toolInfo\)'))
{
	if($tooltips -notmatch $contract) { throw "Missing Settings tooltip delivery contract: $contract" }
}

foreach($pageContract in @{
	'SettingsGeneralPage.cpp' = @('general.backup', 'general.full_path')
	'SettingsEditorPage.cpp' = @('editor.font')
	'SettingsSourcePage.cpp' = @('source.font', 'source.wrap', 'source.syntax', 'source.eol', 'source.whitespace', 'source.line_numbers')
	'SettingsImagesPage.cpp' = @('images.paste_format', 'images.paste_quality')
	'SettingsSpellingPage.cpp' = @('spelling.custom_dictionary')
	'SettingsHotkeysDlg.cpp' = @('hotkeys.layout')
	'SettingsWordsDlg.cpp' = @('words.list', 'words.new')
	'SettingsAdvancedPage.cpp' = @('advanced.scripts_folder', 'advanced.fast_mode')
}.GetEnumerator())
{
	$pageText = Get-Content -Raw -LiteralPath (Join-Path $root (Join-Path 'src\fbe' $pageContract.Key))
	foreach($key in $pageContract.Value)
	{
		if($pageText -notmatch [regex]::Escape($key)) { throw "Settings tooltip registration is missing for $($pageContract.Key): $key" }
	}
}

Write-Host 'Settings tooltip delivery contract passed.'
