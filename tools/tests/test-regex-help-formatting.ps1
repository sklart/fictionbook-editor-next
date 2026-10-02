[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpDialog.cpp')
foreach ($token in @('MarkdownBlockKind', 'ParseMarkdown', 'ParseInlineCode', 'ReadUtf8File', 'EM_REPLACESEL', 'WM_GETTEXTLENGTH', 'CFM_BOLD', 'dwEffects = bold ? CFE_BOLD : 0', 'Consolas', 'PFM_SPACEAFTER', 'PFM_STARTINDENT')) {
    if ($parser -notmatch [regex]::Escape($token)) { throw "Missing Markdown formatting behavior: $token" }
}
if ($parser -match 'JoinHelpBlocks|starts\[|ClassifyHelpLine|section ==') { throw 'Markdown renderer retains position-based or joined-text formatting.' }
if ($dialog -notmatch 'FbeRegexHelp::RenderMarkdown') { throw 'Regex Help dialog does not render parsed Markdown blocks.' }
Write-Host 'Regex Help Markdown formatting contract passed.'
