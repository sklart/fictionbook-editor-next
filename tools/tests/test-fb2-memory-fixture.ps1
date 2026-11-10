[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('fbe-memory-fixture-' + [guid]::NewGuid().ToString('N') + '.fb2')
try {
    & (Join-Path $PSScriptRoot 'new-fb2-memory-fixture.ps1') -SizeMiB 1 -OutputPath $fixture
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $PSScriptRoot '../../runtime/FictionBook.xsd'))
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false; $document.validateOnParse = $true; $document.schemas = $cache
    if(-not $document.load($fixture)) { throw "Benchmark fixture XSD: $($document.parseError.reason)" }
    if((Get-Item -LiteralPath $fixture).Length -lt 1MB) { throw 'Fixture is smaller than requested.' }
    $document.setProperty('SelectionNamespaces',"xmlns:f='http://www.gribuser.ru/xml/fictionbook/2.0' xmlns:l='http://www.w3.org/1999/xlink'")
    $ids = @{}; foreach($node in $document.selectNodes('//*[@id]')) { $id = [string]$node.getAttribute('id'); if($ids.ContainsKey($id)) { throw "Duplicate fixture ID: $id" }; $ids[$id]=$true }
    foreach($node in $document.selectNodes('//*[@l:href]')) { $href = [string]$node.getAttribute('l:href'); if($href.StartsWith('#') -and -not $ids.ContainsKey($href.Substring(1))) { throw "Broken fixture link: $href" } }
    Write-Host 'Performance fixture XSD, IDs and links PASS.'
} finally { Remove-Item -LiteralPath $fixture -Force -ErrorAction SilentlyContinue }
