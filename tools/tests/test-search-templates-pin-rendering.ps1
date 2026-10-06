<# Guards the procedural Templates thumbtack and its state contract. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
foreach($token in @('void DrawPresetPinGlyph', 'const int capWidth', 'const int headWidth', 'const int headHeight', 'const int needleHeight', 'Polygon(dc, head', 'CreateSolidBrush(tint)', 'CreatePen(PS_SOLID, stroke, tint)', 'ThemeManager::AccentColor()', 'ThemeManager::SecondaryTextColor()', 'ThemeManager::WindowColor()', 'ThemeManager::HoverColor()', 'ThemeManager::PressedColor()', 'TrackMouseEvent', 'TME_LEAVE', 'WM_MOUSELEAVE', 'm_presetPinHot', 'm_presetPinPressed', 'ODS_FOCUS', 'focusInset', 'ThemeManager::FocusColor()', 'FrameRect')) {
    if($source -notmatch [regex]::Escape($token)) { throw "Missing procedural Templates pin behavior: $token" }
}
foreach($forbidden in @('PresetPinMaskResource', 'GetDIBits', 'IMAGE_BITMAP', 'DrawFocusRect', 'DrawIconEx', 'GetIconInfo')) {
    if($source -match [regex]::Escape($forbidden)) { throw "Legacy pin artwork path remains: $forbidden" }
}
if($source -notmatch 'pressed \? ThemeManager::PressedColor\(\) : hot \? ThemeManager::HoverColor\(\) : ThemeManager::WindowColor\(\)') { throw 'Pin background must be panel-matched when normal or pinned, with transient hover/pressed surfaces only.' }
foreach($token in @('verifyPins', 'WM_MOUSEMOVE', 'WM_LBUTTONDOWN', 'WM_MOUSELEAVE', 'EqualRect(&before, &after)', 'lightHover.surface == lightHoverSurface', 'lightPressed.surface == lightPressedSurface', 'lightFocus.surface == lightNormal.surface', 'darkFocus.surface == darkNormal.surface', 'lightFocus.focusIndicator', 'darkFocus.focusIndicator')) {
    if($runtime -notmatch [regex]::Escape($token)) { throw "Missing runtime pin state/geometry coverage: $token" }
}
$resourceHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
$resourceScript = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
foreach($legacy in @('IDI_FIND_PRESETS_PIN', 'IDB_FIND_PRESETS_PIN_16', 'IDB_FIND_PRESETS_PIN_20', 'IDB_FIND_PRESETS_PIN_24', 'IDB_FIND_PRESETS_PIN_32')) {
    if($resourceHeader -match [regex]::Escape($legacy) -or $resourceScript -match [regex]::Escape($legacy)) { throw "Obsolete pin resource remains: $legacy" }
}
foreach($path in @('src\fbe\res\icons\lucide\pin.ico', 'src\fbe\res\icons\lucide\pin.svg', 'src\fbe\res\icons\lucide\pin-off.ico', 'src\fbe\res\icons\lucide\pin-off.svg', 'src\fbe\res\icons\lucide\pin-mask-16.bmp', 'src\fbe\res\icons\lucide\pin-mask-20.bmp', 'src\fbe\res\icons\lucide\pin-mask-24.bmp', 'src\fbe\res\icons\lucide\pin-mask-32.bmp', 'tools\build\generate-pin-masks.ps1')) {
    if(Test-Path -LiteralPath (Join-Path $root $path)) { throw "Obsolete procedural-pin asset remains: $path" }
}Write-Host 'Search templates procedural pin rendering smoke passed.'
