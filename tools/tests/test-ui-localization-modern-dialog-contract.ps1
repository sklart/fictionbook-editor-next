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
if ($catalog -notmatch '"fbe\.image\.open_button"') { throw 'Image Open localization key is missing.' }
Write-Host 'Modern dialog UI localization contract passed.'
