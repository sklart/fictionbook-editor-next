#include "stdafx.h"
#include "RegexHelpMarkdown.h"
#include "..\\..\\..\\common\\DeploymentContext.h"
#include "..\\..\\..\\common\\RuntimeLocalizationCommon.h"
#include "..\\..\\ThemeManager.h"
#include "..\\..\\UiMetrics.h"
#include <richedit.h>
#include <string>

namespace
{
using FbeRegexHelp::MarkdownBlock;
using FbeRegexHelp::MarkdownBlockKind;
using FbeRegexHelp::MarkdownInlineCode;

struct CachedMarkdown
{
    FbeSearchPresets::SearchUiContext context;
    CString locale;
    std::vector<MarkdownBlock> blocks;
    CString sourcePath;
    bool loaded;
};

std::vector<CachedMarkdown> g_markdownCache;

bool ReadUtf8File(const CString& path, CString& text)
{
    text.Empty();
    HANDLE file = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    const bool sizeOk = ::GetFileSizeEx(file, &size) != FALSE && size.QuadPart >= 0 && size.QuadPart <= 2 * 1024 * 1024;
    if (!sizeOk) { ::CloseHandle(file); return false; }
    std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const bool readOk = bytes.empty() || (::ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &read, NULL) != FALSE && read == static_cast<DWORD>(bytes.size()));
    ::CloseHandle(file);
    if (!readOk) return false;
    const char* data = bytes.empty() ? "" : &bytes[0];
    int count = static_cast<int>(bytes.size());
    if (count >= 3 && static_cast<unsigned char>(data[0]) == 0xef && static_cast<unsigned char>(data[1]) == 0xbb && static_cast<unsigned char>(data[2]) == 0xbf) { data += 3; count -= 3; }
    if (count == 0) return true;
    const int wideCount = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, count, NULL, 0);
    if (wideCount <= 0) return false;
    wchar_t* buffer = text.GetBuffer(wideCount);
    const int converted = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, count, buffer, wideCount);
    text.ReleaseBuffer(converted == wideCount ? wideCount : 0);
    return converted == wideCount;
}

void ParseInlineCode(CString& text, std::vector<MarkdownInlineCode>& spans)
{
    CString plain;
    bool inCode = false;
    int start = 0;
    for (int index = 0; index < text.GetLength(); ++index)
    {
        if (text[index] == L'`')
        {
            if (inCode) { spans.push_back(MarkdownInlineCode{ start, plain.GetLength() - start }); }
            else { start = plain.GetLength(); }
            inCode = !inCode;
            continue;
        }
        plain += text[index];
    }
    text = plain;
}

CString TrimMarkdownTableCell(const CString& value)
{
    CString result(value); result.Trim();
    if (!result.IsEmpty() && result[0] == L'|') result = result.Mid(1);
    if (!result.IsEmpty() && result[result.GetLength() - 1] == L'|') result = result.Left(result.GetLength() - 1);
    result.Trim();
    return result;
}

std::vector<CString> SplitMarkdownTableRow(const CString& value)
{
    CString row(TrimMarkdownTableCell(value));
    std::vector<CString> cells;
    int start = 0;
    while (start <= row.GetLength())
    {
        int end = row.Find(L'|', start);
        if (end < 0) end = row.GetLength();
        CString cell(row.Mid(start, end - start)); cell.Trim();
        cells.push_back(cell);
        if (end == row.GetLength()) break;
        start = end + 1;
    }
    return cells;
}

bool IsMarkdownTableSeparator(const CString& line)
{
    if (line.Find(L'|') < 0) return false;
    for (int index = 0; index < line.GetLength(); ++index)
        if (line[index] != L'|' && line[index] != L'-' && line[index] != L':' && line[index] != L' ' && line[index] != L'\t') return false;
    return line.Find(L'-') >= 0;
}

void AddBlock(std::vector<MarkdownBlock>& blocks, MarkdownBlockKind kind, const CString& value, int headingLevel = 0)
{
    CString text(value);
    // Fenced code is literal content. In particular, its indentation, trailing
    // whitespace, tabs and empty lines are part of an example's meaning.
    if (kind != MarkdownBlockKind::Code && kind != MarkdownBlockKind::Regex && kind != MarkdownBlockKind::Example) text.Trim();
    if (text.IsEmpty() && kind != MarkdownBlockKind::Code && kind != MarkdownBlockKind::Regex && kind != MarkdownBlockKind::Example) return;
    MarkdownBlock block = {}; block.kind = kind; block.headingLevel = headingLevel; block.text = text;
    if (kind != MarkdownBlockKind::Code) ParseInlineCode(block.text, block.inlineCode);
    blocks.push_back(block);
}

void AddTable(std::vector<MarkdownBlock>& blocks, const CString& header, const std::vector<CString>& lines)
{
    MarkdownBlock block = {}; block.kind = MarkdownBlockKind::Table;
    block.table.headers = SplitMarkdownTableRow(header);
    for (size_t index = 0; index < lines.size(); ++index)
    {
        std::vector<CString> row = SplitMarkdownTableRow(lines[index]);
        if (row.size() == block.table.headers.size()) block.table.rows.push_back(row);
    }
    if (block.table.headers.empty()) return;
    const auto appendRow = [&](const std::vector<CString>& row) {
        if (!block.text.IsEmpty()) block.text += L"\n";
        for (size_t column = 0; column < row.size(); ++column)
        {
            if (column) block.text += L"\t";
            CString cell(row[column]);
            std::vector<MarkdownInlineCode> spans;
            ParseInlineCode(cell, spans);
            const int cellStart = block.text.GetLength();
            block.text += cell;
            for (size_t span = 0; span < spans.size(); ++span)
                block.inlineCode.push_back(MarkdownInlineCode{ cellStart + spans[span].start, spans[span].length });
        }
    };
    appendRow(block.table.headers);
    for (size_t index = 0; index < block.table.rows.size(); ++index) appendRow(block.table.rows[index]);
    blocks.push_back(block);
}

