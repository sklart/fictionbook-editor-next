<# Guards the Fluent Pin source assets and runtime state coverage. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
$generator = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\generate-fluent-pin-masks.ps1')
foreach($size in 16,20,24,28,32) { foreach($state in 'regular','filled') {
    $path = Join-Path $root "src\fbe\res\icons\fluent-pin\ic_fluent_pin_${size}_${state}.svg"
    if(-not (Test-Path -LiteralPath $path)) { throw "Missing native Fluent Pin source: $path" }
} }
foreach($token in @('DrawPresetPinMask','FbeFluentPin::Select','AlphaBlend','UpdatePresetPinAction','fbe.search_preset.pin','fbe.search_preset.unpin','VK_RETURN','ThemeManager::FocusColor()','m_presetPinHot','m_presetPinPressed')) {
    if(-not $source.Contains($token)) { throw "Pin implementation misses $token" }
}
if($source.Contains('DrawPresetPinGlyph')) { throw 'Procedural Pin glyph remains.' }
foreach($token in @('verifyPins','DrawPresetPinMask','FbeFluentPin::Select','WM_MOUSEMOVE','WM_LBUTTONDOWN','WM_MOUSELEAVE','EqualRect(&before, &after)','lightFocus.focusIndicator','darkFocus.focusIndicator')) {
    if(-not $runtime.Contains($token)) { throw "Pin runtime smoke misses $token" }
}
foreach($token in @('Geometry]::Parse','FluentPinMasks.generated.h','[IO.File]::WriteAllText')) {
    if(-not $generator.Contains($token)) { throw "Pin generator misses $token" }
}
Write-Host 'Search templates Fluent Pin rendering contract passed.'
