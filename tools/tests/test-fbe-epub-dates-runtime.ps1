<# Checks EPUB 2 date semantics through FBE Save/Reopen and independent XSD. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$Batch = (Join-Path $PSScriptRoot '../../out/Release/ImportEPUBBatch.exe')
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$batchPath = (Resolve-Path -LiteralPath $Batch).Path
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-epub-dates-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($directory)
$passed = $false
try {
    Add-Type -AssemblyName System.IO.Compression
    $epub = Join-Path $directory 'dates.epub'
    $stream = [IO.File]::Open($epub, [IO.FileMode]::CreateNew)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @{
            'mimetype' = 'application/epub+zip'
            'META-INF/container.xml' = '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
            'OEBPS/book.opf' = '<package xmlns="http://www.idpf.org/2007/opf" xmlns:opf="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">dates</dc:identifier><dc:title>Dates</dc:title><dc:creator>Tester</dc:creator><dc:language>en</dc:language><dc:date opf:event="creation">1999-01-02</dc:date><dc:date opf:event="publication">2004-03-04</dc:date></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>'
            'OEBPS/chapter.xhtml' = '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><p>Content</p></body></html>'
        }
        foreach ($entry in $entries.GetEnumerator()) {
            $writer = [IO.StreamWriter]::new($archive.CreateEntry($entry.Key).Open(), [Text.UTF8Encoding]::new($false))
            try { $writer.Write($entry.Value) } finally { $writer.Dispose() }
        }
    } finally { $archive.Dispose(); $stream.Dispose() }
    $imported = Join-Path $directory 'imported.fb2'; $saved = Join-Path $directory 'saved.fb2'
    & $batchPath $epub $imported
    if ($LASTEXITCODE -ne 0) { throw 'EPUB date import failed.' }
    & (Join-Path $PSScriptRoot 'test-fbe-epub-save-reopen-runtime.ps1') -FbeExe $FbeExe -InputFb2 $imported -OutputFb2 $saved
    $settings = [Xml.XmlReaderSettings]::new(); $settings.ValidationType = [Xml.ValidationType]::Schema
    $settings.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    $null = $settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime/FictionBook.xsd'))
    $settings.Schemas.Compile()
    foreach ($path in @($imported, $saved)) {
        $reader = [Xml.XmlReader]::Create($path, $settings)
        try { while ($reader.Read()) {} } finally { $reader.Close() }
        [xml]$doc = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
        $date = $doc.SelectSingleNode("//*[local-name()='title-info']/*[local-name()='date']")
        $year = $doc.SelectSingleNode("//*[local-name()='publish-info']/*[local-name()='year']")
        $creation = $doc.SelectSingleNode("//*[local-name()='custom-info' and @info-type='EPUB date creation']")
        $publication = $doc.SelectSingleNode("//*[local-name()='custom-info' and @info-type='EPUB date publication']")
        if (!$date -or $date.InnerText -cne '2004-03-04' -or $date.GetAttribute('value') -cne '2004-03-04' -or
            !$year -or $year.InnerText -cne '2004' -or !$creation -or $creation.InnerText -cne '1999-01-02' -or
            !$publication -or $publication.InnerText -cne '2004-03-04') { throw "Date round trip failed: $path" }
    }
    $passed = $true
    Write-Host 'EPUB date semantics and independent XSD after FBE Save/Reopen: PASS.'
} finally {
    if ($passed) { Remove-Item -LiteralPath $directory -Recurse -Force }
    else { Write-Host "Date regression failure artifacts: $directory" }
}