void ParseMarkdown(const CString& source, std::vector<MarkdownBlock>& blocks)
{
    blocks.clear();
    bool codeFence = false;
    MarkdownBlockKind directiveKind = MarkdownBlockKind::Body;
    MarkdownBlockKind codeFenceKind = MarkdownBlockKind::Code;
    bool directiveFence = false;
    bool directiveWarning = false;
    bool codeHasLine = false;
    CString code;
    CString paragraph;
    std::vector<CString> lines;
    int start = 0;
    while (start <= source.GetLength())
    {
        int end = source.Find(L'\n', start);
        if (end < 0) end = source.GetLength();
        CString line = source.Mid(start, end - start); line.TrimRight(L'\r');
        lines.push_back(line);
        if (end == source.GetLength()) break;
        start = end + 1;
    }
    const auto flushParagraph = [&]() { AddBlock(blocks, MarkdownBlockKind::Body, paragraph); paragraph.Empty(); };
    for (size_t index = 0; index < lines.size(); ++index)
    {
        const CString& line = lines[index];
        CString trimmed(line); trimmed.Trim();
        if (trimmed.Left(3) == L":::")
        {
            if (!directiveFence)
            {
                flushParagraph();
                CString directive(trimmed.Mid(3)); directive.MakeLower(); directive.Trim();
                if (directive == L"regex") { directiveKind = MarkdownBlockKind::Regex; directiveWarning = false; directiveFence = true; code.Empty(); codeHasLine = false; continue; }
                if (directive == L"example") { directiveKind = MarkdownBlockKind::Example; directiveWarning = false; directiveFence = true; code.Empty(); codeHasLine = false; continue; }
                if (directive == L"note" || directive == L"warning") { directiveKind = MarkdownBlockKind::Note; directiveWarning = directive == L"warning"; directiveFence = true; code.Empty(); codeHasLine = false; continue; }
            }
            else if (trimmed == L":::")
            {
                AddBlock(blocks, directiveKind, code);
                if (directiveKind == MarkdownBlockKind::Note && !blocks.empty()) blocks.back().warning = directiveWarning;
                code.Empty(); codeHasLine = false; directiveFence = false; continue;
            }
        }
        if (directiveFence) { if (codeHasLine) code += L"\n"; code += line; codeHasLine = true; continue; }
        if (line.Left(3) == L"```")
        {
            flushParagraph();
            if (codeFence) { AddBlock(blocks, codeFenceKind, code); code.Empty(); codeHasLine = false; }
            else
            {
                CString fenceKind(trimmed.Mid(3)); fenceKind.MakeLower(); fenceKind.Trim();
                codeFenceKind = fenceKind == L"regex" ? MarkdownBlockKind::Regex : fenceKind == L"example" ? MarkdownBlockKind::Example : MarkdownBlockKind::Code;
            }
            codeFence = !codeFence;
            continue;
        }
        if (codeFence) { if (codeHasLine) code += L"\n"; code += line; codeHasLine = true; continue; }
        if (trimmed.IsEmpty()) { flushParagraph(); continue; }
        if (trimmed.Left(2) == L"> ") { flushParagraph(); AddBlock(blocks, MarkdownBlockKind::Note, trimmed.Mid(2)); continue; }
        int hashes = 0; while (hashes < trimmed.GetLength() && trimmed[hashes] == L'#') ++hashes;
        if (hashes > 0 && hashes <= 3 && hashes < trimmed.GetLength() && trimmed[hashes] == L' ')
        {
            flushParagraph(); AddBlock(blocks, hashes == 1 ? MarkdownBlockKind::Title : MarkdownBlockKind::Heading, trimmed.Mid(hashes + 1), hashes); continue;
        }
        if (trimmed.Left(2) == L"- ") { flushParagraph(); AddBlock(blocks, MarkdownBlockKind::List, trimmed.Mid(2)); continue; }
        if (trimmed.Find(L'|') >= 0 && index + 1 < lines.size() && IsMarkdownTableSeparator(lines[index + 1]))
        {
            flushParagraph();
            std::vector<CString> rows;
            index += 2;
            while (index < lines.size() && lines[index].Find(L'|') >= 0 && !lines[index].Trim().IsEmpty()) { rows.push_back(lines[index]); ++index; }
            --index;
            AddTable(blocks, trimmed, rows); continue;
        }
        if (!paragraph.IsEmpty()) paragraph += L" ";
        paragraph += trimmed;
    }
    if (codeFence || directiveFence) AddBlock(blocks, MarkdownBlockKind::Note, L"Malformed Markdown code block.");
    flushParagraph();
}

CString HelpFileName(FbeSearchPresets::SearchUiContext context)
{
    return context == FbeSearchPresets::SearchUiContext::Source ? L"regex-source.md" : L"regex-design.md";
}

CString HelpPathForLocale(LPCWSTR locale, const CString& fileName)
{
    CString path(DeploymentContext::ExecutableDirectory().c_str());
    path += L"Help\\"; path += locale; path += L"\\"; path += fileName;
    return path;
}

void SelectAndFormat(HWND richEdit, int start, int end, const CHARFORMAT2& format, const PARAFORMAT2& paragraph)
{
    ::SendMessage(richEdit, EM_SETSEL, start, end);
    ::SendMessage(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    ::SendMessage(richEdit, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraph));
}

CHARFORMAT2 MakeCharacterFormat(HWND richEdit, bool bold, bool monospace, int pointSize)
{
    CHARFORMAT2 format = {}; format.cbSize = sizeof(format); format.dwMask = CFM_BOLD | CFM_FACE | CFM_COLOR | CFM_SIZE;
    format.dwEffects = bold ? CFE_BOLD : 0; format.crTextColor = ThemeManager::TextColor();
    HDC dc = richEdit ? ::GetDC(richEdit) : NULL;
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ::ReleaseDC(richEdit, dc);
    // CHARFORMAT2::yHeight is expressed in twips, not device pixels. RichEdit performs the DPI conversion.
    // Dividing by monitor DPI made the Help text smaller on high-DPI displays.
    (void)dpi;
    format.yHeight = pointSize * 20;
    if (monospace) ::lstrcpynW(format.szFaceName, L"Consolas", LF_FACESIZE);
    else { HFONT font = UiMetrics::DialogFont(); LOGFONTW logFont = {}; if (font && ::GetObjectW(font, sizeof(logFont), &logFont)) ::lstrcpynW(format.szFaceName, logFont.lfFaceName, LF_FACESIZE); }
    return format;
}
CHARFORMAT2 MakeHyperlinkCharacterFormat()
{
    CHARFORMAT2 format = {}; format.cbSize = sizeof(format);
    // A hyperlink is an inline decoration. Keep the parent block's face, size,
    // weight and background (Code/Table/Note) untouched.
    format.dwMask = CFM_UNDERLINE | CFM_COLOR;
    format.dwEffects = CFE_UNDERLINE;
    format.crTextColor = ThemeManager::AccentColor();
    return format;
}

