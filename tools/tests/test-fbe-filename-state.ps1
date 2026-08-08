$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
if($source -match 'document\.fbwFilename') { throw 'Dead document.fbwFilename state must not be recreated.' }
if(-not $source.Contains('window.external.InflateParagraphs(body);')) { throw 'TransformXML InflateParagraphs call is missing.' }
Write-Host 'TransformXML filename-state contract passed.'
