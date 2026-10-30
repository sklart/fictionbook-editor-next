<# Checks figure anchor targets through import, FBE save/reopen and XSD. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$Batch = (Join-Path $PSScriptRoot '../../out/Release/ImportEPUBBatch.exe')
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$batchPath = (Resolve-Path -LiteralPath $Batch).Path
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-figure-anchors-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($directory)
$passed = $false
try {
    Add-Type -AssemblyName System.IO.Compression
    $png = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=')
    $epub = Join-Path $directory 'figure.epub'
    $stream = [IO.File]::Open($epub, [IO.FileMode]::CreateNew)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @{
            'mimetype' = 'application/epub+zip'
            'META-INF/container.xml' = '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
            'OEBPS/book.opf' = '<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">figure-fixture</dc:identifier><dc:title>Figure fixture</dc:title><dc:creator>Tester</dc:creator><dc:language>en</dc:language><meta property="dcterms:modified">2026-01-01T00:00:00Z</meta></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="image" href="image.png" media-type="image/png"/></manifest><spine><itemref idref="chapter"/></spine></package>'
            'OEBPS/chapter.xhtml' = '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Figure fixture</title></head><body><h1>Illustration</h1><figure id="figure-anchor"><img id="image-anchor" src="image.png"/><figcaption id="caption-anchor">Picture <em id="caption-inner">caption</em></figcaption></figure><p><a href="#figure-anchor">Figure</a> <a href="#image-anchor">Image</a> <a href="#caption-anchor">Caption</a> <a href="#caption-inner">Inner</a></p></body></html>'
        }
        foreach($entry in $entries.GetEnumerator()) {
            $writer = [IO.StreamWriter]::new($archive.CreateEntry($entry.Key).Open(), [Text.UTF8Encoding]::new($false))
            try { $writer.Write($entry.Value) } finally { $writer.Dispose() }
        }
        $image = $archive.CreateEntry('OEBPS/image.png').Open()
        try { $image.Write($png, 0, $png.Length) } finally { $image.Dispose() }
    } finally { $archive.Dispose(); $stream.Dispose() }
    $imported = Join-Path $directory 'imported.fb2'
    $saved = Join-Path $directory 'saved.fb2'
    & $batchPath $epub $imported
    if($LASTEXITCODE -ne 0) { throw 'Figure EPUB import failed.' }
    & (Join-Path $PSScriptRoot 'test-fbe-epub-save-reopen-runtime.ps1') -FbeExe $FbeExe -InputFb2 $imported -OutputFb2 $saved
    $schema = [Xml.XmlReaderSettings]::new(); $schema.ValidationType = [Xml.ValidationType]::Schema
    $schema.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    $null = $schema.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime/FictionBook.xsd'))
    $schema.Schemas.Compile()
    foreach($path in @($imported, $saved)) {
        $reader = [Xml.XmlReader]::Create($path, $schema)
        try { while($reader.Read()) {} } finally { $reader.Close() }
        [xml]$doc = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
        foreach($id in @('figure-anchor','image-anchor','caption-anchor','caption-inner')) {
            if($doc.SelectNodes("//*[@id='$id']").Count -ne 1 -or $doc.SelectNodes("//*[local-name()='a' and @*[local-name()='href']='#$id']").Count -ne 1) { throw "Anchor missing, duplicated or unlinked in ${path}: $id" }
        }
        if($doc.SelectNodes("//*[local-name()='subtitle' and normalize-space(.)='Picture caption']").Count -ne 1) { throw "Caption missing in $path" }
        $bytes = [Convert]::FromBase64String($doc.SelectSingleNode("//*[local-name()='binary']").InnerText)
        if([Convert]::ToBase64String($bytes) -cne [Convert]::ToBase64String($png)) { throw "Image bytes changed in $path" }
    }
    $passed = $true
    Write-Host 'Figure anchors, caption, image bytes and XSD after FBE Save/Reopen: PASS.'
} finally {
    if($passed) { Remove-Item -LiteralPath $directory -Recurse -Force }
    else { Write-Host "Figure regression failure artifacts: $directory" }
}
