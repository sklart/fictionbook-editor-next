<# Checks external EPUB note links across import and FBE Save/Reopen. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$Batch = (Join-Path $PSScriptRoot '../../out/Release/ImportEPUBBatch.exe')
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$batchPath = (Resolve-Path -LiteralPath $Batch).Path
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-note-links-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($directory)
$passed = $false
try {
    Add-Type -AssemblyName System.IO.Compression
    $epub = Join-Path $directory 'notes.epub'
    $stream = [IO.File]::Open($epub, [IO.FileMode]::CreateNew)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $entries = @{
            'mimetype' = 'application/epub+zip'
            'META-INF/container.xml' = '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'
            'OEBPS/book.opf' = '<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">notes-fixture</dc:identifier><dc:title>Notes fixture</dc:title><dc:creator>Tester</dc:creator><dc:language>en</dc:language><meta property="dcterms:modified">2026-01-01T00:00:00Z</meta></metadata><manifest><item id="chapter" href="Text/chapter.xhtml" media-type="application/xhtml+xml"/><item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="chapter"/></spine></package>'
            'OEBPS/Text/chapter.xhtml' = '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Chapter</title></head><body><p><a id="ref" epub:type="noteref" href="../notes.xhtml#n">42</a> <a epub:type="noteref" href="../notes.xhtml#n">42</a> <a href="../notes.xhtml#back">Marker link</a></p></body></html>'
            'OEBPS/notes.xhtml' = '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><head><title>Notes</title></head><body><aside epub:type="footnote" id="n"><p>First note paragraph</p><p><em>Second note paragraph</em></p><a id="back" epub:type="backlink" href="Text/chapter.xhtml#ref"><span id="nested">Return</span></a></aside></body></html>'
        }
        foreach($entry in $entries.GetEnumerator()) {
            $writer = [IO.StreamWriter]::new($archive.CreateEntry($entry.Key).Open(), [Text.UTF8Encoding]::new($false))
            try { $writer.Write($entry.Value) } finally { $writer.Dispose() }
        }
    } finally { $archive.Dispose(); $stream.Dispose() }
    $schema = [Xml.XmlReaderSettings]::new(); $schema.ValidationType = [Xml.ValidationType]::Schema
    $schema.Schemas.XmlResolver = [Xml.XmlUrlResolver]::new()
    $null = $schema.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0', (Join-Path $root 'runtime/FictionBook.xsd'))
    $schema.Schemas.Compile()
    foreach($mode in @('original','unified')) {
        $imported = Join-Path $directory "$mode.fb2"; $saved = Join-Path $directory "$mode-saved.fb2"
        $options = @()
        if($mode -eq 'unified') { $options += '--unify-note-numbers' }
        & $batchPath $epub $imported @options
        if($LASTEXITCODE -ne 0) { throw "EPUB note import failed: $mode" }
        & (Join-Path $PSScriptRoot 'test-fbe-epub-save-reopen-runtime.ps1') -FbeExe $FbeExe -InputFb2 $imported -OutputFb2 $saved
        foreach($path in @($imported, $saved)) {
            $reader = [Xml.XmlReader]::Create($path, $schema)
            try { while($reader.Read()) {} } finally { $reader.Close() }
            [xml]$doc = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
            $ids = @($doc.SelectNodes('//*[@id]') | ForEach-Object { $_.GetAttribute('id') })
            if(@($ids | Select-Object -Unique).Count -ne $ids.Count) { throw "Duplicate IDs in $path" }
            foreach($node in $doc.SelectNodes("//*[local-name()='a']")) {
                $target = $node.GetAttribute('href','http://www.w3.org/1999/xlink')
                if($target.StartsWith('#') -and $ids -cnotcontains $target.Substring(1)) { throw "Unresolved link $target in $path" }
            }
            $notes = @($doc.SelectNodes("//*[local-name()='a' and @type='note']"))
            $expected = if($mode -eq 'unified') { '#n_1' } else { '#n' }
            if($notes.Count -ne 2 -or @($notes | Where-Object { $_.GetAttribute('href','http://www.w3.org/1999/xlink') -cne $expected }).Count) { throw "Unexpected note targets in $path" }
            $section = $doc.SelectSingleNode("/*[local-name()='FictionBook']/*[local-name()='body' and @name='notes']/*[local-name()='section']")
            if(!$section -or !$section.InnerText.Contains('First note paragraph') -or !$section.InnerText.Contains('Second note paragraph') -or $section.InnerText.Contains('Return')) { throw "Note content or removed backlink incorrect in $path" }
            if(!$section.SelectSingleNode(".//*[local-name()='emphasis' and normalize-space(.)='Second note paragraph']")) { throw "Note emphasis missing in $path" }
            $main = $doc.SelectSingleNode("/*[local-name()='FictionBook']/*[local-name()='body' and not(@name)]")
            if(!$main -or $main.InnerText.Contains('First note paragraph') -or $main.InnerText.Contains('Second note paragraph')) { throw "Note text duplicated in main body in $path" }
            $marker = $main.SelectSingleNode(".//*[local-name()='a' and normalize-space(.)='Marker link']")
            if(!$marker -or $marker.GetAttribute('href','http://www.w3.org/1999/xlink') -cne '#back') { throw "Marker link lost in $path" }
            foreach($id in @('back','nested')) { if($section.SelectNodes(".//*[@id='$id']").Count -ne 1) { throw "Backlink anchor $id missing in $path" } }
        }
    }
    $passed = $true
    Write-Host 'External note links, repeated references, backlink anchors and XSD after FBE Save/Reopen: PASS.'
} finally {
    if($passed) { Remove-Item -LiteralPath $directory -Recurse -Force }
    else { Write-Host "Note regression failure artifacts: $directory" }
}
