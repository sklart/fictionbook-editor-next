[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\README.md')
$locales = @($catalog.targetLanguages | ForEach-Object { [string]$_ })
foreach ($locale in $locales) {
    foreach ($name in @('regex-design.md', 'regex-source.md')) {
        $path = Join-Path $root "runtime\Help\$locale\$name"
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing localized Markdown help: $locale/$name" }
        $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $path
        foreach ($required in @('# ', '## ', '```')) { if (-not $text.Contains($required)) { throw "$locale/$name misses Markdown construct $required" } }
    }
}
$design = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-design.md')
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'runtime\Help\en-US\regex-source.md')
foreach ($token in @('PCRE2', 'Unicode (UCP)', 'lookaround', 'Replacement', 'greedy', 'lazy', 'possessive')) { if ($design -notmatch [regex]::Escape($token)) { throw "Design Help misses $token" } }
foreach ($token in @('Line-by-line', 'line boundar', 'MatchOnLines', '<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>', 'C++11')) { if ($source -notmatch [regex]::Escape($token)) { throw "Source Help misses $token" } }
if ($source -match '<empty-line/>\\s\*<empty-line/>|<empty-line/>.*\\r|<empty-line/>.*\\n') { throw 'The source empty-line example must remain single-line.' }
if ($readme -notmatch 'all 12 supported UI locales') { throw 'Help README must document complete localization coverage.' }
$packageManifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'packaging\package-manifest.json') | ConvertFrom-Json
$packageRequired = @($packageManifest.core.required)
foreach ($locale in $locales) {
    foreach ($name in @('regex-design.md', 'regex-source.md')) {
        $packagePath = "Help\$locale\$name"
        if ($packageRequired -cnotcontains $packagePath) { throw "Package manifest misses localized Markdown help: $packagePath" }
    }
}
foreach ($token in @('MB_ERR_INVALID_CHARS', 'MarkdownBlockKind::Title', 'MarkdownBlockKind::Heading', 'MarkdownBlockKind::List', 'MarkdownBlockKind::Code', 'MarkdownBlockKind::Table', 'ParseInlineCode', 'ParseMarkdownText', 'RunRuntimeSmoke', 'HelpPathForLocale')) { if ($parser -notmatch [regex]::Escape($token)) { throw "Markdown parser lacks $token" } }
Write-Host 'Regex Help Markdown localization and package contract passed.'
