[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpDialog.cpp')
foreach ($token in @('MarkdownBlockKind', 'headingLevel', 'ParseMarkdown', 'ParseInlineCode', 'ReadUtf8File', 'EM_EXLIMITTEXT', 'EM_REPLACESEL', 'WM_GETTEXTLENGTH', 'CFM_BOLD', 'CFM_SIZE', 'CFM_BACKCOLOR', 'CFE_AUTOBACKCOLOR', 'CFM_UNDERLINE', 'CFE_UNDERLINE', 'yHeight', 'dwEffects = bold ? CFE_BOLD : 0', 'Consolas', 'PFM_SPACEAFTER', 'PFM_STARTINDENT', 'PFM_OFFSET', 'PFM_RIGHTINDENT', 'PFM_TABSTOPS', 'ThemeManager::AccentColor()', 'HelpBlockBackground', 'HelpTableHeaderBackground', 'PointSizeForBlock', 'RenderedBlockText', 'ExpectedRenderedText')) {
    if ($parser -notmatch [regex]::Escape($token)) { throw "Missing Markdown formatting behavior: $token" }
}
if ($parser -match 'JoinHelpBlocks|starts\[|ClassifyHelpLine|section ==') { throw 'Markdown renderer retains position-based or joined-text formatting.' }
if ($parser -match 'MulDiv\(title \? 11 : 9, 1440, dpi\)') { throw 'RichEdit help text must not divide its twip size by monitor DPI.' }
$render = [regex]::Match($parser, 'void\s+RenderMarkdown\(HWND richEdit, const std::vector<MarkdownBlock>& blocks\)\s*\{[\s\S]*?\n\}').Value
if ([string]::IsNullOrWhiteSpace($render)) { throw 'Unable to locate Full Regex Help renderer.' }
if ($render.IndexOf('EM_EXLIMITTEXT') -lt 0 -or $render.IndexOf('EM_REPLACESEL') -lt 0 -or $render.IndexOf('EM_EXLIMITTEXT') -gt $render.IndexOf('EM_REPLACESEL')) { throw 'RichEdit limit must be set before the first Markdown insertion.' }
foreach ($required in @('::SendMessage(richEdit, EM_EXLIMITTEXT, 0, 2 * 1024 * 1024)', 'if (kind != MarkdownBlockKind::Code) text.Trim();', 'if (block.kind == MarkdownBlockKind::List) value = CString(L"\x2022 ") + value;', 'if (block.kind == MarkdownBlockKind::Title) return 16;', 'if (block.headingLevel == 2) return 13;', 'if (block.headingLevel == 3) return 11;', 'bodyFormat.yHeight >= 200', 'tableParagraph.cTabCount >= 2', 'listParagraph.dxStartIndent > 0 && listParagraph.dxOffset < 0', 'codeFormat.dwMask & CFM_BACKCOLOR', 'inlineFormat.dwMask & CFM_BACKCOLOR', 'format.dwEffects |= CFE_AUTOBACKCOLOR', 'ThemeManager::WindowColor()', 'body-after-code', 'body-after-table', 'body-after-note', 'hasAutomaticBodyBackground', 'tableHeader')) {
    if ($parser -notmatch [regex]::Escape($required)) { throw "Readable Help font-size contract is missing: $required" }
}
if ($dialog -notmatch 'FbeRegexHelp::RenderMarkdown') { throw 'Regex Help dialog does not render parsed Markdown blocks.' }
Write-Host 'Regex Help Markdown formatting contract passed.'
