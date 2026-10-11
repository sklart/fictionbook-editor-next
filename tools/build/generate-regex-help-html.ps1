[CmdletBinding()]
param([switch]$Generate, [switch]$Check)

$ErrorActionPreference = 'Stop'
if($Generate.IsPresent -eq $Check.IsPresent) { throw 'Specify exactly one of -Generate or -Check.' }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$helpRoot = Join-Path $repoRoot 'runtime\Help'
$labels = Get-Content -LiteralPath (Join-Path $repoRoot 'localization\regex-help-html.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$languages = @(Get-Content -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') -Raw -Encoding UTF8 | ConvertFrom-Json).targetLanguages
$utf8 = [Text.UTF8Encoding]::new($false, $true)

function Encode([string]$value) { return [Net.WebUtility]::HtmlEncode($value) }

function Inline([string]$value) {
    $result = [Text.StringBuilder]::new()
    $start = 0
    foreach($match in [regex]::Matches($value, '`([^`]+)`|\[([^\]]+)\]\(([^)]+)\)')) {
        [void]$result.Append((Encode $value.Substring($start, $match.Index - $start)))
        if($match.Groups[1].Success) {
            [void]$result.Append('<code>').Append((Encode $match.Groups[1].Value)).Append('</code>')
        } else {
            $target = $match.Groups[3].Value
            if($target -match '^https?://[^\s"<>]+$') {
                [void]$result.Append('<a href="').Append((Encode $target)).Append('" rel="noopener noreferrer">').Append((Encode $match.Groups[2].Value)).Append('</a>')
            } elseif($target -match '^regex-(design|source)\.md$') {
                [void]$result.Append('<a href="').Append($target.Replace('.md','.html')).Append('">').Append((Encode $match.Groups[2].Value)).Append('</a>')
            } elseif($target -match '^#[a-zA-Z0-9-]+$') {
                [void]$result.Append('<a href="').Append($target).Append('">').Append((Encode $match.Groups[2].Value)).Append('</a>')
            } else { [void]$result.Append((Encode $match.Value)) }
        }
        $start = $match.Index + $match.Length
    }
    [void]$result.Append((Encode $value.Substring($start)))
    return $result.ToString()
}

