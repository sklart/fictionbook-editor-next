[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$PlatformToolset,
    [string]$RoundtripFbeExe,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset

. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'image-import-native' -RuntimePayload
$passed = $false
try {
Assert-IsolatedFbeFreeSpace -RequiredBytes 384MB
$testDir = $scratch.Root
Set-IsolatedRuntimeStage -Isolation $scratch -Stage 'prepare-fixtures'

function Invoke-ImageImportNativeProcess {
    param([Parameter(Mandatory)][string]$Executable, [Parameter(Mandatory)][string[]]$Arguments, [Parameter(Mandatory)][string]$Stage)
    Set-IsolatedRuntimeStage -Isolation $scratch -Stage $Stage
    $stdout = Join-Path $scratch.Root ($Stage + '.stdout')
    $stderr = Join-Path $scratch.Root ($Stage + '.stderr')
    # Windows file paths cannot contain quotes. Double trailing backslashes so
    # CommandLineToArgvW does not consume the closing quote (notably /Fo paths).
    $quoted = @($Arguments | ForEach-Object {
        if ($_ -match '"') { throw 'Native test argument unexpectedly contains a quote.' }
        '"' + ($_ -replace '(\\+)$', '$1$1') + '"'
    }) -join ' '
    $process = Start-Process -FilePath $Executable -ArgumentList $quoted -WorkingDirectory $repoRoot -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    try {
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $scratch -Scenario $Stage -Report $stdout -TimeoutSeconds $TimeoutSeconds
    } finally {
        if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout | ForEach-Object { Write-Host $_ } }
        if (Test-Path -LiteralPath $stderr) { Get-Content -LiteralPath $stderr | ForEach-Object { Write-Host $_ } }
    }
    if ($code -ne 0) { throw "Native ImageImport $Stage failed: exit=$code." }
}

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
function New-NclxFixture {
    param([string]$Source, [string]$Destination, [UInt16]$Primaries, [UInt16]$Transfer)
    $bytes = [IO.File]::ReadAllBytes($Source)
    $marker = [Text.Encoding]::ASCII.GetBytes('nclx')
    $offset = -1
    for ($i = 0; $i -le $bytes.Length - $marker.Length; ++$i) {
        if ($bytes[$i] -eq $marker[0] -and $bytes[$i + 1] -eq $marker[1] -and $bytes[$i + 2] -eq $marker[2] -and $bytes[$i + 3] -eq $marker[3]) { $offset = $i; break }
    }
    if ($offset -lt 0 -or $offset + 8 -ge $bytes.Length) { throw "В $Source не найден NCLX." }
    $bytes[$offset + 4] = [byte]($Primaries -shr 8); $bytes[$offset + 5] = [byte]$Primaries
    $bytes[$offset + 6] = [byte]($Transfer -shr 8); $bytes[$offset + 7] = [byte]$Transfer
    [IO.File]::WriteAllBytes($Destination, $bytes)
}
$displayP3AlphaFixture = Join-Path $testDir 'display-p3-alpha.avif'
$displayP3Fixture = Join-Path $testDir 'display-p3.avif'
$hdrPqFixture = Join-Path $testDir 'hdr-pq-10bit.avif'
$hdrHlgFixture = Join-Path $testDir 'hdr-hlg-10bit.avif'
$hdrUnspecifiedPrimariesFixture = Join-Path $testDir 'hdr-unspecified-primaries.avif'
New-NclxFixture (Join-Path $repoRoot 'tools\tests\fixtures\abc_color_irot_alpha_irot.avif') $displayP3AlphaFixture 12 13
New-NclxFixture $avifFixture $displayP3Fixture 12 13
New-NclxFixture (Join-Path $repoRoot 'tools\tests\fixtures\abc_color_irot_alpha_irot.avif') $hdrPqFixture 9 16
New-NclxFixture (Join-Path $repoRoot 'tools\tests\fixtures\abc_color_irot_alpha_irot.avif') $hdrHlgFixture 9 18
New-NclxFixture (Join-Path $repoRoot 'tools\tests\fixtures\abc_color_irot_alpha_irot.avif') $hdrUnspecifiedPrimariesFixture 2 16
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

$compiler = if ($env:FBE_UBSAN_CLANG) { $env:FBE_UBSAN_CLANG } else { (Get-Command cl.exe -ErrorAction Stop).Source }
$compileArguments = @(
    '/nologo', '/EHsc', '/std:c++17', '/MT', '/DUNICODE', '/D_UNICODE', '/DFBE_IMAGE_IMPORT_TESTING', "/Fo$testDir\\",
    "/I$repoRoot\src\fbe", "/I$repoRoot\third_party\wtl", "/I$webp\include", "/I$openjpeg\include", "/I$repoRoot\build\libheif\install\$Configuration\include",
    (Join-Path $PSScriptRoot 'image-import-smoke.cpp'), (Join-Path $repoRoot 'src\fbe\ImageImport.cpp'),
    "/Fe$exe", '/link', '/SUBSYSTEM:CONSOLE', "/LIBPATH:$webp\lib", "/LIBPATH:$openjpeg\lib", "/LIBPATH:$repoRoot\build\libheif\install\$Configuration\lib", "/LIBPATH:$repoRoot\build\libde265\install\$Configuration\lib", "/LIBPATH:$repoRoot\build\aom\install\$Configuration\lib",
    'libwebpmux.lib', 'libwebp.lib', 'libsharpyuv.lib', 'openjp2.lib', 'heif.lib', 'libde265.lib', 'aom.lib', 'gdiplus.lib', 'mscms.lib', 'ole32.lib'
)
if ($env:FBE_UBSAN_CLANG) {
    if (-not $global:FbeUbsanHelperOptions.Count) { throw 'Missing UBSan helper compiler options.' }
    $compileArguments = @('--target=i686-pc-windows-msvc') + @($global:FbeUbsanHelperOptions) + $compileArguments
}
Invoke-ImageImportNativeProcess -Executable $compiler -Arguments $compileArguments -Stage 'compile'
$smokeArguments = @(
    (Join-Path $repoRoot 'src\fbe\res\imgph.png'), $emptyFixture, $avifFixture,
    (Join-Path $repoRoot 'third_party\libheif\fuzzing\data\corpus\colors-no-alpha.heic'),
    (Join-Path $repoRoot 'third_party\libheif\fuzzing\data\corpus\colors-with-alpha.heic'),
    (Join-Path $repoRoot 'src\fbe\res\xml.bmp'), $avifWithPngExtension, $truncatedAvif,
    (Join-Path $repoRoot 'third_party\libwebp\examples\test.webp'), $jpegFixture, $gifFixture, $tiffFixture, $transparentPngFixture,
    (Join-Path $repoRoot 'third_party\libheif\examples\example.heic'), $transparentPngWithWebpExtension, $animatedGifFixture, $jpegPassThroughFixture, $oversizedFixture, $singleTiffFixture,
    (Join-Path $repoRoot 'third_party\libheif\tests\data\rainbow-451x461.heic'),
    (Join-Path $repoRoot 'tools\tests\fixtures\sdr_fox_10bit.avif'),
    (Join-Path $repoRoot 'tools\tests\fixtures\abc_color_irot_alpha_irot.avif'), $displayP3Fixture, $displayP3AlphaFixture,
    (Join-Path $repoRoot 'third_party\libheif\tests\data\rainbow-451x461.heic'), $hdrPqFixture, $hdrHlgFixture, $hdrUnspecifiedPrimariesFixture,
    (Join-Path $repoRoot 'tools\tests\fixtures\sdr_fox_10bit.avif')
)
Invoke-ImageImportNativeProcess -Executable $exe -Arguments $smokeArguments -Stage 'run-smoke'
if ($RoundtripFbeExe) {
    & (Join-Path $PSScriptRoot 'test-image-import-fbe-roundtrip.ps1') -FbeExe $RoundtripFbeExe -SmokeExe $exe -JpegFixture $jpegPassThroughFixture -TimeoutSeconds $TimeoutSeconds
}
$passed = $true
Write-Host 'Native ImageImport smoke-test passed.'
} catch {
    [IO.File]::WriteAllText((Join-Path $scratch.Root 'error.txt'), $_.Exception.ToString(), [Text.UTF8Encoding]::new($false))
    throw
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed
}
