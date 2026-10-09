[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\README.md')
$locales = @($catalog.targetLanguages | ForEach-Object { [string]$_ })
function Get-FencedBlocks([string]$text, [string]$label) {
    $fences = [regex]::Matches($text, '(?m)^```[^\r\n]*(?:\r?\n|$)')
    if (($fences.Count % 2) -ne 0) { throw "$label has unbalanced fenced blocks" }
    $blocks = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $fences.Count; $index += 2) {
        $start = $fences[$index].Index
        $end = $fences[$index + 1].Index + $fences[$index + 1].Length
        $blocks.Add($text.Substring($start, $end - $start))
    }
    return @($blocks)
}
foreach ($locale in $locales) {
    foreach ($name in @('regex-design.md', 'regex-source.md')) {
        $path = Join-Path $root "runtime\Help\$locale\$name"
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing localized Markdown help: $locale/$name" }
        $bytes = [IO.File]::ReadAllBytes($path)
        try { $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) } catch { throw "$locale/$name is not valid UTF-8: $($_.Exception.Message)" }
        if ([string]::IsNullOrWhiteSpace($text) -or $bytes.Length -lt 1024) { throw "$locale/$name is empty or unreasonably short" }
        foreach ($required in @('# ', '## ', '```')) { if (-not $text.Contains($required)) { throw "$locale/$name misses Markdown construct $required" } }
        if (([regex]::Matches($text, '(?m)^```')).Count % 2 -ne 0) { throw "$locale/$name has unbalanced fenced blocks" }
        $specialBlocks = @([regex]::Matches($text, '(?m)^:::(example|note|warning)\r?$') | ForEach-Object { $_.Groups[1].Value })
        $expectedBlocks = if ($name -eq 'regex-design.md') { @('example', 'example', 'warning', 'note', 'example') } else { @('note', 'note', 'example', 'warning') }
        if (($specialBlocks -join ',') -cne ($expectedBlocks -join ',')) { throw "$locale/$name special-block sequence differs from the reference: $($specialBlocks -join ',')" }
        if (([regex]::Matches($text, '(?m)^:::\r?$')).Count -ne $specialBlocks.Count) { throw "$locale/$name has unbalanced special blocks" }
        $examples = @([regex]::Matches($text, '(?ms)^:::example\r?\n(.*?)^:::\r?$') | ForEach-Object { $_.Groups[1].Value })
        if ($name -eq 'regex-design.md') {
            $controls = @(@('[ \t]{2,}', 'Он   пришёл'), @('[ \t]{2,}', 'Он   пришёл', 'Он пришёл'), @('(?<=№ )([0-9]+)', '№ 125', '№ [125]'))
        } else { $controls = @(, @('<p>[ \t]*</p>', '<p>   </p>')) }
        for ($index = 0; $index -lt $controls.Count; ++$index) {
            foreach ($control in $controls[$index]) {
                if (-not $examples[$index].Contains($control)) { throw "$locale/$name example #$($index + 1) changed control text: $control" }
            }
        }
        if ($name -eq 'regex-source.md') {
            if ($text -notmatch '(?m)^:::note\r?$') { throw "$locale/$name must include a user-facing information note." }
            if ($text -notmatch '(?m)^```regex\r?$') { throw "$locale/$name must mark regular-expression examples explicitly." }
        }
        if ($locale -ne 'en-US') {
            $english = [Text.UTF8Encoding]::new($false, $true).GetString([IO.File]::ReadAllBytes((Join-Path $root "runtime\Help\en-US\$name")))
            if ($text -ceq $english) { throw "$locale/$name must not be an English copy" }
        }
        if ($locale -ne 'ru-RU') {
            $firstSection = $text.IndexOf("`n## ")
            if ($firstSection -lt 0) { throw "$locale/$name has no first H2 section" }
            $preface = $text.Substring(0, $firstSection)
            $translationNotes = @($preface -split '(?:\r?\n){2,}' | Where-Object { $_ -match '(?i)translation|übersetz|tradu|перекладу|превода|překlad|tłumacz|vertal' })
            if ($translationNotes.Count -ne 1) { throw "$locale/$name must contain exactly one translation note before the first section; got $($translationNotes.Count)" }
        }
    }
}
$ruHelpRoot = Join-Path $root 'runtime\Help\ru-RU'
foreach ($name in @('regex-design.md', 'regex-source.md')) {
    $reference = Get-FencedBlocks (Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $ruHelpRoot $name)) "ru-RU/$name"
    foreach ($locale in $locales | Where-Object { $_ -ne 'ru-RU' }) {
        $actual = Get-FencedBlocks (Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root "runtime\Help\$locale\$name")) "$locale/$name"
        if ($actual.Count -ne $reference.Count) { throw "$locale/$name fenced-block count differs from ru-RU" }
        for ($index = 0; $index -lt $reference.Count; ++$index) {
            if ($actual[$index] -cne $reference[$index]) { throw "$locale/$name fenced block #$($index + 1) differs from ru-RU" }
        }
    }
}
$design = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-design.md')
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-source.md')
foreach ($token in @('PCRE2', 'Unicode (UCP)', 'lookaround', 'Replacement', 'greedy', 'lazy', 'possessive')) { if ($design -notmatch [regex]::Escape($token)) { throw "Design Help misses $token" } }
foreach ($token in @('Line-by-line', 'line boundar', 'MatchOnLines', '<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>', 'C++11')) { if ($source -notmatch [regex]::Escape($token)) { throw "Source Help misses $token" } }
if ($source -match '<empty-line/>\\s\*<empty-line/>|<empty-line/>.*\\r|<empty-line/>.*\\n') { throw 'The source empty-line example must remain single-line.' }
foreach ($forbidden in @('search-preset-source-scintilla-smoke.cpp', 'search-preset-design-fixtures.cpp', 'archive README', 'README архива')) {
    if (($design + "`n" + $source) -match [regex]::Escape($forbidden)) { throw "User Help must not expose internal verification detail: $forbidden" }
}
if ($readme -notmatch 'all 12 supported UI locales') { throw 'Help README must document complete localization coverage.' }
$packageManifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'packaging\package-manifest.json') | ConvertFrom-Json
$packageRequired = @($packageManifest.core.required)
foreach ($locale in $locales) {
    foreach ($name in @('regex-design.md', 'regex-source.md')) {
        $packagePath = "Help\$locale\$name"
        if ($packageRequired -cnotcontains $packagePath) { throw "Package manifest misses localized Markdown help: $packagePath" }
    }
}
foreach ($token in @('MB_ERR_INVALID_CHARS', 'MarkdownBlockKind::Title', 'MarkdownBlockKind::Heading', 'MarkdownBlockKind::List', 'MarkdownBlockKind::Code', 'MarkdownBlockKind::Regex', 'MarkdownBlockKind::Example', 'MarkdownBlockKind::Table', 'MarkdownTable', 'SplitMarkdownTableRow', 'ParseInlineCode', 'ParseMarkdownText', 'RunRuntimeSmoke', 'HelpPathForLocale', 'EM_EXLIMITTEXT', 'Malformed Markdown code block.')) { if ($parser -notmatch [regex]::Escape($token)) { throw "Markdown parser lacks $token" } }
if ($catalog.strings.PSObject.Properties.Name -contains 'fbe.regex_help.malformed_markdown') { throw 'Malformed Markdown diagnostic must remain the neutral built-in fallback until it is localized for every locale.' }
Write-Host 'Regex Help Markdown localization and package contract passed.'
