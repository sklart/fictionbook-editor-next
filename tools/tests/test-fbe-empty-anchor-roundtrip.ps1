[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [switch]$KeepArtifacts
)
$ErrorActionPreference = 'Stop'
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-empty-anchor-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $inputFile = Join-Path $directory input.fb2
    $output = Join-Path $directory output.fb2
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Anchor</book-title><lang>en</lang></title-info><document-info><author><nickname>Fixture</nickname></author><date value="2026-10-28">2026-10-28</date><id>anchor-test</id><version>1.0</version></document-info></description><body><section id="section"><subtitle id="empty-subtitle"></subtitle><p id="empty-anchor"></p><p>Text <a l:href="#empty-anchor">back</a> <a l:href="#empty-subtitle">subtitle</a> <a l:href="#empty-verse">verse</a></p><empty-line/><poem><stanza><v id="empty-verse"></v><v>Verse</v></stanza></poem></section></body></FictionBook>'
    [IO.File]::WriteAllText($inputFile, $xml, [Text.UTF8Encoding]::new($false))
    $inputSchema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $inputSchema.add('http://www.gribuser.ru/xml/fictionbook/2.0', (Resolve-Path (Join-Path $PSScriptRoot '../../runtime/FictionBook.xsd')).Path)
    $inputDoc = New-Object -ComObject Msxml2.DOMDocument.6.0
    $inputDoc.async = $false; $inputDoc.schemas = $inputSchema
    if (-not $inputDoc.load($inputFile)) { throw "Input FB2 failed XSD: $($inputDoc.parseError.reason)" }
    & (Join-Path $PSScriptRoot 'test-fbe-epub-save-reopen-runtime.ps1') -FbeExe $FbeExe -InputFb2 $inputFile -OutputFb2 $output
    [xml]$result = Get-Content -LiteralPath $output -Raw
    $ns = [Xml.XmlNamespaceManager]::new($result.NameTable)
    $ns.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $ns.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    foreach ($id in @('empty-anchor','empty-subtitle','empty-verse')) {
        if (@($result.SelectNodes('//*[@id="' + $id + '"]', $ns)).Count -ne 1) { throw "Empty anchor ID was lost or duplicated after Save/Reopen: $id" }
        if (@($result.SelectNodes('//f:a[@l:href="#' + $id + '"]', $ns)).Count -ne 1) { throw "Backlink was lost after Save/Reopen: $id" }
    }
    if (@($result.SelectNodes('//f:empty-line', $ns)).Count -ne 1) { throw 'An ordinary empty line changed unexpectedly.' }
    $schema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $schema.add('http://www.gribuser.ru/xml/fictionbook/2.0', (Resolve-Path (Join-Path $PSScriptRoot '../../runtime/FictionBook.xsd')).Path)
    $doc = New-Object -ComObject Msxml2.DOMDocument.6.0
    $doc.async = $false; $doc.schemas = $schema
    if (-not $doc.load($output)) { throw "Saved FB2 failed XSD: $($doc.parseError.reason)" }
    Write-Host 'Empty anchor, backlink, Save/Reopen and XSD: PASS'
} finally {
    if ($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
