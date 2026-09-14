param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)

$ErrorActionPreference = 'Stop'
$doc = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\FBDoc.cpp')
$backgrounds = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\settings\EditorBackgrounds.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'src\fbe\mainfrm.cpp')
$workflow = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot '.github\workflows\build.yml')

foreach($required in @('ResolveBodyEditorColors', 'ThemeManager::TextColor()', 'ThemeManager::WindowColor()', 'IsHighContrastEnabled()', 'backgroundKind == L"none"', 'backgroundKind == L"builtin"', 'GetBuiltInRecommendedColors')) {
    if($doc -notlike "*$required*") { throw "BODY theme resolution is missing $required." }
}

$apply = [regex]::Match($doc, 'void\s+Doc::ApplyConfChanges\(\)\s*\{[\s\S]*?\n\}')
if(!$apply.Success) { throw 'Doc::ApplyConfChanges was not found.' }
foreach($required in @('const BodyEditorColors colors = ResolveBodyEditorColors()', 'fs = colors.foreground', 'fs = colors.background', 'ApplyEditorBackground(hs)')) {
    if($apply.Value -notlike "*$required*") { throw "BODY ApplyConfChanges is missing $required." }
}
foreach($required in @('fallbackColor', 'recommendedTextColor', 'ParseCssColor', 'GetBuiltInRecommendedColors')) {
    if($backgrounds -notlike "*$required*") { throw "Built-in background metadata is not consumed: $required." }
}

$resolver = [regex]::Match($doc, 'static\s+BodyEditorColors\s+ResolveBodyEditorColors\(\)\s*\{[\s\S]*?\n\}')
if(!$resolver.Success) { throw 'BODY colour resolver was not found.' }
foreach($required in @(
    'configuredForeground == CLR_DEFAULT && configuredBackground == CLR_DEFAULT && ThemeManager::IsDark()', # Default FG + Default BG
    'configuredForeground == CLR_DEFAULT ? static_cast<DWORD>(::GetSysColor(COLOR_WINDOWTEXT)) : configuredForeground', # custom FG
    'configuredBackground == CLR_DEFAULT ? static_cast<DWORD>(::GetSysColor(COLOR_WINDOW)) : configuredBackground', # custom BG
    'if(backgroundKind == L"builtin")', # built-in background
    'Custom images, including an unavailable file selected by the user, never', # custom background
    'if(IsHighContrastEnabled()) return colors' # High Contrast
)) {
    if($resolver.Value -notlike "*$required*") { throw "BODY scenario is not protected: $required." }
}
if($workflow -notlike '*./tools/tests/test-fbe-body-interface-theme.ps1*') {
    throw 'BODY interface-theme test is not executed by CI.'
}

$themeHandler = [regex]::Match($frame, 'LRESULT\s+CMainFrame::OnThemeChanged[\s\S]*?\n\}')
if(!$themeHandler.Success -or $themeHandler.Value -notlike '*m_doc->ApplyConfChanges()*') {
    throw 'Open BODY editor is not reapplied after an interface theme change.'
}
Write-Host 'Контракт BODY Editor для интерфейсных тем прошёл проверку.'