PARAFORMAT2 MakeParagraphFormat(const MarkdownBlock& block)
{
    PARAFORMAT2 paragraph = {}; paragraph.cbSize = sizeof(paragraph); paragraph.dwMask = PFM_SPACEAFTER;
    paragraph.dySpaceAfter = block.kind == MarkdownBlockKind::Title ? 220 : block.headingLevel == 2 ? 100 : block.headingLevel == 3 ? 60 : 40;
    if (block.headingLevel == 2) { paragraph.dwMask |= PFM_SPACEBEFORE; paragraph.dySpaceBefore = 180; }
    if (block.headingLevel == 3) { paragraph.dwMask |= PFM_SPACEBEFORE; paragraph.dySpaceBefore = 100; }
    if (block.kind == MarkdownBlockKind::List) { paragraph.dwMask |= PFM_STARTINDENT | PFM_OFFSET; paragraph.dxStartIndent = 240; paragraph.dxOffset = -120; }
    if (block.kind == MarkdownBlockKind::Code || block.kind == MarkdownBlockKind::Regex || block.kind == MarkdownBlockKind::Example)
    {
        paragraph.dwMask |= PFM_STARTINDENT | PFM_RIGHTINDENT | PFM_SPACEBEFORE;
        paragraph.dxStartIndent = 140;
        paragraph.dxRightIndent = 140;
        paragraph.dySpaceBefore = 60;
        paragraph.dySpaceAfter = 60;
    }
    if (block.kind == MarkdownBlockKind::Table) { paragraph.dwMask |= PFM_STARTINDENT | PFM_SPACEBEFORE; paragraph.dxStartIndent = 140; paragraph.dySpaceBefore = 40; paragraph.dySpaceAfter = 40; }
    if (block.kind == MarkdownBlockKind::Note)
    {
        paragraph.dwMask |= PFM_STARTINDENT | PFM_RIGHTINDENT | PFM_SPACEBEFORE;
        paragraph.dxStartIndent = 360;
        paragraph.dxRightIndent = 140;
        paragraph.dySpaceBefore = 60;
        paragraph.dySpaceAfter = 60;
    }
    return paragraph;
}

int PointSizeForBlock(const MarkdownBlock& block)
{
    if (block.kind == MarkdownBlockKind::Title) return 16;
    if (block.headingLevel == 2) return 13;
    if (block.headingLevel == 3) return 11;
    return 10;
}

CString RenderedBlockText(const MarkdownBlock& block)
{
    CString value(block.text);
    if (block.kind == MarkdownBlockKind::List) value = CString(L"\x2022 ") + value;
    if (block.kind == MarkdownBlockKind::Note) value = CString(block.warning ? L"\x26A0 " : L"\x2139 ") + value;
    return value;
}

CString ExpectedRenderedText(const std::vector<MarkdownBlock>& blocks)
{
    CString expected;
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        CString value(RenderedBlockText(blocks[index]));
        // RichEdit represents literal LF inside a fenced block as CR/LF.
        value.Replace(L"\n", L"\r\n");
        expected += value;
        expected += L"\r\n";
        if (blocks[index].kind == MarkdownBlockKind::Table) expected += L"\r\n\r\n";
    }
    return expected;
}

bool ReadRichEditText(HWND richEdit, CString& text)
{
    const int length = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0));
    wchar_t* buffer = text.GetBuffer(length + 1);
    const int copied = ::GetWindowTextW(richEdit, buffer, length + 1);
    text.ReleaseBuffer(copied);
    // Native RichEdit table row/cell delimiters count differently in
    // WM_GETTEXTLENGTH and GetWindowText. The copied visible text is valid.
    return copied >= 0;
}

void AppendRtfText(CStringA& rtf, const CString& text)
{
    for (int index = 0; index < text.GetLength(); ++index)
    {
        const wchar_t ch = text[index];
        if (ch == L'\\' || ch == L'{' || ch == L'}') { rtf += '\\'; rtf += static_cast<char>(ch); }
        else if (ch == L'\n') rtf += "\\line ";
        else if (ch == L'\t') rtf += "\\tab ";
        else if (ch >= 32 && ch < 127) rtf += static_cast<char>(ch);
        else if (ch >= 32) rtf.AppendFormat("\\u%d?", static_cast<int>(static_cast<short>(ch)));
    }
}

void AppendRtfCellText(CStringA& rtf, const CString& raw, bool firstColumn)
{
    CString text(raw);
    std::vector<MarkdownInlineCode> codeSpans;
    ParseInlineCode(text, codeSpans);
    bool inCode = false, inLink = false;
    for (int index = 0; index < text.GetLength(); ++index)
    {
        bool code = false;
        for (size_t span = 0; span < codeSpans.size(); ++span)
            if (index >= codeSpans[span].start && index < codeSpans[span].start + codeSpans[span].length) { code = true; break; }
        if (code != inCode) { rtf += code || firstColumn ? "\\f1 " : "\\f0 "; inCode = code; }
        const bool startsLink = text.Mid(index, 4).CompareNoCase(L"http") == 0;
        if (!inLink && startsLink) { rtf += "\\ul\\cf2 "; inLink = true; }
        if (inLink && (text[index] == L' ' || text[index] == L')' || text[index] == L'\n'))
        {
            rtf += "\\ulnone\\cf1 "; inLink = false;
        }
        AppendRtfText(rtf, text.Mid(index, 1));
    }
    if (inLink) rtf += "\\ulnone\\cf1 ";
}

CStringA BuildTableRtf(HWND richEdit, const MarkdownBlock& block)
{
    const size_t columnCount = block.table.headers.size();
    RECT area = {};
    ::SendMessage(richEdit, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&area));
    if (area.right <= area.left) ::GetClientRect(richEdit, &area);
    HDC dc = ::GetDC(richEdit);
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ::ReleaseDC(richEdit, dc);
    const int widthPixels = (std::max)(120, static_cast<int>(area.right - area.left - 16));
    const int width = (std::max)(900, ::MulDiv(widthPixels, 1440, dpi));
    const int firstWidth = columnCount == 3 ? width * 26 / 100 : width * 32 / 100;
    std::vector<int> edges(columnCount);
    for (size_t column = 0; column < columnCount; ++column)
        edges[column] = column == 0 ? firstWidth : firstWidth + (width - firstWidth) * static_cast<int>(column) / static_cast<int>(columnCount - 1);

    LOGFONTW logFont = {};
    HFONT uiFont = UiMetrics::DialogFont();
    const CString face(uiFont && ::GetObjectW(uiFont, sizeof(logFont), &logFont) ? logFont.lfFaceName : L"Segoe UI");
    const COLORREF ink = ThemeManager::TextColor(), accent = ThemeManager::AccentColor();
    CStringA rtf("{\\rtf1\\ansi\\ansicpg1252\\uc1\\deff0{\\fonttbl{\\f0\\fnil ");
    AppendRtfText(rtf, face);
    rtf += ";}{\\f1\\fmodern Consolas;}}";
    rtf.AppendFormat("{\\colortbl;\\red%d\\green%d\\blue%d;\\red%d\\green%d\\blue%d;\\red%d\\green%d\\blue%d;}",
        GetRValue(ink), GetGValue(ink), GetBValue(ink), GetRValue(accent), GetGValue(accent), GetBValue(accent),
        160, 160, 160);
    for (size_t rowIndex = 0; rowIndex <= block.table.rows.size(); ++rowIndex)
    {
        const std::vector<CString>& row = rowIndex == 0 ? block.table.headers : block.table.rows[rowIndex - 1];
        rtf += "\\trowd\\trgaph80\\trleft0";
        for (size_t column = 0; column < columnCount; ++column)
            rtf.AppendFormat("\\clbrdrt\\brdrs\\brdrw6\\brdrcf3\\clbrdrl\\brdrs\\brdrw6\\brdrcf3\\clbrdrb\\brdrs\\brdrw6\\brdrcf3\\clbrdrr\\brdrs\\brdrw6\\brdrcf3\\cellx%d", edges[column]);
        for (size_t column = 0; column < columnCount; ++column)
        {
            rtf.AppendFormat("\\pard\\intbl\\cf1\\f%d\\fs20%s ", column == 0 ? 1 : 0, rowIndex == 0 ? "\\b" : "\\b0");
            AppendRtfCellText(rtf, row[column], column == 0);
            rtf += "\\cell ";
        }
        rtf += "\\row ";
    }
    rtf += "\\pard\\par}";
    return rtf;
}

