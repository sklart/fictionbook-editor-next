[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$commonDirectory = Join-Path $repoRoot 'src\common\fb2'
$files = @(
    'Fb2BinaryInspector.h', 'Fb2BinaryInspector.cpp',
    'Fb2Metadata.h', 'Fb2Metadata.cpp',
    'Fb2CoverImage.h', 'Fb2CoverImage.cpp',
    'Fb2CoverThumbnail.h', 'Fb2CoverThumbnail.cpp',
    'Fb2ShellProperties.h', 'Fb2ShellProperties.cpp'
)

foreach ($file in $files) {
    $commonPath = Join-Path $commonDirectory $file
    if (-not (Test-Path -LiteralPath $commonPath -PathType Leaf)) {
        throw "Missing shared FB2 source: $commonPath"
    }

    $legacyPath = Join-Path $repoRoot "src\fbe\$file"
    if (Test-Path -LiteralPath $legacyPath) {
        throw "Shared FB2 source must not remain under src/fbe: $legacyPath"
    }
}

$fbeProject = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.vcxproj')
$shellProject = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbshell\FBShell.vcxproj')
foreach ($name in 'Fb2Metadata', 'Fb2CoverImage', 'Fb2CoverThumbnail', 'Fb2ShellProperties') {
    if ($fbeProject -notmatch [regex]::Escape("..\common\fb2\$name.cpp")) {
        throw "FBE project does not compile the common FB2 source $name."
    }
    if ($shellProject -notmatch [regex]::Escape("..\common\fb2\$name.cpp")) {
        throw "FBShell project does not compile the common FB2 source $name."
    }
}

foreach ($project in @(
    @{ Name = 'FBE'; Text = $fbeProject },
    @{ Name = 'ExportEPUB'; Text = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\export-epub\ExportEPUB.vcxproj')) },
    @{ Name = 'ExportDOCX'; Text = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\export-docx\ExportDOCX.vcxproj')) },
    @{ Name = 'ExportHTML'; Text = (Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\export-html\ExportHTML.vcxproj')) }
)) {
    if ($project.Text -notmatch [regex]::Escape('..\common\fb2\Fb2BinaryInspector.cpp')) {
        throw "$($project.Name) project does not compile the shared FB2 binary inspector."
    }
}

foreach ($legacy in @(
    'src\export-epub\ExportEPUBPlugin.cpp',
    'src\export-docx\ExportDOCXPlugin.cpp'
)) {
    if ((Get-Content -Raw -LiteralPath (Join-Path $repoRoot $legacy)) -match 'ExtFromImageSignature|DecodeBase64Text|bool\s+DecodeBase64\(') {
        throw "Legacy binary inspection implementation remains in $legacy."
    }
}

if ($shellProject -match '\.\.\\fbe\\Fb2(?:Metadata|CoverImage|CoverThumbnail|ShellProperties)') {
    throw 'FBShell must not compile FB2 sources from src/fbe.'
}

$commonSources = Get-ChildItem -LiteralPath $commonDirectory -Filter '*.cpp' -File
foreach ($source in $commonSources) {
    $text = Get-Content -Raw -LiteralPath $source.FullName
    if ($text -match '\.\.\\fbe\\|\.\./fbe/') {
        throw "Common FB2 source depends on private FBE path: $($source.FullName)"
    }
}

$thumbnailSource = Get-Content -Raw -LiteralPath (Join-Path $commonDirectory 'Fb2CoverThumbnail.cpp')
if ($thumbnailSource -match '#include\s+"stdafx\.h"') {
    throw 'Fb2CoverThumbnail must not use the FBE precompiled header.'
}
if ($thumbnailSource -notmatch 'IWICBitmapFrameDecode' -or $thumbnailSource -notmatch 'IWICBitmapScaler' -or $thumbnailSource -notmatch 'IWICFormatConverter') {
    throw 'Fb2CoverThumbnail must keep the IWICBitmapFrameDecode -> IWICBitmapScaler -> IWICFormatConverter pipeline.'
}
if ($thumbnailSource -match 'CImage|StretchBlt|atlimage\.h') {
    throw 'Fb2CoverThumbnail thumbnail path must not restore legacy ATL/GDI scaling.'
}

Write-Host 'Common FB2 boundary contract passed.'
