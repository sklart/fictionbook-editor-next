[CmdletBinding()]
param([string]$OutputPath)

$ErrorActionPreference = 'Stop'
if (-not $OutputPath) {
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $OutputPath = Join-Path $repoRoot 'src\fbe\res\Scripts_48.bmp'
}

Add-Type -AssemblyName System.Drawing
$size = 48
$bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $paper = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $paper.AddLines([System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new(9, 4), [System.Drawing.PointF]::new(29, 4),
        [System.Drawing.PointF]::new(40, 15), [System.Drawing.PointF]::new(40, 43),
        [System.Drawing.PointF]::new(9, 43)))
    $paper.CloseFigure()
    $outline = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(72, 83, 94), 2)
    $outline.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.FillPath([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(252, 253, 254)), $paper)
    $graphics.DrawPath($outline, $paper)

    $fold = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $fold.AddLines([System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new(29, 4), [System.Drawing.PointF]::new(29, 15),
        [System.Drawing.PointF]::new(40, 15)))
    $fold.CloseFigure()
    $graphics.FillPath([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(215, 228, 237)), $fold)
    $graphics.DrawPath($outline, $fold)
    $line = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(174, 185, 194), 2)
    $graphics.DrawLine($line, 14, 20, 33, 20)
    $graphics.DrawLine($line, 14, 24, 30, 24)

    $badge = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $badge.AddArc(12, 27, 8, 8, 180, 90)
    $badge.AddArc(37, 27, 8, 8, 270, 90)
    $badge.AddArc(37, 38, 8, 8, 0, 90)
    $badge.AddArc(12, 38, 8, 8, 90, 90)
    $badge.CloseFigure()
    $graphics.FillPath([System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(22, 115, 55)), $badge)
    $font = [System.Drawing.Font]::new('Segoe UI', 15, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $graphics.DrawString('JS', $font, [System.Drawing.Brushes]::White, [System.Drawing.PointF]::new(17, 27))
}
finally {
    $graphics.Dispose()
}

try {
    # Preserve the alpha bytes in a conventional 32-bit DIB. The Windows
    # resource loader does not accept the BITMAPV5HEADER variant here.
    $stream = [IO.File]::Open($OutputPath, [IO.FileMode]::Create, [IO.FileAccess]::Write)
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $pixelOffset = 14 + 40
        $writer.Write([byte[]](0x42, 0x4d))
        $writer.Write([uint32]($pixelOffset + $size * $size * 4))
        $writer.Write([uint32]0)
        $writer.Write([uint32]$pixelOffset)
        $writer.Write([uint32]40)
        $writer.Write([int32]$size)
        $writer.Write([int32](-$size))
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]0) # BI_RGB
        $writer.Write([uint32]($size * $size * 4))
        foreach ($value in 1..4) { $writer.Write([uint32]0) } # resolution and palette
        for ($y = 0; $y -lt $size; ++$y) {
            for ($x = 0; $x -lt $size; ++$x) {
                $color = $bitmap.GetPixel($x, $y)
                $writer.Write([byte]$color.B)
                $writer.Write([byte]$color.G)
                $writer.Write([byte]$color.R)
                $writer.Write([byte]$color.A)
            }
        }
    }
    finally { $writer.Dispose() }
}
finally { $bitmap.Dispose() }