struct TableRtfInput { const char* data; size_t length; size_t offset; };
DWORD CALLBACK ReadTableRtf(DWORD_PTR cookie, LPBYTE buffer, LONG requested, LONG* copied)
{
    TableRtfInput& input = *reinterpret_cast<TableRtfInput*>(cookie);
    const size_t count = (std::min)(static_cast<size_t>(requested), input.length - input.offset);
    if (count) memcpy(buffer, input.data + input.offset, count);
    input.offset += count;
    *copied = static_cast<LONG>(count);
    return 0;
}

bool ReplaceTableWithRtf(HWND richEdit, const MarkdownBlock& block, int first, int last)
{
    if (block.table.headers.size() < 2 || block.table.headers.size() > 3) return false;
    CStringA rtf = BuildTableRtf(richEdit, block);
    TableRtfInput input = { rtf.GetString(), static_cast<size_t>(rtf.GetLength()), 0 };
    EDITSTREAM stream = { reinterpret_cast<DWORD_PTR>(&input), 0, ReadTableRtf };
    ::SendMessage(richEdit, EM_SETSEL, first, last);
    ::SendMessage(richEdit, EM_STREAMIN, SF_RTF | SFF_SELECTION, reinterpret_cast<LPARAM>(&stream));
    return stream.dwError == 0 && input.offset == input.length;
}
}

namespace FbeRegexHelp
{
bool LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext context, LPCWSTR requestedLocale, std::vector<MarkdownBlock>& blocks, CString& sourcePath, MarkdownLoadMetrics* metrics)
{
    if (metrics) *metrics = MarkdownLoadMetrics{};
    sourcePath.Empty(); blocks.clear();
    const CString fileName = HelpFileName(context);
    CString locale(requestedLocale ? requestedLocale : L"");
    CString markdown;
    const CString localized = HelpPathForLocale(locale, fileName);
    ULONGLONG started = ::GetTickCount64();
    const bool localizedRead = ReadUtf8File(localized, markdown);
    if (metrics) metrics->markdownReadMs += ::GetTickCount64() - started;
    if (localizedRead && !markdown.IsEmpty())
    {
        sourcePath = localized; started = ::GetTickCount64(); ParseMarkdown(markdown, blocks);
        if (metrics) { metrics->parseMs += ::GetTickCount64() - started; metrics->blockCount = blocks.size(); }
        return !blocks.empty();
    }
    const CString fallback = HelpPathForLocale(L"en-US", fileName);
    started = ::GetTickCount64();
    const bool fallbackRead = ReadUtf8File(fallback, markdown);
    if (metrics) metrics->markdownReadMs += ::GetTickCount64() - started;
    if (fallbackRead && !markdown.IsEmpty())
    {
        sourcePath = fallback; started = ::GetTickCount64(); ParseMarkdown(markdown, blocks);
        if (metrics) { metrics->parseMs += ::GetTickCount64() - started; metrics->blockCount = blocks.size(); }
        return !blocks.empty();
    }
    AddBlock(blocks, MarkdownBlockKind::Title, context == FbeSearchPresets::SearchUiContext::Source ? L"Source regular expression help" : L"Regular expression help");
    AddBlock(blocks, MarkdownBlockKind::Note, L"Help file was not found.");
    if (metrics) metrics->blockCount = blocks.size();
    return false;
}

