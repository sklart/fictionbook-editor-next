[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'empty-anchor'
$passed = $false
try {
    $inputFile = Join-Path $scratch.Root 'input.fb2'
    $outputFile = Join-Path $scratch.Root 'output.fb2'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Anchor</book-title><lang>en</lang></title-info><document-info><author><nickname>Fixture</nickname></author><date value="2026-10-28">2026-10-28</date><id>anchor-test</id><version>1.0</version></document-info></description><body><section id="section"><subtitle id="empty-subtitle"></subtitle><p id="empty-anchor"></p><p>Text <a l:href="#empty-anchor">back</a> <a l:href="#empty-subtitle">subtitle</a> <a l:href="#empty-verse">verse</a></p><empty-line/><poem><stanza><v id="empty-verse"></v><v>Verse</v></stanza></poem></section></body></FictionBook>'
    [IO.File]::WriteAllText($inputFile, $xml, [Text.UTF8Encoding]::new($false))
    $schema = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $schema.add('http://www.gribuser.ru/xml/fictionbook/2.0', (Resolve-Path (Join-Path $PSScriptRoot '../../runtime/FictionBook.xsd')).Path)
    $inputDoc = New-Object -ComObject Msxml2.DOMDocument.6.0
    $inputDoc.async = $false; $inputDoc.schemas = $schema
    if (-not $inputDoc.load($inputFile)) { throw "Input FB2 failed XSD: $($inputDoc.parseError.reason)" }
    & (Join-Path $PSScriptRoot 'test-fbe-epub-save-reopen-runtime.ps1') -FbeExe $FbeExe -InputFb2 $inputFile -OutputFb2 $outputFile
    [xml]$result = Get-Content -LiteralPath $outputFile -Raw
    $ns = [Xml.XmlNamespaceManager]::new($result.NameTable)
    $ns.AddNamespace('f', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $ns.AddNamespace('l', 'http://www.w3.org/1999/xlink')
    foreach ($pair in @(@('empty-anchor','p'), @('empty-subtitle','subtitle'), @('empty-verse','v'))) {
        $id = $pair[0]; $tag = $pair[1]
        if (@($result.SelectNodes('//*[@id="' + $id + '"]', $ns)).Count -ne 1 -or
            @($result.SelectNodes('//f:' + $tag + '[@id="' + $id + '"]', $ns)).Count -ne 1) { throw "Empty anchor ID or element type was lost: $id" }
        if (@($result.SelectNodes('//f:a[@l:href="#' + $id + '"]', $ns)).Count -ne 1) { throw "Backlink was lost after Save/Reopen: $id" }
    }
    if (@($result.SelectNodes('//f:empty-line', $ns)).Count -ne 1) { throw 'An ordinary empty line changed unexpectedly.' }
    $doc = New-Object -ComObject Msxml2.DOMDocument.6.0
    $doc.async = $false; $doc.schemas = $schema
    if (-not $doc.load($outputFile)) { throw "Saved FB2 failed XSD: $($doc.parseError.reason)" }
    $passed = $true
    Write-Host 'Empty anchor types, backlinks, Save/Reopen and XSD: PASS'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
