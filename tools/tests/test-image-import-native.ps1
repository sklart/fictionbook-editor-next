[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset

$testDir = Join-Path $repoRoot 'out\tests\image-import'
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$emptyFixture = Join-Path $testDir 'empty-image.bin'
[IO.File]::WriteAllBytes($emptyFixture, [byte[]]@())
$oversizedFixture = Join-Path $testDir 'oversized-image.bin'
$oversizedStream = [IO.File]::Open($oversizedFixture, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $oversizedStream.SetLength(64MB + 1) }
finally { $oversizedStream.Dispose() }
$avifWithPngExtension = Join-Path $testDir 'actual-avif.png'
$avifFixture = Join-Path $repoRoot 'third_party\libheif\examples\example.avif'
Copy-Item -LiteralPath $avifFixture -Destination $avifWithPngExtension -Force
$truncatedAvif = Join-Path $testDir 'truncated-avif.heic'
$avifBytes = [IO.File]::ReadAllBytes($avifFixture)
[IO.File]::WriteAllBytes($truncatedAvif, $avifBytes[0..15])
$jpegFixture = Join-Path $testDir 'generated.jpg'
$jpegPassThroughFixture = Join-Path $testDir 'original.jpeg'
$gifFixture = Join-Path $testDir 'generated.gif'
$singleTiffFixture = Join-Path $testDir 'single-frame.tiff'
$tiffFixture = Join-Path $testDir 'generated.tiff'
$transparentPngFixture = Join-Path $testDir 'transparent.png'
$transparentPngWithWebpExtension = Join-Path $testDir 'actual-png.webp'
$animatedGifFixture = Join-Path $testDir 'animated.gif'
Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new(2, 2)
try {
    $bitmap.SetPixel(0, 0, [Drawing.Color]::Red)
    $bitmap.SetPixel(1, 0, [Drawing.Color]::Green)
    $bitmap.SetPixel(0, 1, [Drawing.Color]::Blue)
    $bitmap.SetPixel(1, 1, [Drawing.Color]::White)
    $bitmap.Save($jpegFixture, [Drawing.Imaging.ImageFormat]::Jpeg)
    $bitmap.Save($gifFixture, [Drawing.Imaging.ImageFormat]::Gif)
	$bitmap.Save($singleTiffFixture, [Drawing.Imaging.ImageFormat]::Tiff)
    $tiffCodec = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/tiff' }
    $tiffParameters = [Drawing.Imaging.EncoderParameters]::new(1)
    $tiffParameters.Param[0] = [Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::SaveFlag, [long][Drawing.Imaging.EncoderValue]::MultiFrame)
    $bitmap.Save($tiffFixture, $tiffCodec, $tiffParameters)
    $secondPage = [Drawing.Bitmap]::new(2, 2)
    try {
        $secondPage.SetPixel(0, 0, [Drawing.Color]::Black)
        $tiffParameters.Param[0] = [Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::SaveFlag, [long][Drawing.Imaging.EncoderValue]::FrameDimensionPage)
        $bitmap.SaveAdd($secondPage, $tiffParameters)
        $tiffParameters.Param[0] = [Drawing.Imaging.EncoderParameter]::new([Drawing.Imaging.Encoder]::SaveFlag, [long][Drawing.Imaging.EncoderValue]::Flush)
        $bitmap.SaveAdd($tiffParameters)
    }
    finally {
        $secondPage.Dispose()
        $tiffParameters.Dispose()
    }

    $transparent = [Drawing.Bitmap]::new(2, 2)
    try {
        $transparent.SetPixel(0, 0, [Drawing.Color]::FromArgb(64, 255, 0, 0))
        $transparent.SetPixel(1, 0, [Drawing.Color]::FromArgb(255, 0, 255, 0))
        $transparent.SetPixel(0, 1, [Drawing.Color]::FromArgb(255, 0, 0, 255))
        $transparent.SetPixel(1, 1, [Drawing.Color]::FromArgb(255, 255, 255, 255))
        $transparent.Save($transparentPngFixture, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $transparent.Dispose()
    }
}
finally {
    $bitmap.Dispose()
}
Copy-Item -LiteralPath $transparentPngFixture -Destination $transparentPngWithWebpExtension -Force
Copy-Item -LiteralPath $jpegFixture -Destination $jpegPassThroughFixture -Force
# Two 1x1 frames, encoded without relying on an external image utility.
[IO.File]::WriteAllBytes($animatedGifFixture, [Convert]::FromBase64String('R0lGODlhAQABAIAAAAAAAP///yH/C05FVFNDQVBFMi4wAwEAAAAh+QQECgAAACwAAAAAAQABAAACAkQBACH5BAQKAAAALAAAAAABAAEAAAICTAEAOw=='))
$exe = Join-Path $testDir 'image-import-smoke.exe'
$webp = Join-Path $repoRoot "build\libwebp\install\$Configuration"
$openjpeg = Join-Path $repoRoot "build\openjpeg\install\$Configuration"

& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE /D_UNICODE `
    "/I$repoRoot\src\fbe" "/I$repoRoot\third_party\wtl" "/I$webp\include" "/I$openjpeg\include" "/I$repoRoot\build\libheif\install\$Configuration\include" `
    (Join-Path $PSScriptRoot 'image-import-smoke.cpp') (Join-Path $repoRoot 'src\fbe\ImageImport.cpp') `
    "/Fe$exe" "/link" "/SUBSYSTEM:CONSOLE" "/LIBPATH:$webp\lib" "/LIBPATH:$openjpeg\lib" "/LIBPATH:$repoRoot\build\libheif\install\$Configuration\lib" "/LIBPATH:$repoRoot\build\libde265\install\$Configuration\lib" "/LIBPATH:$repoRoot\build\aom\install\$Configuration\lib" libwebpmux.lib libwebp.lib libsharpyuv.lib openjp2.lib heif.lib libde265.lib aom.lib gdiplus.lib ole32.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $exe `
    (Join-Path $repoRoot 'src\fbe\res\imgph.png') `
    $emptyFixture `
    $avifFixture `
    (Join-Path $repoRoot 'third_party\libheif\fuzzing\data\corpus\colors-no-alpha.heic') `
    (Join-Path $repoRoot 'third_party\libheif\fuzzing\data\corpus\colors-with-alpha.heic') `
    (Join-Path $repoRoot 'src\fbe\res\xml.bmp') `
    $avifWithPngExtension `
    $truncatedAvif `
    (Join-Path $repoRoot 'third_party\libwebp\examples\test.webp') `
    $jpegFixture `
    $gifFixture `
    $tiffFixture `
    $transparentPngFixture `
    (Join-Path $repoRoot 'third_party\libheif\examples\example.heic') `
    $transparentPngWithWebpExtension `
    $animatedGifFixture `
    $jpegPassThroughFixture `
    $oversizedFixture `
    $singleTiffFixture `
    (Join-Path $repoRoot 'third_party\libheif\tests\data\rainbow-451x461.heic')
if ($LASTEXITCODE -ne 0) { throw "Native ImageImport smoke-test завершился с кодом $LASTEXITCODE." }
Write-Host 'Native ImageImport smoke-test passed.'
