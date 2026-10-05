[CmdletBinding()]
param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\src\fbe\res\icons\lucide'))

$ErrorActionPreference = 'Stop'

function Test-PinPixel([int]$Size, [int]$X, [int]$Y) {
    $center = ($Size - 1) / 2.0
    $distance = [Math]::Abs($X - $center)
    $relative = ($Y + 0.5) / $Size
    if ($relative -ge 0.12 -and $relative -lt 0.50) { return $distance -le [Math]::Max(1, [Math]::Floor($Size * 0.16)) }
    if ($relative -ge 0.50 -and $relative -lt 0.68) { return $distance -le [Math]::Max(2, [Math]::Floor($Size * 0.27)) }
    if ($relative -ge 0.68 -and $relative -lt 0.88) { return $distance -le [Math]::Max(1, [Math]::Floor($Size * 0.055)) }
    return $false
}

foreach ($size in 16, 20, 24, 32) {
    $stride = [int]([Math]::Ceiling($size / 32.0) * 4)
    $pixels = New-Object byte[] ($stride * $size)
    for ($y = 0; $y -lt $size; ++$y) {
        $row = $size - 1 - $y
        for ($x = 0; $x -lt $size; ++$x) {
            if (Test-PinPixel $size $x $y) { $byte = [int][Math]::Floor($x / 8); $pixels[$row * $stride + $byte] = $pixels[$row * $stride + $byte] -bor (0x80 -shr ($x % 8)) }
        }
    }
    $headerSize = 14 + 40 + 8
    $fileSize = $headerSize + $pixels.Length
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    $writer.Write([uint16]0x4d42); $writer.Write([uint32]$fileSize); $writer.Write([uint16]0); $writer.Write([uint16]0); $writer.Write([uint32]$headerSize)
    $writer.Write([uint32]40); $writer.Write([int32]$size); $writer.Write([int32]$size); $writer.Write([uint16]1); $writer.Write([uint16]1)
    $writer.Write([uint32]0); $writer.Write([uint32]$pixels.Length); $writer.Write([int32]0); $writer.Write([int32]0); $writer.Write([uint32]2); $writer.Write([uint32]0)
    $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([byte]255); $writer.Write([byte]255); $writer.Write([byte]255); $writer.Write([byte]0)
    $writer.Write($pixels); $writer.Flush()
    [IO.File]::WriteAllBytes((Join-Path $OutputDirectory "pin-mask-$size.bmp"), $stream.ToArray())
    $writer.Dispose(); $stream.Dispose()
}
