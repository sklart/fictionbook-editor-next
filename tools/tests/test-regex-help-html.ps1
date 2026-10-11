[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\generate-regex-help-html.ps1') -Check
$catalog = Get-Content -LiteralPath (Join-Path $root 'localization\regex-help-html.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$locales = @($catalog.PSObject.Properties.Name)
if($locales.Count -ne 12) { throw "Expected 12 HTML locales, got $($locales.Count)." }
$help = Join-Path $root 'runtime\Help'
$css = Get-Content -LiteralPath (Join-Path $help 'regex-help.css') -Raw -Encoding UTF8
foreach($token in @('max-width: 1050px','prefers-color-scheme: dark','@media print','focus-visible','Cascadia Mono','table-wrap')) {
    if(-not $css.Contains($token)) { throw "HTML CSS missing $token" }
}
foreach($fallback in @('background: #ffffff; background: var(--page)', 'color: #20232a; color: var(--text)',
    'border: 1px solid #c8d0da; border: 1px solid var(--line)', 'background: #e8edf4; background: var(--head)',
    'color: #0758a7; color: var(--link)', 'padding: 10px 20px; padding: 10px max(',
    'word-wrap: break-word; overflow-wrap: anywhere')) {
    if(-not $css.Contains($fallback)) { throw "HTML CSS lacks legacy fallback: $fallback" }
}
if($css -notmatch 'a\s*\{[^}]*word-wrap:\s*break-word;\s*overflow-wrap:\s*anywhere;') {
    throw 'HTML links lack legacy wrapping fallback.'
}
if($css -match '(?i)@import|url\(\s*https?://') { throw 'HTML CSS has a network dependency.' }
$workflow = Get-Content -LiteralPath (Join-Path $root '.github\workflows\build.yml') -Raw -Encoding UTF8
$validate = ($workflow -split '(?m)^  build:\s*$', 2)[0]
if($validate -notmatch 'generate-regex-help-html\.ps1 -Check' -or $validate -match 'generate-regex-help-html\.ps1 -Generate') {
    throw 'CI validate must check generated HTML before the build can generate it.'
}
$regexCount = 0
foreach($locale in $locales) {
    foreach($mode in 'design','source') {
        $markdown = Get-Content -LiteralPath (Join-Path $help "$locale\regex-$mode.md") -Raw -Encoding UTF8
        $html = Get-Content -LiteralPath (Join-Path $help "$locale\regex-$mode.html") -Raw -Encoding UTF8
        $sourceIds = @([regex]::Matches($markdown, '(?m)^\[([DS]\d+)\] ') | ForEach-Object { $_.Groups[1].Value })
        $expectedIds = if($mode -eq 'design') { @('D1','D2') } else { @('S1','S2') }
        if(($sourceIds -join ',') -cne ($expectedIds -join ',') -or
            $markdown.Contains('D1–D4') -or $markdown.Contains('[S3]') -or
            $html.Contains('D1–D4') -or $html.Contains('[S3]')) {
            throw "$locale/$mode has missing or stale source references"
        }
        $heading = ([regex]::Match($markdown, '(?m)^# (.+)$')).Groups[1].Value
        $titleMatch = [regex]::Match($html, '<title>(.*?)</title>')
        if(-not $heading -or -not $titleMatch.Success -or [Net.WebUtility]::HtmlDecode($titleMatch.Groups[1].Value) -cne $heading) {
            throw "$locale/$mode has an incomplete browser title"
        }
        if($html -notmatch "<html lang=`"$locale`">" -or $html -notmatch '<h1 id="top">') { throw "$locale/$mode lacks document language or H1" }
        foreach($token in @('<table>','<thead>','<tbody>','<th scope="col">','<section class="example"','<aside class="note"','<aside class="warning"','<pre class="regex"')) {
            if(-not $html.Contains($token)) { throw "$locale/$mode missing semantic element $token" }
        }
        if($html -match '(?m)^:::' -or $html.Contains('```') -or $html -match '<script\b|https?://[^" ]+\.css') { throw "$locale/$mode contains raw markers or script/network content" }
        $anchors = @([regex]::Matches($html, '\bid="([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
        if($anchors.Count -ne @($anchors | Select-Object -Unique).Count) { throw "$locale/$mode has duplicate anchors" }
        $sourceBlocks = @([regex]::Matches($markdown, '(?ms)^```regex\r?\n(.*?)^```\r?$', [Text.RegularExpressions.RegexOptions]::Multiline) | ForEach-Object { $_.Groups[1].Value -replace '\r?\n$', '' })
        $renderedBlocks = @([regex]::Matches($html, '(?s)<pre class="regex" id="[^"]+"><code>(.*?)</code></pre>') | ForEach-Object {
            [Net.WebUtility]::HtmlDecode(($_.Groups[1].Value -replace '<span class="[^"]+">|</span>', ''))
        })
        if($sourceBlocks.Count -ne $renderedBlocks.Count) { throw "$locale/$mode regex block count changed: $($sourceBlocks.Count) -> $($renderedBlocks.Count)" }
        for($index=0; $index -lt $sourceBlocks.Count; ++$index) {
            if($sourceBlocks[$index] -cne $renderedBlocks[$index]) { throw "$locale/$mode regex text changed at block $index" }
        }
        $regexCount += $renderedBlocks.Count
        if($mode -eq 'source' -and -not $html.Contains('&lt;p&gt;[ \t]')) { throw "$locale/source XML regex is not escaped" }
        $externalLinks = @([regex]::Matches($html, '<a href="https?://[^" ]+" rel="noopener noreferrer">'))
        if($externalLinks.Count -lt $(if($mode -eq 'design') { 2 } else { 3 }) -or
            $html -match '<a href="https?://[^" ]+"(?! rel="noopener noreferrer")' -or
            $html -match '<code>https?://') { throw "$locale/$mode lacks clickable safe source links" }
    }
}
$manifest = Get-Content -LiteralPath (Join-Path $root 'packaging\package-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$required = @($manifest.core.required)
if($required -cnotcontains 'Help\regex-help.css') { throw 'Package manifest lacks regex-help.css' }
foreach($locale in $locales) { foreach($mode in 'design','source') {
    if($required -cnotcontains "Help\$locale\regex-$mode.html") { throw "Package manifest lacks $locale/$mode HTML" }
} }
Write-Host "Regex Help HTML generator/semantics passed: 24 pages, $regexCount regex blocks."
