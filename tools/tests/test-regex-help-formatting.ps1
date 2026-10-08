[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$parser = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpMarkdown.cpp')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexHelpDialog.cpp')
foreach ($token in @('MarkdownBlockKind', 'headingLevel', 'ParseMarkdown', 'ParseInlineCode', 'ReadUtf8File', 'EM_EXLIMITTEXT', 'EM_REPLACESEL', 'WM_GETTEXTLENGTH', 'CFM_BOLD', 'CFM_SIZE', 'CFM_UNDERLINE', 'CFE_UNDERLINE', 'Consolas', 'PFM_SPACEAFTER', 'PFM_STARTINDENT', 'PFM_OFFSET', 'PFM_RIGHTINDENT', 'ThemeManager::AccentColor()', 'PointSizeForBlock', 'RenderedBlockText', 'ExpectedRenderedText', 'MakeHyperlinkCharacterFormat', 'format.dwMask = CFM_UNDERLINE | CFM_COLOR', 'preservesParentLinkFormat')) {
    if ($parser -notmatch [regex]::Escape($token)) { throw "Missing Markdown formatting behavior: $token" }
}
if ($parser -match '(?s)void RenderMarkdown\(.*?WM_GETTEXTLENGTH') { throw 'Full Help renderer must not derive formatting ranges from total text length.' }
if ($parser -notmatch '(?s)void RenderMarkdown\(.*?EM_EXGETSEL.*?EM_REPLACESEL.*?EM_EXGETSEL.*?const int last = range\.cpMax.*?EM_REPLACESEL.*?L"\\r"') { throw 'Full Help must format exact CHARRANGE text and append the paragraph delimiter separately.' }
if ($parser -match 'HelpBlockBackground|HelpTableHeaderBackground|CFM_BACKCOLOR|crBackColor') { throw 'Full Help must not create character-level background fragments.' }
if ($parser -match 'CHARFORMAT2 hyperlink = MakeCharacterFormat') { throw 'Hyperlinks must not overwrite parent formatting.' }
if ($parser -match 'MulDiv\(title \? 11 : 9, 1440, dpi\)') { throw 'RichEdit help text must not divide twip size by monitor DPI.' }
if ($parser -match 'PFM_TABSTOPS|720|1440|2160|2880') { throw 'Full Help tables must not use fixed tab stops.' }
foreach ($required in @('if (block.kind == MarkdownBlockKind::Title) return 16;', 'if (block.headingLevel == 2) return 13;', 'if (block.headingLevel == 3) return 11;', 'bodyFormat.yHeight >= 200', 'table = term + L" \x2014 " + description;', 'tableParagraph.cTabCount == 0', 'listParagraph.dxStartIndent > 0 && listParagraph.dxOffset < 0', 'automaticBackgrounds', 'backgroundReset', 'tableHeader')) {
    if ($parser -notmatch [regex]::Escape($required)) { throw "Readable Help typography contract is missing: $required" }
}
foreach($required in @('::GetWindow(m_hWnd, GW_OWNER)', '::MonitorFromWindow(owner ? owner : m_hWnd, MONITOR_DEFAULTTONEAREST)', '::MulDiv(workWidth, 80, 100)', '::MulDiv(workHeight, 80, 100)', 'info.rcWork', 'MonitorFromRect(&placement.rcNormalPosition, MONITOR_DEFAULTTONEAREST)')) {
    if($dialog -notmatch [regex]::Escape($required)) { throw "Full Help first-run/placement contract is missing: $required" }
}
foreach($required in @('WM_REGEX_HELP_RENDER = WM_APP + 211', '::PostMessage(m_hWnd, WM_REGEX_HELP_RENDER, 0, 0)', 'SetWindowTextW(text, L"Loading\x2026")', 'LRESULT OnDeferredRender', 'm_rendered', 'g_markdownCache', 'metrics->cacheHit = true')) {
    if(($dialog + $parser) -notmatch [regex]::Escape($required)) { throw "Full Help deferred rendering/cache contract is missing: $required" }
}
foreach($required in @('tableDescriptionFormat', 'tableDescriptionAt', 'standaloneDescription.szFaceName, standaloneBody.szFaceName) == 0')) {
    if ($parser -notmatch [regex]::Escape($required)) { throw "Table terms must use Consolas while descriptions retain the UI font: $required" }
}
Write-Host 'Regex Help formatting and placement contract passed.'