bool LoadMarkdown(FbeSearchPresets::SearchUiContext context, std::vector<MarkdownBlock>& blocks, CString& sourcePath, MarkdownLoadMetrics* metrics)
{
    const CString locale(FbeRuntimeLocalization::GetPreferredRuntimeLocaleName());
    for (size_t index = 0; index < g_markdownCache.size(); ++index)
    {
        const CachedMarkdown& cached = g_markdownCache[index];
        if (cached.context == context && cached.locale.CompareNoCase(locale) == 0)
        {
            blocks = cached.blocks;
            sourcePath = cached.sourcePath;
            if (metrics) { *metrics = MarkdownLoadMetrics{}; metrics->blockCount = blocks.size(); metrics->cacheHit = true; }
            return cached.loaded;
        }
    }

    CachedMarkdown cached = {};
    cached.context = context;
    cached.locale = locale;
    cached.loaded = LoadMarkdownForLocale(context, locale, cached.blocks, cached.sourcePath, metrics);
    blocks = cached.blocks;
    sourcePath = cached.sourcePath;
    g_markdownCache.push_back(cached);
    return cached.loaded;
}
void ParseMarkdownText(const CString& text, std::vector<MarkdownBlock>& blocks)
{
    ParseMarkdown(text, blocks);
}
void RenderMarkdown(HWND richEdit, const std::vector<MarkdownBlock>& blocks)
{
    if (!richEdit) return;
    // Full help documents exceed the legacy 32K RichEdit default. Set this
    // before any text is inserted, rather than relying on a control default.
    ::SendMessage(richEdit, EM_EXLIMITTEXT, 0, 2 * 1024 * 1024);
    ::SetWindowTextW(richEdit, L"");
    struct RenderedRange { int first; int last; };
    std::vector<RenderedRange> ranges;
    ranges.reserve(blocks.size());
    // Insert all plain text first. Formatting a completed document avoids
    // inheriting a previous block's character attributes at its delimiter.
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        const MarkdownBlock& block = blocks[index];
        CHARRANGE range = {};
        ::SendMessage(richEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        const int first = range.cpMin;
        CString value(RenderedBlockText(block));
        ::SendMessage(richEdit, EM_SETSEL, first, first);
        ::SendMessage(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(value)));
        ::SendMessage(richEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        const int last = range.cpMax;
        ranges.push_back(RenderedRange{ first, last });
        ::SendMessage(richEdit, EM_SETSEL, last, last);
        ::SendMessage(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r"));
    }
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        const MarkdownBlock& block = blocks[index];
        const int first = ranges[index].first, last = ranges[index].last;
        CString value(RenderedBlockText(block));
        const bool bold = block.kind == MarkdownBlockKind::Title || block.kind == MarkdownBlockKind::Heading;
        const bool monospace = block.kind == MarkdownBlockKind::Code || block.kind == MarkdownBlockKind::Regex || block.kind == MarkdownBlockKind::Example;
        const CHARFORMAT2 format = MakeCharacterFormat(richEdit, bold, monospace, PointSizeForBlock(block));
        const PARAFORMAT2 paragraph = MakeParagraphFormat(block);
        SelectAndFormat(richEdit, first, (std::max)(first, last), format, paragraph);
        if (block.kind == MarkdownBlockKind::Table) continue;
        for (size_t span = 0; span < block.inlineCode.size(); ++span)
        {
            CHARFORMAT2 code = MakeCharacterFormat(richEdit, false, true, 10);
            ::SendMessage(richEdit, EM_SETSEL, first + block.inlineCode[span].start, first + block.inlineCode[span].start + block.inlineCode[span].length);
            ::SendMessage(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&code));
        }
        for (int link = value.Find(L"http"); link >= 0; )
        {
            int end = link; while (end < value.GetLength() && value[end] != L' ' && value[end] != L')' && value[end] != L'\r' && value[end] != L'\n') ++end;
            const CHARFORMAT2 hyperlink = MakeHyperlinkCharacterFormat();
            ::SendMessage(richEdit, EM_SETSEL, first + link, first + end);
            ::SendMessage(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&hyperlink));
            link = value.Find(L"http", end);
        }
    }
    // Work backwards so the native table delimiters do not invalidate the
    // plain-text ranges of earlier blocks. RichEdit owns the cell wrapping.
    for (size_t index = blocks.size(); index > 0; --index)
        if (blocks[index - 1].kind == MarkdownBlockKind::Table)
            ReplaceTableWithRtf(richEdit, blocks[index - 1], ranges[index - 1].first, ranges[index - 1].last);
    ::SendMessage(richEdit, EM_SETSEL, 0, 0);
    ::SendMessage(richEdit, EM_SCROLLCARET, 0, 0);
}
bool RunRuntimeSmoke(HWND owner, CStringA& report)
{
    report.Empty();
    std::vector<MarkdownBlock> enDesign, enSource, ruDesign, ruSource, fallback;
    CString enDesignPath, enSourcePath, ruDesignPath, ruSourcePath, fallbackPath;
    const bool designLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"en-US", enDesign, enDesignPath);
    const bool sourceLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Source, L"en-US", enSource, enSourcePath);
    const bool ruDesignLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"ru-RU", ruDesign, ruDesignPath);
    const bool ruSourceLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Source, L"ru-RU", ruSource, ruSourcePath);
    const bool fallbackLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"zz-ZZ", fallback, fallbackPath) && fallbackPath.Find(L"Help\\en-US\\regex-design.md") >= 0;
    const bool content = !enDesign.empty() && !enSource.empty() && !ruDesign.empty() && !ruSource.empty() &&
        enDesign[0].kind == MarkdownBlockKind::Title && enSource[0].kind == MarkdownBlockKind::Title;
    const auto hasProductionBlocks = [](const std::vector<MarkdownBlock>& blocks, size_t minimumExamples) {
        size_t examples = 0, notes = 0, warnings = 0;
        for (const MarkdownBlock& block : blocks)
        {
            if (block.kind == MarkdownBlockKind::Example) ++examples;
            if (block.kind == MarkdownBlockKind::Note) block.warning ? ++warnings : ++notes;
        }
        return examples >= minimumExamples && notes >= 1 && warnings >= 1;
    };
    const bool productionBlocks = hasProductionBlocks(enDesign, 3) && hasProductionBlocks(ruDesign, 3) &&
        hasProductionBlocks(enSource, 1) && hasProductionBlocks(ruSource, 1);
    std::vector<MarkdownBlock> cachedFirst, cachedSecond;
    CString cachedFirstPath, cachedSecondPath;
    MarkdownLoadMetrics firstLoad, secondLoad;
    const bool cached = LoadMarkdown(FbeSearchPresets::SearchUiContext::Design, cachedFirst, cachedFirstPath, &firstLoad) &&
        LoadMarkdown(FbeSearchPresets::SearchUiContext::Design, cachedSecond, cachedSecondPath, &secondLoad) &&
        !cachedFirst.empty() && cachedFirst.size() == cachedSecond.size() && cachedFirstPath == cachedSecondPath &&
        secondLoad.cacheHit && secondLoad.markdownReadMs == 0 && secondLoad.parseMs == 0;
    std::vector<MarkdownBlock> parsed, empty, malformed, unknown;
    ParseMarkdownText(L"# Title\n## Heading two\n### Heading three\nBody `inline` text\n- one\n- two\n```text\n   leading\ntrailing   \n\ttab\n\nlast\n```\n:::regex\n\\d{2,4}\n:::\n:::example\n2026\n1234\n:::\n:::warning\nRegex is text based.\n:::\n:::note\nInformation is visible.\n:::\n| Syntax | Meaning |\n| --- | --- |\n| `\\d` | `digit` |\n| `\\w` | word |\nBody after table https://example.invalid", parsed);
    ParseMarkdownText(L"", empty);
    ParseMarkdownText(L"```regex\n[", malformed);
    ParseMarkdownText(L"> unknown extension", unknown);
    bool hasTitle = false, hasHeading2 = false, hasHeading3 = false, hasBody = false, hasList = false, hasCode = false, hasRegex = false, hasExample = false, hasNote = false, hasInformation = false, hasWarning = false, hasTable = false, hasInlineCode = false, codeWhitespace = false, tableStructured = false, tableInlineCode = false;
    for (size_t index = 0; index < parsed.size(); ++index)
    {
        const MarkdownBlock& block = parsed[index];
        hasTitle = hasTitle || block.kind == MarkdownBlockKind::Title;
        hasHeading2 = hasHeading2 || (block.kind == MarkdownBlockKind::Heading && block.headingLevel == 2);
        hasHeading3 = hasHeading3 || (block.kind == MarkdownBlockKind::Heading && block.headingLevel == 3);
        hasBody = hasBody || block.kind == MarkdownBlockKind::Body;
        hasList = hasList || block.kind == MarkdownBlockKind::List;
        hasCode = hasCode || block.kind == MarkdownBlockKind::Code;
        hasRegex = hasRegex || (block.kind == MarkdownBlockKind::Regex && block.text == L"\\d{2,4}");
        hasExample = hasExample || (block.kind == MarkdownBlockKind::Example && block.text == L"2026\n1234");
        hasNote = hasNote || (block.kind == MarkdownBlockKind::Note && block.text == L"Regex is text based.");
        hasInformation = hasInformation || (block.kind == MarkdownBlockKind::Note && !block.warning && block.text == L"Information is visible.");
        hasWarning = hasWarning || (block.kind == MarkdownBlockKind::Note && block.warning);
        codeWhitespace = codeWhitespace || (block.kind == MarkdownBlockKind::Code && block.text == L"   leading\ntrailing   \n\ttab\n\nlast");
        hasTable = hasTable || block.kind == MarkdownBlockKind::Table;
        tableStructured = tableStructured || (block.kind == MarkdownBlockKind::Table && block.table.headers.size() == 2 && block.table.rows.size() == 2);
        tableInlineCode = tableInlineCode || (block.kind == MarkdownBlockKind::Table && !block.inlineCode.empty());
        hasInlineCode = hasInlineCode || !block.inlineCode.empty();
    }
    const bool parser = hasTitle && hasHeading2 && hasHeading3 && hasBody && hasList && hasCode && hasRegex && hasExample && hasNote && hasInformation && hasWarning && hasTable && tableStructured && tableInlineCode && hasInlineCode && codeWhitespace &&
        empty.empty() && malformed.size() == 1 && malformed[0].text == L"Malformed Markdown code block." && !unknown.empty();
    HMODULE richEditLibrary = ::LoadLibraryW(L"Msftedit.dll");
    HWND richEdit = richEditLibrary ? ::CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_POPUP | ES_MULTILINE, 0, 0, 16, 16, owner, NULL, NULL, NULL) : NULL;
    bool formatting = false, longDocuments = false;
    int facesDetail = 0;
    int formattingDetail = 0, shadedLinkDetail = 0, styleDetail = 0;
    int enDesignLength = 0, enSourceLength = 0, ruDesignLength = 0, ruSourceLength = 0;
    int enDesignExpectedLength = 0, enSourceExpectedLength = 0, ruDesignExpectedLength = 0, ruSourceExpectedLength = 0;
    bool enDesignTerminalNewlineOmitted = false, enSourceTerminalNewlineOmitted = false;
    bool ruDesignTerminalNewlineOmitted = false, ruSourceTerminalNewlineOmitted = false;
    if (richEdit)
    {
        const auto renderFullDocument = [richEdit](const std::vector<MarkdownBlock>& blocks, int& length, int& expectedLength,
            bool& terminalNewlineOmitted) -> bool {
            RenderMarkdown(richEdit, blocks);
            CString rendered;
            if (!ReadRichEditText(richEdit, rendered)) return false;
            const CString expected = ExpectedRenderedText(blocks);
            length = rendered.GetLength();
            expectedLength = expected.GetLength();
            // Msftedit versions differ only in whether GetWindowText exposes
            // the LF of the final paragraph delimiter. Accept that single
            // terminal representation, while retaining an exact comparison
            // for every preceding rendered character.
            terminalNewlineOmitted = length + 1 == expectedLength && expected.Left(length) == rendered && expected[length] == L'\n';
            const CString& lastMeaningful = blocks.back().text;
            return length > 32767 && (rendered == expected || terminalNewlineOmitted) && rendered.Find(lastMeaningful) >= 0;
        };
        const bool enDesignComplete = renderFullDocument(enDesign, enDesignLength, enDesignExpectedLength, enDesignTerminalNewlineOmitted);
        const bool enSourceComplete = renderFullDocument(enSource, enSourceLength, enSourceExpectedLength, enSourceTerminalNewlineOmitted);
        const bool ruDesignComplete = renderFullDocument(ruDesign, ruDesignLength, ruDesignExpectedLength, ruDesignTerminalNewlineOmitted);
        const bool ruSourceComplete = renderFullDocument(ruSource, ruSourceLength, ruSourceExpectedLength, ruSourceTerminalNewlineOmitted);
        longDocuments = enDesignComplete && enSourceComplete && ruDesignComplete && ruSourceComplete;

        RenderMarkdown(richEdit, parsed);
        auto formatAt = [richEdit](int position, CHARFORMAT2& format) -> bool {
            format = {}; format.cbSize = sizeof(format);
            ::SendMessage(richEdit, EM_SETSEL, position, position + 1);
            return ::SendMessage(richEdit, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format)) != 0;
        };
        auto paragraphAt = [richEdit](int position, PARAFORMAT2& paragraph) -> bool {
            paragraph = {}; paragraph.cbSize = sizeof(paragraph);
            ::SendMessage(richEdit, EM_SETSEL, position, position);
            return ::SendMessage(richEdit, EM_GETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraph)) != 0;
        };
        const auto findNative = [richEdit](LPCWSTR needle) -> int {
            FINDTEXTEXW match = {};
            match.chrg.cpMax = -1;
            match.lpstrText = const_cast<LPWSTR>(needle);
            return static_cast<int>(::SendMessage(richEdit, EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&match)));
        };
        CString rendered; ReadRichEditText(richEdit, rendered);
        CHARFORMAT2 titleFormat = {}, heading2Format = {}, heading3Format = {}, bodyFormat = {}, codeFormat = {}, regexFormat = {}, exampleFormat = {}, noteFormat = {}, warningFormat = {}, tableFormat = {}, tableDescriptionFormat = {}, tableDataFormat = {}, inlineFormat = {}, afterTableFormat = {}, linkFormat = {};
        PARAFORMAT2 heading2Paragraph = {}, heading3Paragraph = {}, exampleParagraph = {}, noteParagraph = {}, warningParagraph = {}, tableParagraph = {}, listParagraph = {};
        const int titleAt = findNative(L"Title"), heading2At = findNative(L"Heading two"), heading3At = findNative(L"Heading three"), bodyAt = findNative(L"Body"),
            codeAt = findNative(L"   leading"), regexAt = findNative(L"\\d{2,4}"), exampleAt = findNative(L"2026"), noteAt = findNative(L"Information is visible."), warningAt = findNative(L"Regex is text based."), tableAt = findNative(L"Syntax"), tableDescriptionAt = findNative(L"Meaning"), tableDataAt = findNative(L"\\d"), inlineAt = findNative(L"inline"), afterTableAt = findNative(L"Body after table"), linkAt = findNative(L"https://example.invalid"), firstBullet = findNative(L"\x2022 one"), secondBullet = findNative(L"\x2022 two");
        const bool positions = titleAt >= 0 && heading2At >= 0 && heading3At >= 0 && bodyAt >= 0 && codeAt >= 0 && regexAt >= 0 && exampleAt >= 0 && noteAt >= 0 && warningAt >= 0 && tableAt >= 0 && tableDescriptionAt >= 0 && tableDataAt >= 0 && inlineAt >= 0 && afterTableAt >= 0 && linkAt >= 0 && firstBullet >= 0 && secondBullet > firstBullet;
        const bool textContract = rendered.Find(L"| --- | --- |") < 0 && rendered.Find(L"Syntax\tMeaning") >= 0 &&
            rendered.Find(L"\\d\tdigit") >= 0 && rendered.Find(L"\\w\tword") >= 0 && rendered.Find(L"`\\d`") < 0;
        const bool formats = positions && formatAt(titleAt, titleFormat) && formatAt(heading2At, heading2Format) && formatAt(heading3At, heading3Format) && formatAt(bodyAt, bodyFormat) &&
            formatAt(codeAt, codeFormat) && formatAt(regexAt, regexFormat) && formatAt(exampleAt, exampleFormat) && formatAt(noteAt, noteFormat) && formatAt(warningAt, warningFormat) && formatAt(tableAt, tableFormat) && formatAt(tableDescriptionAt, tableDescriptionFormat) && formatAt(tableDataAt, tableDataFormat) && formatAt(inlineAt, inlineFormat) && formatAt(afterTableAt, afterTableFormat) && formatAt(linkAt, linkFormat) && paragraphAt(heading2At, heading2Paragraph) && paragraphAt(heading3At, heading3Paragraph) && paragraphAt(exampleAt, exampleParagraph) && paragraphAt(noteAt, noteParagraph) && paragraphAt(warningAt, warningParagraph) && paragraphAt(tableAt, tableParagraph) && paragraphAt(firstBullet, listParagraph);
        styleDetail = ((titleFormat.dwEffects & CFE_BOLD) != 0 ? 1 : 0) | ((heading2Format.dwEffects & CFE_BOLD) != 0 ? 2 : 0) |
            ((heading3Format.dwEffects & CFE_BOLD) != 0 ? 4 : 0) | ((bodyFormat.dwEffects & CFE_BOLD) == 0 ? 8 : 0) |
            ((codeFormat.dwEffects & CFE_BOLD) == 0 ? 16 : 0) | ((inlineFormat.dwEffects & CFE_BOLD) == 0 ? 32 : 0) |
            ((afterTableFormat.dwEffects & CFE_BOLD) == 0 ? 64 : 0);
        const bool codeFace = ::lstrcmpiW(codeFormat.szFaceName, L"Consolas") == 0 &&
            ::lstrcmpiW(regexFormat.szFaceName, L"Consolas") == 0;
        const bool inlineFace = ::lstrcmpiW(inlineFormat.szFaceName, L"Consolas") == 0;
        // Use a stand-alone table to query native RichEdit character offsets:
        // GetWindowText expands CR to CR/LF, while EM_SETSEL positions count a
        // paragraph delimiter once. This keeps the term/description assertion
        // about the actual rendered characters rather than converted offsets.
        std::vector<MarkdownBlock> tableFontBlocks;
        ParseMarkdownText(L"| Syntax | Meaning |\n| --- | --- |\n| \\d | digit |", tableFontBlocks);
        RenderMarkdown(richEdit, tableFontBlocks);
        CHARFORMAT2 standaloneTerm = {}, standaloneDescription = {}, standaloneData = {};
        const bool tableTermFace = formatAt(findNative(L"Syntax"), standaloneTerm) && ::lstrcmpiW(standaloneTerm.szFaceName, L"Consolas") == 0;
        const bool tableDescriptionFace = formatAt(findNative(L"Meaning"), standaloneDescription) && ::lstrcmpiW(standaloneDescription.szFaceName, standaloneTerm.szFaceName) != 0;
        const bool tableHeaderBold = (standaloneTerm.dwEffects & CFE_BOLD) != 0 && (standaloneDescription.dwEffects & CFE_BOLD) != 0;
        const bool tableDataNonBold = formatAt(findNative(L"\\d"), standaloneData) && (standaloneData.dwEffects & CFE_BOLD) == 0;
        std::vector<MarkdownBlock> threeColumnBlocks;
        ParseMarkdownText(L"| Term | Meaning | Example |\n| --- | --- | --- |\n| \\w | word character | letters and digits |", threeColumnBlocks);
        RenderMarkdown(richEdit, threeColumnBlocks);
        CString threeColumnText; ReadRichEditText(richEdit, threeColumnText);
        CHARFORMAT2 thirdColumnFormat = {};
        const bool threeColumns = threeColumnBlocks.size() == 1 && threeColumnBlocks[0].table.headers.size() == 3 &&
            threeColumnText.Find(L"Term\tMeaning\tExample") >= 0 &&
            threeColumnText.Find(L"\\w\tword character\tletters and digits") >= 0 &&
            formatAt(findNative(L"Example"), thirdColumnFormat) &&
            ::lstrcmpiW(thirdColumnFormat.szFaceName, standaloneDescription.szFaceName) == 0 &&
            (thirdColumnFormat.dwEffects & CFE_BOLD) != 0;
        styleDetail |= tableHeaderBold ? 128 : 0;
        styleDetail |= tableDataNonBold ? 256 : 0;
        const bool styles = formats && styleDetail == 511;
        facesDetail = (codeFace ? 1 : 0) | (tableTermFace ? 2 : 0) | (inlineFace ? 4 : 0) | (tableDescriptionFace ? 8 : 0);
        const bool faces = formats && codeFace && tableTermFace && inlineFace && tableDescriptionFace;
        const bool sizes = formats && titleFormat.yHeight > heading2Format.yHeight && heading2Format.yHeight > bodyFormat.yHeight && heading3Format.yHeight >= bodyFormat.yHeight &&
            (heading2Format.yHeight != heading3Format.yHeight || heading2Paragraph.dySpaceBefore != heading3Paragraph.dySpaceBefore) && bodyFormat.yHeight >= 200 && inlineFormat.yHeight == bodyFormat.yHeight;
        const bool tables = formats && tableParagraph.cTabCount == 0 && textContract && threeColumns;
        const bool specialBlocks = formats && ::lstrcmpiW(exampleFormat.szFaceName, L"Consolas") == 0 &&
            (exampleFormat.dwEffects & CFE_AUTOBACKCOLOR) != 0 && exampleParagraph.dxStartIndent > 0 &&
            noteParagraph.dxStartIndent > exampleParagraph.dxStartIndent && warningParagraph.dxStartIndent == noteParagraph.dxStartIndent &&
            (noteFormat.dwEffects & CFE_BOLD) == 0 && (warningFormat.dwEffects & CFE_BOLD) == 0 &&
            rendered.Find(L"\x2139 Information is visible.") >= 0 && rendered.Find(L"\x26A0 Regex is text based.") >= 0;
        const bool automaticBackgrounds = formats && (codeFormat.dwEffects & CFE_AUTOBACKCOLOR) != 0 && (inlineFormat.dwEffects & CFE_AUTOBACKCOLOR) != 0 && (tableFormat.dwEffects & CFE_AUTOBACKCOLOR) != 0;
        std::vector<MarkdownBlock> backgroundSequence;
        AddBlock(backgroundSequence, MarkdownBlockKind::Code, L"code");
        AddBlock(backgroundSequence, MarkdownBlockKind::Body, L"body-after-code");
        AddBlock(backgroundSequence, MarkdownBlockKind::Table, L"table");
        AddBlock(backgroundSequence, MarkdownBlockKind::Body, L"body-after-table");
        AddBlock(backgroundSequence, MarkdownBlockKind::Note, L"note");
        AddBlock(backgroundSequence, MarkdownBlockKind::Body, L"body-after-note");
        RenderMarkdown(richEdit, backgroundSequence);
        CString backgroundText; ReadRichEditText(richEdit, backgroundText);
        CHARFORMAT2 bodyAfterCode = {}, bodyAfterTable = {}, bodyAfterNote = {};
        const auto hasAutomaticBodyBackground = [](const CHARFORMAT2& format) -> bool {
            return (format.dwEffects & CFE_AUTOBACKCOLOR) != 0;
        };
        const bool backgroundReset = formatAt(backgroundText.Find(L"body-after-code"), bodyAfterCode) &&
            formatAt(backgroundText.Find(L"body-after-table"), bodyAfterTable) &&
            formatAt(backgroundText.Find(L"body-after-note"), bodyAfterNote) &&
            hasAutomaticBodyBackground(bodyAfterCode) && hasAutomaticBodyBackground(bodyAfterTable) && hasAutomaticBodyBackground(bodyAfterNote);
        std::vector<MarkdownBlock> shadedLinkBlocks;
        AddBlock(shadedLinkBlocks, MarkdownBlockKind::Code, L"code https://code.invalid");
        std::vector<MarkdownBlock> linkedTable;
        ParseMarkdownText(L"| Name | URL |\n| --- | --- |\n| row | table https://table.invalid |", linkedTable);
        shadedLinkBlocks.insert(shadedLinkBlocks.end(), linkedTable.begin(), linkedTable.end());
        AddBlock(shadedLinkBlocks, MarkdownBlockKind::Note, L"note https://note.invalid");
        RenderMarkdown(richEdit, shadedLinkBlocks);
        const auto preservesParentLinkFormat = [&](LPCWSTR prefix, LPCWSTR url) -> bool {
            CHARFORMAT2 parent = {}, hyperlink = {};
            const int parentAt = findNative(prefix), linkAt = findNative(url);
            if (parentAt < 0 || linkAt < 0 || !formatAt(parentAt, parent) || !formatAt(linkAt, hyperlink)) return false;
            return (hyperlink.dwEffects & CFE_UNDERLINE) != 0 && hyperlink.crTextColor == ThemeManager::AccentColor() &&
                (parent.dwEffects & CFE_AUTOBACKCOLOR) != 0 && (hyperlink.dwEffects & CFE_AUTOBACKCOLOR) != 0 &&
                parent.yHeight == hyperlink.yHeight && ::lstrcmpiW(parent.szFaceName, hyperlink.szFaceName) == 0 &&
                (parent.dwEffects & CFE_BOLD) == (hyperlink.dwEffects & CFE_BOLD);
        };
        const bool codeLinkPreserved = preservesParentLinkFormat(L"code", L"https://code.invalid");
        const bool tableLinkPreserved = preservesParentLinkFormat(L"table", L"https://table.invalid");
        const bool noteLinkPreserved = preservesParentLinkFormat(L"note", L"https://note.invalid");
        const bool shadedLinks = codeLinkPreserved && tableLinkPreserved && noteLinkPreserved;
        shadedLinkDetail = (codeLinkPreserved ? 1 : 0) | (tableLinkPreserved ? 2 : 0) | (noteLinkPreserved ? 4 : 0);
        const bool hangingIndent = formats && listParagraph.dxStartIndent > 0 && listParagraph.dxOffset < 0;
        const bool link = formats && (linkFormat.dwEffects & CFE_UNDERLINE) != 0 && linkFormat.crTextColor == ThemeManager::AccentColor();
        formattingDetail = (positions ? 1 : 0) | (textContract ? 2 : 0) | (formats ? 4 : 0) | (styles ? 8 : 0) | (faces ? 16 : 0) | (sizes ? 32 : 0) | (tables ? 64 : 0) | (automaticBackgrounds ? 128 : 0) | (hangingIndent ? 256 : 0) | (link ? 512 : 0) | (backgroundReset ? 1024 : 0) | (shadedLinks ? 2048 : 0) | (specialBlocks ? 4096 : 0);
        formatting = positions && textContract && formats && styles && faces && sizes && tables && specialBlocks && automaticBackgrounds && backgroundReset && shadedLinks && hangingIndent && link;
        ::DestroyWindow(richEdit);
    }
    if (richEditLibrary) ::FreeLibrary(richEditLibrary);
    const bool passed = designLoaded && sourceLoaded && ruDesignLoaded && ruSourceLoaded && fallbackLoaded && content && productionBlocks && cached && parser && formatting && longDocuments;
    report.Format("design=%d\nsource=%d\nru_design=%d\nru_source=%d\nfallback=%d\ncontent=%d\nproduction_blocks=%d\ncache=%d\nparser=%d\nformat=%d\nformat_detail=%d\nstyle_detail=%d\nfaces_detail=%d\nshaded_link_detail=%d\nlong=%d\nen_design_length=%d\nen_design_expected_length=%d\nen_design_terminal_newline_omitted=%d\nen_source_length=%d\nen_source_expected_length=%d\nen_source_terminal_newline_omitted=%d\nru_design_length=%d\nru_design_expected_length=%d\nru_design_terminal_newline_omitted=%d\nru_source_length=%d\nru_source_expected_length=%d\nru_source_terminal_newline_omitted=%d\nresult=%s\n",
        designLoaded, sourceLoaded, ruDesignLoaded, ruSourceLoaded, fallbackLoaded, content, productionBlocks, cached, parser, formatting, formattingDetail, styleDetail, facesDetail, shadedLinkDetail, longDocuments,
        enDesignLength, enDesignExpectedLength, enDesignTerminalNewlineOmitted, enSourceLength, enSourceExpectedLength, enSourceTerminalNewlineOmitted,
        ruDesignLength, ruDesignExpectedLength, ruDesignTerminalNewlineOmitted, ruSourceLength, ruSourceExpectedLength, ruSourceTerminalNewlineOmitted, passed ? "pass" : "fail");
    return passed;
}

