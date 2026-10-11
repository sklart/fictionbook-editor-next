[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceDirectory = Join-Path $repoRoot 'src\fbe\res\icons\fluent-pin'
$outputPath = Join-Path $OutputDirectory 'FluentPinMasks.generated.h'
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('// Generated from Microsoft Fluent UI System Icons Pin SVGs. Do not edit.')
$lines.Add('#pragma once')
$lines.Add('namespace FbeFluentPin {')
$lines.Add('struct Mask { int size; const unsigned char* alpha; };')
foreach($size in 16,20,24,28,32) {
    foreach($style in 'regular','filled') {
        $source = Join-Path $sourceDirectory "ic_fluent_pin_${size}_${style}.svg"
        if(-not (Test-Path -LiteralPath $source)) { throw "Missing Fluent Pin asset: $source" }
        [xml]$svg = Get-Content -LiteralPath $source -Raw -Encoding UTF8
        if($svg.svg.width -ne "$size" -or $svg.svg.height -ne "$size" -or @($svg.svg.path).Count -ne 1) {
            throw "Unsupported Fluent Pin SVG: $source"
        }
        $geometry = [System.Windows.Media.Geometry]::Parse($svg.svg.path.d)
        $visual = [System.Windows.Media.DrawingVisual]::new()
        $drawing = $visual.RenderOpen()
        $drawing.DrawGeometry([System.Windows.Media.Brushes]::Black, $null, $geometry)
        $drawing.Close()
        $bitmap = [System.Windows.Media.Imaging.RenderTargetBitmap]::new($size,$size,96,96,[System.Windows.Media.PixelFormats]::Pbgra32)
        $bitmap.Render($visual)
        $pixels = [byte[]]::new($size * $size * 4)
        $bitmap.CopyPixels($pixels,$size * 4,0)
        $mask = [System.Collections.Generic.List[string]]::new()
        for($index=0; $index -lt $size * $size; ++$index) { $mask.Add($pixels[$index * 4 + 3].ToString()) }
        if(-not ($mask | Where-Object { $_ -ne '0' } | Select-Object -First 1)) { throw "Empty Fluent Pin mask: $source" }
        $name = "kPin${size}${style}"
        $lines.Add("static const unsigned char $name[] = {")
        for($offset=0; $offset -lt $mask.Count; $offset+=32) {
            $last=[Math]::Min($offset+31,$mask.Count-1)
            $lines.Add('    ' + (($mask[$offset..$last]) -join ', ') + ',')
        }
        $lines.Add('};')
    }
}
$lines.Add('inline Mask Select(unsigned dpi, bool pinned) {')
$lines.Add('    const int size = dpi < 108 ? 16 : dpi < 132 ? 20 : dpi < 156 ? 24 : dpi < 180 ? 28 : 32;')
$lines.Add('    switch(size) {')
foreach($size in 16,20,24,28) { $lines.Add("    case ${size}: return { ${size}, pinned ? kPin${size}filled : kPin${size}regular };") }
$lines.Add('    default: return { 32, pinned ? kPin32filled : kPin32regular };')
$lines.Add('    }')
$lines.Add('}')
$lines.Add('}')
$content = ($lines -join "`n") + "`n"
if($Check) {
    if(-not (Test-Path -LiteralPath $outputPath) -or [IO.File]::ReadAllText($outputPath) -cne $content) { throw "Stale Fluent Pin masks: $outputPath" }
    Write-Host 'Fluent Pin masks are current.'
} else {
    [IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
    if(-not (Test-Path -LiteralPath $outputPath) -or [IO.File]::ReadAllText($outputPath) -cne $content) {
        [IO.File]::WriteAllText($outputPath,$content,[Text.UTF8Encoding]::new($false))
    }
    Write-Host "Fluent Pin masks prepared: $outputPath"
}
