param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)

$ErrorActionPreference = 'Stop'
$doc = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\FBDoc.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\mainfrm.cpp')

foreach($required in @('ResolveBodyEditorColor', 'ThemeManager::TextColor()', 'ThemeManager::WindowColor()', 'IsHighContrastEnabled()')) {
    if($doc -notlike "*$required*") { throw "BODY theme resolution is missing $required." }
}

$apply = [regex]::Match($doc, 'void\s+Doc::ApplyConfChanges\(\)\s*\{[\s\S]*?\n\}')
if(!$apply.Success) { throw 'Doc::ApplyConfChanges was not found.' }
foreach($required in @('ResolveBodyEditorColor(_Settings.GetColorFG(), COLOR_WINDOWTEXT, ThemeManager::TextColor())', 'ResolveBodyEditorColor(_Settings.GetColorBG(), COLOR_WINDOW, ThemeManager::WindowColor())', 'ApplyEditorBackground(hs)')) {
    if($apply.Value -notlike "*$required*") { throw "BODY ApplyConfChanges is missing $required." }
}

$themeHandler = [regex]::Match($frame, 'LRESULT\s+CMainFrame::OnThemeChanged[\s\S]*?\n\}')
if(!$themeHandler.Success -or $themeHandler.Value -notlike '*m_doc->ApplyConfChanges()*') {
    throw 'Open BODY editor is not reapplied after an interface theme change.'
}
Write-Host 'Контракт BODY Editor для интерфейсных тем прошёл проверку.'
