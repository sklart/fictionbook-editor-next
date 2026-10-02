[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\README.md')
foreach ($locale in @('en-US', 'ru-RU')) {
    foreach ($name in @('regex-design.md', 'regex-source.md')) {
        $path = Join-Path $root "runtime\Help\$locale\$name"
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing Markdown help: $locale/$name" }
        $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
        foreach ($required in @('# ', '## ', '```')) { if (-not $text.Contains($required)) { throw "$locale/$name misses Markdown construct $required" } }
    }
}
$design = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-design.md')
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-source.md')
foreach ($token in @('PCRE2', 'Unicode (UCP)', 'lookaround', 'Replacement', 'greedy', 'lazy', 'possessive')) { if ($design -notmatch [regex]::Escape($token)) { throw "Design Help misses $token" } }
foreach ($token in @('line by line', 'line boundaries', 'MatchOnLines', '<empty-line/>[ \t]*<empty-line/>', 'C++11')) { if ($source -notmatch [regex]::Escape($token)) { throw "Source Help misses $token" } }
if ($source -match '<empty-line/>\\s\*<empty-line/>|<empty-line/>.*\\r|<empty-line/>.*\\n') { throw 'The source empty-line example must remain single-line.' }
foreach ($language in $catalog.targetLanguages) {
    $locale = [string]$language
    $hasBoth = (Test-Path -LiteralPath (Join-Path $root "runtime\Help\$locale\regex-design.md")) -and (Test-Path -LiteralPath (Join-Path $root "runtime\Help\$locale\regex-source.md"))
    if (-not $hasBoth -and $readme -notmatch 'en-US') { throw "Missing documented fallback policy for $locale." }
}
foreach ($token in @('MB_ERR_INVALID_CHARS', 'MarkdownBlockKind::Title', 'MarkdownBlockKind::Heading', 'MarkdownBlockKind::List', 'MarkdownBlockKind::Code', 'MarkdownBlockKind::Table', 'ParseInlineCode', 'HelpPathForLocale')) { if ($parser -notmatch [regex]::Escape($token)) { throw "Markdown parser lacks $token" } }
Write-Host 'Regex Help Markdown content and fallback contract passed.'
