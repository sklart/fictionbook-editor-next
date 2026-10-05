[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
foreach ($token in @('PresetPinMaskResource', 'LoadImage', 'IMAGE_BITMAP', 'GetDIBits', 'struct BitmapInfo1Bit', 'RGBQUAD colors[2]', 'maskInfo.header.biBitCount = 1', 'maskBits[', 'ThemeManager::AccentColor()', 'ThemeManager::SecondaryTextColor()')) {
    if ($source.IndexOf($token, [System.StringComparison]::Ordinal) -lt 0) { throw "Pin renderer misses $token." }
}
if ($source -match 'DrawIconEx\(|GetIconInfo\(|IMAGE_ICON|maskPixels\[index\]|0x00ffffff') { throw 'The retired ICO/RGB coverage path remains in the pin renderer.' }
foreach ($size in 16,20,24,32) {
    $path = Join-Path $root "src\fbe\res\icons\lucide\pin-mask-$size.bmp"
    $bytes = [IO.File]::ReadAllBytes($path)
    if ([BitConverter]::ToUInt16($bytes, 0) -ne 0x4d42 -or [BitConverter]::ToInt32($bytes, 18) -ne $size -or [Math]::Abs([BitConverter]::ToInt32($bytes, 22)) -ne $size) { throw "Invalid $size px pin mask." }
    if ([BitConverter]::ToUInt16($bytes, 28) -ne 1) { throw "Pin mask $size px must be 1-bit, not color-derived." }
    $offset = [BitConverter]::ToInt32($bytes, 10)
    $stride = [int](([Math]::Floor(($size + 31) / 32)) * 4)
    $foregroundCount = 0; $backgroundCount = 0; $minX = $size; $minY = $size; $maxX = -1; $maxY = -1; $hasLowerNeedle = $false; $foregroundByRow = New-Object int[] $size
    # This is the same off-screen BGRA composition contract as DrawPresetPinGlyph:
    # bit 1 selects the themed glyph, bit 0 keeps the button surface.
    $surface = New-Object byte[] ($size * $size * 4)
    for ($y = 0; $y -lt $size; ++$y) {
        for ($x = 0; $x -lt $size; ++$x) {
            $sourceRow = $size - 1 - $y
            $covered = ($bytes[$offset + $sourceRow * $stride + [int][Math]::Floor($x / 8)] -band (0x80 -shr ($x % 8))) -ne 0
            $pixel = 4 * ($y * $size + $x)
            if ($covered) { $surface[$pixel] = 215; $surface[$pixel + 1] = 120; $surface[$pixel + 2] = 0; ++$foregroundCount; ++$foregroundByRow[$y]; $minX = [Math]::Min($minX, $x); $minY = [Math]::Min($minY, $y); $maxX = [Math]::Max($maxX, $x); $maxY = [Math]::Max($maxY, $y); if ($y -ge [int]($size * 0.70) -and [Math]::Abs($x - (($size - 1) / 2)) -le 2) { $hasLowerNeedle = $true } }
            else { $surface[$pixel] = 245; $surface[$pixel + 1] = 245; $surface[$pixel + 2] = 245; ++$backgroundCount }
            $surface[$pixel + 3] = 255
        }
    }
    $coverage = $foregroundCount / ($size * $size)
    if ($foregroundCount -le 0 -or $backgroundCount -le 0 -or $coverage -lt 0.03 -or $coverage -gt 0.55) { throw "Pin coverage for $size px is not a visible glyph: $coverage." }
    if ($minX -le 0 -or $minY -le 0 -or $maxX -ge $size - 1 -or $maxY -ge $size - 1 -or -not $hasLowerNeedle) { throw "Pin mask $size px is not a centered pin silhouette with a needle." }
    $headRows = $foregroundByRow[([int]($size * .10))..([int]($size * .45))]
    $baseRows = $foregroundByRow[([int]($size * .45))..([int]($size * .70))]
    $needleRows = $foregroundByRow[([int]($size * .70))..([int]($size * .94))]
    if ((($headRows | Measure-Object -Maximum).Maximum -lt 3) -or (($baseRows | Measure-Object -Maximum).Maximum -le (($headRows | Measure-Object -Maximum).Maximum)) -or (($needleRows | Where-Object { $_ -gt 0 -and $_ -le 3 }).Count -lt 2)) {
        throw "Pin mask $size px must retain a filled head, a wider pin bar, and a narrow lower needle."
    }
    for ($y = 0; $y -lt $size; ++$y) {
        for ($x = 0; $x -lt [int]($size / 2); ++$x) {
            $sourceRow = $size - 1 - $y
            $left = ($bytes[$offset + $sourceRow * $stride + [int][Math]::Floor($x / 8)] -band (0x80 -shr ($x % 8))) -ne 0
            $rightX = $size - 1 - $x
            $right = ($bytes[$offset + $sourceRow * $stride + [int][Math]::Floor($rightX / 8)] -band (0x80 -shr ($rightX % 8))) -ne 0
            if ($left -ne $right) { throw "Pin mask $size px must be horizontally symmetric." }
        }
    }
}
if ((Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\res\icons\lucide\pin.svg')) -notmatch '<path ') { throw 'Pin SVG must contain the authored symmetric thumbtack path.' }
Write-Host 'Search templates pin rendering smoke passed.'
