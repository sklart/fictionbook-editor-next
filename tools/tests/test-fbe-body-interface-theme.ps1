param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)

$ErrorActionPreference = 'Stop'
$doc = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\FBDoc.cpp')
$backgrounds = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\settings\EditorBackgrounds.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\mainfrm.cpp')
$workflow = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot '.github\workflows\build.yml')

foreach($required in @('ResolveBodyEditorColors', 'EditorBackgrounds::ResolveBodyColors', 'IsHighContrastEnabled()')) {
    if($doc -notlike "*$required*") { throw "BODY theme resolution is missing $required." }
}

$apply = [regex]::Match($doc, 'void\s+Doc::ApplyConfChanges\(\)\s*\{[\s\S]*?\n\}')
if(!$apply.Success) { throw 'Doc::ApplyConfChanges was not found.' }
foreach($required in @('ApplyEditorBackground(hs)', 'ApplyThemeAppearance()')) {
    if($apply.Value -notlike "*$required*") { throw "BODY ApplyConfChanges is missing $required." }
}
if($apply.Value -match 'hs->(color|backgroundColor)\s*=') { throw 'Theme colours must not mutate the editable BODY style.' }
$runtimeColors = [regex]::Match($doc, 'static void ApplyRuntimeBodyColors\(MSHTML::IHTMLDocument2Ptr document\)\s*\{[\s\S]*?\n\}')
if(!$runtimeColors.Success -or $runtimeColors.Value -notlike '*fbe-runtime-body-colors*' -or $runtimeColors.Value -notlike '*sheet->cssText*') {
    throw 'Runtime BODY colour sheet is missing.'
}
foreach($required in @('fallbackColor', 'recommendedTextColor', 'ParseCssColor', 'GetBuiltInRecommendedColors')) {
    if($backgrounds -notlike "*$required*") { throw "Built-in background metadata is not consumed: $required." }
}

$resolver = [regex]::Match($backgrounds, 'EditorBackgroundColors\s+EditorBackgrounds::ResolveBodyColors\([\s\S]*?\n\}')
if(!$resolver.Success) { throw 'Shared BODY colour resolver was not found.' }
foreach($required in @(
    'configuredForeground == CLR_DEFAULT ? (dark ? ThemeManager::TextColor() : ::GetSysColor(COLOR_WINDOWTEXT)) : static_cast<COLORREF>(configuredForeground)',
    'configuredBackground == CLR_DEFAULT ? (dark ? ThemeManager::WindowColor() : ::GetSysColor(COLOR_WINDOW)) : static_cast<COLORREF>(configuredBackground)',
    'if(backgroundKind == L"builtin")', # built-in background
    'if(highContrast)', # High Contrast affects only effective colors
    'GetSysColor(COLOR_WINDOWTEXT), ::GetSysColor(COLOR_WINDOW)'
)) {
    if($resolver.Value -notlike "*$required*") { throw "BODY scenario is not protected: $required." }
}
if($workflow -notlike '*./tools/tests/test-fbe-body-interface-theme.ps1*') {
    throw 'BODY interface-theme test is not executed by CI.'
}

$themeHandler = [regex]::Match($frame, 'LRESULT\s+CMainFrame::OnThemeChanged[\s\S]*?\n\}')
if(!$themeHandler.Success -or $themeHandler.Value -notlike '*m_doc->ApplyThemeAppearance()*') {
    throw 'Open BODY editor is not reapplied after an interface theme change.'
}
Write-Host 'Контракт BODY Editor для интерфейсных тем прошёл проверку.'
