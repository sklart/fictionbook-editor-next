[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$api = & (Join-Path $PSScriptRoot 'ensure-fbe-api.ps1') -Configuration Release
$objects = Join-Path $root 'out\tests\import-epub-separator-com'; New-Item -ItemType Directory -Force -Path $objects | Out-Null
$exe = Join-Path $objects 'separator-com.exe'
& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE ("/I$api") ("/Fo:$objects\") (Join-Path $PSScriptRoot 'import-epub-separator-com-harness.cpp') (Join-Path $api 'FBE_i.c') /link ole32.lib oleaut32.lib msxml6.lib ("/OUT:$exe")
if($LASTEXITCODE -ne 0) { throw 'COM separator harness failed to compile.' }
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'separator-com'
$passed = $false
try {
    Copy-Item -LiteralPath $exe -Destination (Join-Path $iso.Root 'separator-com.exe')
    $epub = Join-Path $iso.Root 'separator.epub'
    Add-Type -AssemblyName System.IO.Compression
    $file = [IO.File]::Open($epub, [IO.FileMode]::CreateNew)
    $archive = [IO.Compression.ZipArchive]::new($file, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @{
            'mimetype' = 'application/epub+zip'
            'META-INF/container.xml' = '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
            'OEBPS/book.opf' = '<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">fixture</dc:identifier><dc:title>Fixture</dc:title><dc:language>en</dc:language><dc:creator>Tester</dc:creator><meta name="cover" content="coveritem"/><meta property="dcterms:modified">2026-10-21T00:00:00Z</meta></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="coveritem" href="cover.png" media-type="image/png"/></manifest><spine><itemref idref="chapter"/></spine></package>'
            'OEBPS/chapter.xhtml' = '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Fixture</title></head><body><p>Before<a epub:type="noteref" href="#footnote">42</a></p><p id="divider">* * *</p><p>After<img src="cover.png"/></p><aside epub:type="footnote" id="footnote"><p>Note body</p></aside></body></html>'
        }
        foreach($entry in $entries.GetEnumerator()) {
            $writer = [IO.StreamWriter]::new($archive.CreateEntry($entry.Key).Open(), [Text.UTF8Encoding]::new($false))
            try { $writer.Write($entry.Value) } finally { $writer.Dispose() }
        }
        $png = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=')
        $stream = $archive.CreateEntry('OEBPS/cover.png').Open()
        try { $stream.Write($png, 0, $png.Length) } finally { $stream.Dispose() }
    } finally { $archive.Dispose(); $file.Dispose() }
    $settings = Join-Path $iso.DataPath 'Settings\Plugins'; New-Item -ItemType Directory -Force -Path $settings | Out-Null
    foreach($case in @(@{ separator=0; title=0 }, @{ separator=1; title=0 }, @{ separator=0; title=1 }, @{ separator=1; title=1 })) {
        $enabled = $case.separator; $titleEnabled = $case.title
        $profile = @{ schemaVersion = 1; values = @{ SeparatorSubtitles = $enabled; CreateTitleBlock = $titleEnabled; UnifyImageNames = $enabled; SeparateCoverName = $titleEnabled; UnifyNoteNumbers = $titleEnabled } } | ConvertTo-Json
        [IO.File]::WriteAllText((Join-Path $settings 'import-epub.json'), $profile, [Text.UTF8Encoding]::new($false))
        $fb2 = Join-Path $iso.Root "result-$enabled-$titleEnabled.fb2"
        $process = Start-Process -FilePath (Join-Path $iso.Root 'separator-com.exe') -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru -ArgumentList @('"' + (Join-Path $iso.Root 'Plugins\ImportEPUB.dll') + '"', '"' + $epub + '"', '"' + $fb2 + '"')
        if(-not $process.WaitForExit(30000)) { Stop-Process -Id $process.Id -Force; throw 'COM separator conversion timed out.' }
        if($process.ExitCode -ne 0) { throw "COM separator conversion failed: $($process.ExitCode)." }
        [xml]$result = [IO.File]::ReadAllText($fb2, [Text.Encoding]::UTF8)
        $validation = [Xml.XmlReaderSettings]::new(); $validation.ValidationType = [Xml.ValidationType]::Schema
        $validation.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
        $null = $validation.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime\FictionBook.xsd'))
        $validation.Schemas.Compile(); $validation.DtdProcessing = [Xml.DtdProcessing]::Prohibit
        $reader = [Xml.XmlReader]::Create($fb2, $validation)
        try { while($reader.Read()) {} } finally { $reader.Close() }
        $paragraph = $result.SelectNodes("//*[local-name()='p' and normalize-space(.)='* * *']").Count
        $subtitle = $result.SelectNodes("//*[local-name()='subtitle' and normalize-space(.)='* * *']").Count
        if(($enabled -eq 0 -and ($paragraph -ne 1 -or $subtitle -ne 0)) -or ($enabled -eq 1 -and ($paragraph -ne 0 -or $subtitle -ne 1))) { throw "Unexpected separator structure: enabled=$enabled p=$paragraph subtitle=$subtitle" }
        $title = $result.SelectNodes("/*[local-name()='FictionBook']/*[local-name()='body']//*[local-name()='p' and normalize-space(.)='Fixture']").Count
        $author = $result.SelectNodes("/*[local-name()='FictionBook']/*[local-name()='body']//*[local-name()='p' and normalize-space(.)='Tester']").Count
        if($title -ne 1 -or $author -ne $titleEnabled) { throw "Unexpected title block: enabled=$titleEnabled title=$title author=$author" }
        $ids = @($result.SelectNodes('//*[@id]') | ForEach-Object { $_.GetAttribute('id') })
        if(-not ($ids -match 'divider')) { throw 'Separator anchor was lost.' }
        $binary = $result.SelectSingleNode("//*[local-name()='binary']").GetAttribute('id')
        $expected = if($enabled -eq 0) { 'coveritem' } elseif($titleEnabled -eq 1) { 'cover.png' } else { 'i_001.png' }
        if($binary -ne $expected) { throw "Unexpected image ID: $binary, expected $expected" }
        foreach($image in $result.SelectNodes("//*[local-name()='image']")) {
            if($image.GetAttribute('href', 'http://www.w3.org/1999/xlink') -ne "#$binary") { throw 'Image reference did not match the binary ID.' }
        }
        $noteLink = $result.SelectSingleNode("//*[local-name()='a' and @type='note']")
        $expectedMarker = if($titleEnabled -eq 1) { '[1]' } else { '42' }
        $expectedTarget = if($titleEnabled -eq 1) { '#n_1' } else { '#footnote' }
        if($noteLink.InnerText -ne $expectedMarker -or $noteLink.GetAttribute('href', 'http://www.w3.org/1999/xlink') -ne $expectedTarget) { throw 'Unexpected note numbering or target.' }
    }
    $passed = $true
    Write-Host 'COM separator/title/image/note options, anchors and XSD checks passed.'
} finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