bool RunMissingFilesRuntimeSmoke(HWND owner, CStringA& report)
{
    report.Empty();
    std::vector<MarkdownBlock> fallback;
    CString sourcePath;
    const bool loaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"zz-ZZ", fallback, sourcePath);
    const bool content = !loaded && sourcePath.IsEmpty() && fallback.size() == 2 &&
        fallback[0].kind == MarkdownBlockKind::Title && fallback[1].kind == MarkdownBlockKind::Note &&
        fallback[1].text == L"Help file was not found.";
    HMODULE richEditLibrary = ::LoadLibraryW(L"Msftedit.dll");
    HWND richEdit = richEditLibrary ? ::CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_POPUP | ES_MULTILINE, 0, 0, 16, 16, owner, NULL, NULL, NULL) : NULL;
    bool rendered = false;
    if (richEdit)
    {
        RenderMarkdown(richEdit, fallback);
        CString text;
        rendered = ReadRichEditText(richEdit, text) && text.Find(L"Help file was not found.") >= 0;
        ::DestroyWindow(richEdit);
    }
    if (richEditLibrary) ::FreeLibrary(richEditLibrary);
    const bool passed = content && rendered;
    report.Format("missing_files=%d\nfallback_content=%d\nfallback_render=%d\nresult=%s\n", !loaded, content, rendered, passed ? "pass" : "fail");
    return passed;
}
}
