[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
foreach($locale in @('en-US','ru-RU')) {
    $html = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root "runtime\Help\$locale\regex-source.html")
    if(-not $html.Contains('&lt;empty-line')) { throw "$locale XML example is not escaped." }
    if($html -match '<empty-line(?:\s|/|>)') { throw "$locale XML example became an HTML element." }
    if($html -match '(?i)<script\b|javascript:') { throw "$locale executable content in Help." }
}
Write-Host 'Regex Help HTML escaping contract passed.'