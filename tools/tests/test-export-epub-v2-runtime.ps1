[CmdletBinding()] param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'; $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dll = Join-Path $root "out\$Configuration\Plugins\ExportEPUB.dll"; if (-not (Test-Path -LiteralPath $dll)) { throw "Missing ExportEPUB.dll: $dll" }
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$exe = Join-Path $root "out\$Configuration\export-epub-v2-runtime.exe"
& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DFBE_TEST_EXPORT_EPUB /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'export-html-v2-runtime-harness.cpp') (Join-Path $root 'src\fbe\FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { throw 'ExportEPUB v2 runtime harness did not compile.' }
function Test-V2EpubContent([string]$Path, [int]$Version) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -eq 0) { throw "v2 EPUB $Version is missing or empty: $Path" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $texts = @{}; foreach ($entry in $zip.Entries) { if ($entry.FullName -match '\.(opf|xhtml|ncx)$') { $reader = [IO.StreamReader]::new($entry.Open(), [Text.Encoding]::UTF8, $true); try { $texts[$entry.FullName] = $reader.ReadToEnd() } finally { $reader.Dispose() } } }
    } finally { $zip.Dispose() }
    foreach ($required in @('OEBPS/content.opf','OEBPS/text/chapter_001.xhtml')) { if (-not $texts.ContainsKey($required) -or [string]::IsNullOrWhiteSpace($texts[$required])) { throw "v2 EPUB $Version lacks required content: $required" } }
    if ($Version -eq 3 -and -not $texts.ContainsKey('OEBPS/nav.xhtml')) { throw 'v2 EPUB 3 lacks nav.xhtml.' }
    if (-not $texts.ContainsKey('OEBPS/toc.ncx')) { throw "v2 EPUB $Version lacks toc.ncx." }
    $combined = ($texts.Values -join "`n"); if (-not $combined.Contains('Привет, мир')) { throw "v2 EPUB $Version lost Cyrillic fixture text." }
    foreach ($marker in @([string][char]0x00D0,[string][char]0x00D1)) { if ($combined.Contains($marker)) { throw "v2 EPUB $Version contains mojibake." } }
}
try {
    $lines = @(& $exe $dll); if ($LASTEXITCODE -ne 0) { throw "ExportEPUB v2 runtime harness failed: $LASTEXITCODE" }
    $epub3 = ($lines | Where-Object { $_ -like 'EPUB3=*' } | Select-Object -First 1) -replace '^EPUB3='; $epub2 = ($lines | Where-Object { $_ -like 'EPUB2=*' } | Select-Object -First 1) -replace '^EPUB2='
    if ([string]::IsNullOrWhiteSpace($epub3) -or [string]::IsNullOrWhiteSpace($epub2)) { throw 'Runtime harness did not return both EPUB paths.' }
    try { Test-V2EpubContent $epub3 3; Test-V2EpubContent $epub2 2 } finally { Remove-Item -LiteralPath $epub3,$epub2 -Force -ErrorAction SilentlyContinue; $parent = Split-Path -Parent $epub3; Remove-Item -LiteralPath $parent -Force -ErrorAction SilentlyContinue }
} finally { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath ($exe -replace '\.exe$','.pdb') -Force -ErrorAction SilentlyContinue }