function Highlight-Regex([string]$value) {
    $result = [Text.StringBuilder]::new()
    $start = 0
    $tokens = '\\(?:[pP]\{[^}\r\n]+\}|.)|\[[^\]\r\n]*\]|\{\d+(?:,\d*)?\}|[+*?]|\(\?[:<!=]*|[()]|[\^$]|\|'
    foreach($match in [regex]::Matches($value, $tokens)) {
        [void]$result.Append((Encode $value.Substring($start, $match.Index - $start)))
        $token = $match.Value
        $class = if($token.StartsWith('\')) { if($token -in @('\A','\z','\Z','\G','\b','\B')) { 'rx-anchor' } else { 'rx-escape' } }
            elseif($token.StartsWith('[')) { 'rx-class' }
            elseif($token -eq '^' -or $token -eq '$') { 'rx-anchor' }
            elseif($token -eq '|') { 'rx-alternation' }
            elseif($token.StartsWith('(') -or $token -eq ')') { 'rx-group' }
            else { 'rx-quantifier' }
        [void]$result.Append('<span class="').Append($class).Append('">').Append((Encode $token)).Append('</span>')
        $start = $match.Index + $match.Length
    }
    [void]$result.Append((Encode $value.Substring($start)))
    return $result.ToString()
}

function Table-Cells([string]$line) {
    $line = $line.Trim()
    if($line.StartsWith('|')) { $line = $line.Substring(1) }
    if($line.EndsWith('|')) { $line = $line.Substring(0, $line.Length - 1) }
    $cells = [System.Collections.Generic.List[string]]::new()
    $cell = [Text.StringBuilder]::new()
    $code = $false
    foreach($character in $line.ToCharArray()) {
        if($character -eq '`') { $code = !$code }
        if($character -eq '|' -and !$code) { $cells.Add($cell.ToString().Trim()); [void]$cell.Clear() }
        else { [void]$cell.Append($character) }
    }
    $cells.Add($cell.ToString().Trim())
    return ,$cells.ToArray()
}

function Is-Separator([string]$line) {
    return $line.Contains('|') -and $line.Contains('-') -and $line -match '^\s*\|?[\s|:\-]+\|?\s*$'
}

function Render-Page([string]$locale, [string]$mode, [string]$source, $strings) {
    $lines = ($source -replace "`r`n", "`n" -replace "`r", "`n").Split("`n")
    $body = [Text.StringBuilder]::new()
    $ids = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $headingNumber = 0
    $h1Count = 0
    $blockNumbers = @{ regex=0; example=0; note=0; warning=0; table=0 }
    $index = 0
    while($index -lt $lines.Length) {
        $line = $lines[$index]
        $trim = $line.Trim()
        if($trim.Length -eq 0) { ++$index; continue }
        if($trim -match '^```([^`]*)$' -or $trim -match '^:::(regex|example|note|warning)$') {
            $directive = $trim.StartsWith(':::')
            $kind = if($directive) { $Matches[1] } else { $Matches[1].Trim().ToLowerInvariant() }
            $closer = if($directive) { ':::' } else { '```' }
            $start = ++$index
            while($index -lt $lines.Length -and $lines[$index].Trim() -ne $closer) { ++$index }
            if($index -ge $lines.Length) { throw "$locale/$mode has an unclosed $closer block at line $start" }
            $content = if($index -gt $start) { $lines[$start..($index-1)] -join "`n" } else { '' }
            switch($kind) {
                'regex' { ++$blockNumbers.regex; [void]$body.Append("<pre class=`"regex`" id=`"regex-$($blockNumbers.regex)`"><code>").Append((Highlight-Regex $content)).Append("</code></pre>`n") }
                'example' { ++$blockNumbers.example; [void]$body.Append("<section class=`"example`" id=`"example-$($blockNumbers.example)`"><strong>").Append((Encode $strings.example)).Append('</strong><pre><code>').Append((Encode $content)).Append("</code></pre></section>`n") }
                'note' { ++$blockNumbers.note; [void]$body.Append("<aside class=`"note`" id=`"note-$($blockNumbers.note)`"><strong>").Append((Encode $strings.note)).Append('</strong><p>').Append((Inline $content.Trim())).Append("</p></aside>`n") }
                'warning' { ++$blockNumbers.warning; [void]$body.Append("<aside class=`"warning`" id=`"warning-$($blockNumbers.warning)`"><strong>").Append((Encode $strings.warning)).Append('</strong><p>').Append((Inline $content.Trim())).Append("</p></aside>`n") }
                default { [void]$body.Append('<pre><code>').Append((Encode $content)).Append("</code></pre>`n") }
            }
            ++$index
            continue
        }
        if($trim -match '^:::' -or $trim -match '^```') { throw "$locale/$mode has an unexpected marker at line $($index+1): $trim" }
        if($trim -match '^(#{1,3})\s+(.+)$') {
            $level = $Matches[1].Length
            $title = $Matches[2]
            if($level -eq 1) { ++$h1Count; $id='top' }
            elseif($title -match '^\s*(\d+(?:\.\d+)*)\.') { $id='section-' + $Matches[1].Replace('.','-') }
            else { $id='heading-' + (++$headingNumber) }
            if(-not $ids.Add($id)) { throw "$locale/$mode duplicates anchor $id" }
            [void]$body.Append("<h$level id=`"$id`">").Append((Inline $title)).Append("</h$level>`n")
            ++$index
            continue
        }
        if($index+1 -lt $lines.Length -and $trim.Contains('|') -and (Is-Separator $lines[$index+1])) {
            $headers = Table-Cells $line
            $index += 2
            ++$blockNumbers.table
            [void]$body.Append("<div class=`"table-wrap`" id=`"table-$($blockNumbers.table)`"><table><thead><tr>")
            foreach($cell in $headers) { [void]$body.Append('<th scope="col">').Append((Inline $cell)).Append('</th>') }
            [void]$body.Append("</tr></thead><tbody>`n")
            $rows = 0
            while($index -lt $lines.Length -and $lines[$index].Trim().Contains('|')) {
                $cells = Table-Cells $lines[$index]
                if($cells.Count -ne $headers.Count) { throw "$locale/$mode table column mismatch at line $($index+1)" }
                [void]$body.Append('<tr>')
                for($column=0; $column -lt $cells.Count; ++$column) {
                    $syntax = if($column -eq 0 -and $cells[$column] -match '[`\\<>\[\]{}^$]') { ' class="syntax"' } else { '' }
                    [void]$body.Append("<td$syntax>").Append((Inline $cells[$column])).Append('</td>')
                }
                [void]$body.Append("</tr>`n")
                ++$rows; ++$index
            }
            if($rows -eq 0) { throw "$locale/$mode has an empty table" }
            [void]$body.Append("</tbody></table></div>`n")
            continue
        }
        if($trim.StartsWith('- ')) {
            [void]$body.Append('<ul>')
            while($index -lt $lines.Length -and $lines[$index].Trim().StartsWith('- ')) {
                [void]$body.Append('<li>').Append((Inline $lines[$index].Trim().Substring(2))).Append('</li>')
                ++$index
            }
            [void]$body.Append("</ul>`n")
            continue
        }
        $paragraph = [System.Collections.Generic.List[string]]::new()
        while($index -lt $lines.Length -and $lines[$index].Trim().Length -gt 0) {
            $current = $lines[$index].Trim()
            if($paragraph.Count -gt 0 -and ($current -match '^(#{1,3})\s|^```|^:::|^- ' -or
                ($index+1 -lt $lines.Length -and $current.Contains('|') -and (Is-Separator $lines[$index+1])))) { break }
            $paragraph.Add($current); ++$index
        }
        [void]$body.Append('<p>').Append((Inline ($paragraph -join ' '))).Append("</p>`n")
    }
    if($h1Count -ne 1) { throw "$locale/$mode needs exactly one H1" }
    $other = if($mode -eq 'design') { 'source' } else { 'design' }
    $title = if($mode -eq 'design') { $strings.design } else { $strings.source }
    $otherTitle = if($other -eq 'design') { $strings.design } else { $strings.source }
    return "<!doctype html>`n<html lang=`"$locale`">`n<head><meta charset=`"utf-8`"><meta name=`"viewport`" content=`"width=device-width, initial-scale=1`"><title>$(Encode $title)</title><link rel=`"stylesheet`" href=`"../regex-help.css`"></head>`n<body><nav aria-label=`"$(Encode $strings.switch)`"><strong>$(Encode $title)</strong><a href=`"regex-$other.html`">$(Encode $otherTitle)</a></nav><main>`n$($body.ToString())</main></body></html>`n"
}

foreach($locale in $languages) {
    $locale = [string]$locale
    $strings = $labels.PSObject.Properties[$locale].Value
    if($null -eq $strings) { throw "Missing HTML labels for $locale" }
    foreach($mode in 'design','source') {
        $markdown = Join-Path $helpRoot "$locale\regex-$mode.md"
        $html = Join-Path $helpRoot "$locale\regex-$mode.html"
        if(-not (Test-Path -LiteralPath $markdown)) { throw "Missing Markdown source: $markdown" }
        $source = $utf8.GetString([IO.File]::ReadAllBytes($markdown))
        $content = Render-Page $locale $mode $source $strings
        if($Check) {
            if(-not (Test-Path -LiteralPath $html) -or [IO.File]::ReadAllText($html,$utf8) -cne $content) { throw "Generated HTML is stale: $html" }
        } elseif(-not (Test-Path -LiteralPath $html) -or [IO.File]::ReadAllText($html,$utf8) -cne $content) {
            [IO.File]::WriteAllText($html,$content,[Text.UTF8Encoding]::new($false))
        }
    }
}
Write-Host "Regex Help HTML $($(if($Check){'checked'}else{'generated'})): $($languages.Count * 2) pages."
