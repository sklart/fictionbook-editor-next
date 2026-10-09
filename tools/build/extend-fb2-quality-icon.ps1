[CmdletBinding()]
param([string]$IconPath)

$ErrorActionPreference = 'Stop'
if (-not $IconPath) {
    $repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $IconPath = Join-Path $repositoryRoot 'src\fbe\res\Fb2Quality.ico'
}
$IconPath = [IO.Path]::GetFullPath($IconPath)
Add-Type -AssemblyName System.Drawing

$source = [IO.File]::ReadAllBytes($IconPath)
if ([BitConverter]::ToUInt16($source, 2) -ne 1) { throw 'Expected an ICO resource.' }
$count = [BitConverter]::ToUInt16($source, 4)
$base = $null
for ($index = 0; $index -lt $count; $index++) {
    $entry = 6 + 16 * $index
    if ($source[$entry] -ne 32 -or $source[$entry + 1] -ne 32) { continue }
    $length = [BitConverter]::ToUInt32($source, $entry + 8)
    $offset = [BitConverter]::ToUInt32($source, $entry + 12)
    $base = New-Object byte[] $length
    [Array]::Copy($source, $offset, $base, 0, $length)
    break
}
if (-not $base) { throw 'The original 32x32 icon entry is missing.' }

$entries = [Collections.Generic.List[object]]::new()
$entries.Add(@{ Size = 32; Bytes = $base })
foreach ($size in 40, 48, 64) {
    $scale = $size / 32.0
    $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.ScaleTransform([single]$scale, [single]$scale)
        $dark = [Drawing.Color]::FromArgb(255, 17, 83, 139)
        $blue = [Drawing.Color]::FromArgb(255, 43, 133, 190)
        $paper = [Drawing.SolidBrush]::new([Drawing.Color]::White)
        $line = [Drawing.Pen]::new($dark, 1.5)
        $accent = [Drawing.Pen]::new($blue, 1.3)
        $lens = [Drawing.Pen]::new($dark, 2.3)
        try {
            $page = [Drawing.PointF[]]@(
                [Drawing.PointF]::new(7, 3), [Drawing.PointF]::new(21, 3),
                [Drawing.PointF]::new(26, 8), [Drawing.PointF]::new(26, 28),
                [Drawing.PointF]::new(7, 28))
            $graphics.FillPolygon($paper, $page)
            $graphics.DrawPolygon($line, $page)
            $graphics.DrawLine($line, 21, 3, 21, 8)
            $graphics.DrawLine($line, 21, 8, 26, 8)
            foreach ($y in 12, 15, 18) { $graphics.DrawLine($accent, 10, $y, 21, $y) }
            $graphics.FillEllipse($paper, 17.5, 18.5, 10, 10)
            $graphics.DrawEllipse($lens, 17.5, 18.5, 10, 10)
            $lens.StartCap = [Drawing.Drawing2D.LineCap]::Round
            $lens.EndCap = [Drawing.Drawing2D.LineCap]::Round
            $graphics.DrawLine($lens, 26, 27, 30, 31)
        }
        finally { $paper.Dispose(); $line.Dispose(); $accent.Dispose(); $lens.Dispose() }
        $stream = [IO.MemoryStream]::new()
        try {
            $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
            $entries.Add(@{ Size = $size; Bytes = $stream.ToArray() })
        }
        finally { $stream.Dispose() }
    }
    finally { $graphics.Dispose(); $bitmap.Dispose() }
}

$output = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$entries.Count)
    $offset = 6 + 16 * $entries.Count
    foreach ($entry in $entries) {
        $writer.Write([byte]$entry.Size)
        $writer.Write([byte]$entry.Size)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$entry.Bytes.Length)
        $writer.Write([uint32]$offset)
        $offset += $entry.Bytes.Length
    }
    foreach ($entry in $entries) { $writer.Write([byte[]]$entry.Bytes) }
    [IO.File]::WriteAllBytes($IconPath, $output.ToArray())
}
finally { $writer.Dispose(); $output.Dispose() }
Write-Host "Extended FB2 quality icon: $IconPath (32, 40, 48, 64 px)"
