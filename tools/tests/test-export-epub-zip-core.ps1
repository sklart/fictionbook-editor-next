[CmdletBinding()]
param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'export-epub-zip'; $passed = $false
try {
    & (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
    $exe = Join-Path $scratch.Root 'zip-core.exe'; $epub = Join-Path $scratch.Root 'zip-core.epub'
    $zlib = Join-Path $root "build\zlib\install\$Configuration"
    $runtimeOption = if($Configuration -eq 'Debug') { '/MTd' } else { '/MT' }
    $zlibName = if($Configuration -eq 'Debug') { 'zsd.lib' } else { 'zs.lib' }
    $objects = Join-Path $scratch.Root 'obj'; New-Item -ItemType Directory -Path $objects | Out-Null
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 $runtimeOption "/I$zlib\include" "/Fo:$objects\\" (Join-Path $PSScriptRoot 'export-epub-zip-core-test.cpp') (Join-Path $root 'src\export-epub\FbeEpubExport.cpp') /link (Join-Path $zlib ("lib\" + $zlibName)) "/OUT:$exe"
    if($LASTEXITCODE -ne 0) { throw 'ZIP core fixture did not compile.' }
    & $exe $epub; if($LASTEXITCODE -ne 0) { throw 'ZIP core fixture export failed.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($epub)
    try {
        $image = $zip.GetEntry('OEBPS/изображения/обложка.png'); $text = $zip.GetEntry('OEBPS/данные/текст.txt')
        if(-not $image -or -not $text -or $image.CompressedLength -ne $image.Length -or $text.CompressedLength -ge $text.Length) { throw 'Cyrillic resource path or compression method is incorrect.' }
        $bytes = [IO.MemoryStream]::new(); $stream = $image.Open(); try { $stream.CopyTo($bytes) } finally { $stream.Dispose() }
        if([Convert]::ToHexString($bytes.ToArray()) -ne '89504E4700FF42') { throw 'Binary resource changed.' }
        $bytes.Dispose(); $bytes = [IO.MemoryStream]::new(); $stream = $text.Open(); try { $stream.CopyTo($bytes) } finally { $stream.Dispose() }
        if($bytes.Length -ne 150000 -or @($bytes.ToArray() | Where-Object { $_ -ne 65 }).Count -ne 0) { throw 'Large compressed resource changed.' }
        $bytes.Dispose()
    } finally { $zip.Dispose() }
    $passed = $true; Write-Host 'EPUB ZIP Cyrillic paths, stored binary and multiblock Deflate: PASS'
} finally { Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed }
