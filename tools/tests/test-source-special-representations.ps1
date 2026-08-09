[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Get-Text([string]$RelativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $repoRoot $RelativePath)
}

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        throw "Missing $Description."
    }
}

$settingsHeader = Get-Text 'src\fbe\Settings.h'
$settings = Get-Text 'src\fbe\Settings.cpp'
$mainFrame = Get-Text 'src\fbe\mainfrm.cpp'
$nextDialog = Get-Text 'src\fbe\SettingsNextDlg.cpp'
$resourceHeader = Get-Text 'src\fbe\resource.h'
$catalog = Get-Text 'localization\app-ui\fbe-small-dialogs.json'

Assert-Contains $settingsHeader 'XmlSrcShowSpecialChars\s*\(\)const' 'persisted special-character visibility getter'
Assert-Contains $settingsHeader 'SetXmlSrcShowSpecialChars' 'persisted special-character visibility setter'
Assert-Contains $settings 'XMLSrcShowSpecialChars' 'special-character settings key'
Assert-Contains $settings 'm_xml_src_showSpecialChars\s*=\s*false' 'disabled-by-default special-character setting'
Assert-Contains $mainFrame 'ConfigureSourceSpecialCharacterRepresentations\s*\(\)' 'representation configuration hook'
Assert-Contains $mainFrame 'SCI_SETREPRESENTATION' 'Scintilla representation setup'
Assert-Contains $mainFrame 'SCI_CLEARREPRESENTATION' 'Scintilla representation cleanup'
Assert-Contains $mainFrame '"NBSP"' 'NBSP representation'
Assert-Contains $mainFrame '"SHY"' 'soft-hyphen representation'
Assert-Contains $mainFrame '"ZWSP"' 'zero-width-space representation'
Assert-Contains $resourceHeader 'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS' 'FBE Next special-character control id'
Assert-Contains $nextDialog 'IDC_OPTIONS_SOURCE_SHOW_SPECIAL_CHARS' 'FBE Next special-character control use'
Assert-Contains $nextDialog 'SetXmlSrcShowSpecialChars' 'FBE Next special-character setting save'
Assert-Contains $catalog 'fbe\.dialog\.idd_setting_next\.show_special_characters' 'localized FBE Next special-character label'

Write-Host 'Source special-character representation contract passed.'
