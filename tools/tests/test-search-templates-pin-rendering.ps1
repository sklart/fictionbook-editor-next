<# Guards the procedural Templates thumbtack and its state contract. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
foreach($token in @('void DrawPresetPinGlyph', 'const int capWidth', 'const int headWidth', 'const int headHeight', 'const int needleHeight', 'Polygon(dc, head', 'CreateSolidBrush(tint)', 'CreatePen(PS_SOLID, stroke, tint)', 'ThemeManager::AccentColor()', 'ThemeManager::SecondaryTextColor()', 'ThemeManager::WindowColor()', 'ThemeManager::HoverColor()', 'ThemeManager::PressedColor()', 'TrackMouseEvent', 'TME_LEAVE', 'WM_MOUSELEAVE', 'm_presetPinHot', 'm_presetPinPressed')) {
    if($source -notmatch [regex]::Escape($token)) { throw "Missing procedural Templates pin behavior: $token" }
}
foreach($forbidden in @('PresetPinMaskResource', 'GetDIBits', 'IMAGE_BITMAP', 'DrawFocusRect', 'DrawIconEx', 'GetIconInfo')) {
    if($source -match [regex]::Escape($forbidden)) { throw "Legacy pin artwork path remains: $forbidden" }
}
if($source -notmatch 'pressed \? ThemeManager::PressedColor\(\) : hot \? ThemeManager::HoverColor\(\) : ThemeManager::WindowColor\(\)') { throw 'Pin background must be panel-matched when normal or pinned, with transient hover/pressed surfaces only.' }
foreach($token in @('verifyPins', 'WM_MOUSEMOVE', 'WM_LBUTTONDOWN', 'WM_MOUSELEAVE', 'EqualRect(&before, &after)', 'lightHover.surface == lightHoverSurface', 'lightPressed.surface == lightPressedSurface', 'lightFocus.surface == lightNormal.surface', 'darkFocus.surface == darkNormal.surface')) {
    if($runtime -notmatch [regex]::Escape($token)) { throw "Missing runtime pin state/geometry coverage: $token" }
}
Write-Host 'Search templates procedural pin rendering smoke passed.'