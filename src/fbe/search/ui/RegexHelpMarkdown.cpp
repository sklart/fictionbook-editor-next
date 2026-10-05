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
    if (kind != MarkdownBlockKind::Code) text.Trim();
    if (text.IsEmpty() && kind != MarkdownBlockKind::Code) return;
    MarkdownBlock block = {}; block.kind = kind; block.headingLevel = headingLevel; block.text = text;
    if (kind != MarkdownBlockKind::Code) ParseInlineCode(block.text, block.inlineCode);
    blocks.push_back(block);
}

void ParseMarkdown(const CString& source, std::vector<MarkdownBlock>& blocks)
{
    blocks.clear();
    bool codeFence = false;
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
        if (line.Left(3) == L"```")
        {
            flushParagraph();
            if (codeFence) { AddBlock(blocks, MarkdownBlockKind::Code, code); code.Empty(); codeHasLine = false; }
            codeFence = !codeFence;
            continue;
        }
        if (codeFence) { if (codeHasLine) code += L"\n"; code += line; codeHasLine = true; continue; }
        CString trimmed(line); trimmed.Trim();
        if (trimmed.IsEmpty()) { flushParagraph(); continue; }
        int hashes = 0; while (hashes < trimmed.GetLength() && trimmed[hashes] == L'#') ++hashes;
        if (hashes > 0 && hashes <= 3 && hashes < trimmed.GetLength() && trimmed[hashes] == L' ')
        {
            flushParagraph(); AddBlock(blocks, hashes == 1 ? MarkdownBlockKind::Title : MarkdownBlockKind::Heading, trimmed.Mid(hashes + 1), hashes); continue;
        }
        if (trimmed.Left(2) == L"- ") { flushParagraph(); AddBlock(blocks, MarkdownBlockKind::List, trimmed.Mid(2)); continue; }
        if (trimmed.Find(L'|') >= 0)
        {
            flushParagraph();
            if (IsMarkdownTableSeparator(trimmed)) continue;
            CString table = TrimMarkdownTableCell(trimmed); table.Replace(L"|", L"\t"); AddBlock(blocks, MarkdownBlockKind::Table, table); continue;
        }
        if (!paragraph.IsEmpty()) paragraph += L" ";
        paragraph += trimmed;
    }
    if (codeFence) AddBlock(blocks, MarkdownBlockKind::Note, L"Malformed Markdown code block.");
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

COLORREF HelpBlockBackground(MarkdownBlockKind kind)
{
    if (ThemeManager::IsHighContrast()) return ::GetSysColor(COLOR_WINDOW);
    if (kind == MarkdownBlockKind::Code) return ThemeManager::IsDark() ? ThemeManager::PressedColor() : ThemeManager::ControlColor();
    if (kind == MarkdownBlockKind::Table) return ThemeManager::IsDark() ? ThemeManager::PressedColor() : ThemeManager::ControlColor();
    if (kind == MarkdownBlockKind::Note) return ThemeManager::IsDark() ? ThemeManager::ControlColor() : ThemeManager::HoverColor();
    return ThemeManager::WindowColor();
}

COLORREF HelpTableHeaderBackground()
{
    if (ThemeManager::IsHighContrast()) return ::GetSysColor(COLOR_WINDOW);
    return ThemeManager::HoverColor();
}

CHARFORMAT2 MakeCharacterFormat(HWND richEdit, bool bold, bool monospace, int pointSize, bool shaded = false, COLORREF background = 0)
{
    CHARFORMAT2 format = {}; format.cbSize = sizeof(format); format.dwMask = CFM_BOLD | CFM_FACE | CFM_COLOR | CFM_SIZE;
    format.dwEffects = bold ? CFE_BOLD : 0; format.crTextColor = ThemeManager::TextColor();
    format.dwMask |= CFM_BACKCOLOR;
    if (shaded) { format.dwEffects &= ~CFE_AUTOBACKCOLOR; format.crBackColor = background; }
    else { format.dwEffects |= CFE_AUTOBACKCOLOR; format.crBackColor = ThemeManager::WindowColor(); }
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

PARAFORMAT2 MakeParagraphFormat(const MarkdownBlock& block)
{
    PARAFORMAT2 paragraph = {}; paragraph.cbSize = sizeof(paragraph); paragraph.dwMask = PFM_SPACEAFTER;
    paragraph.dySpaceAfter = block.kind == MarkdownBlockKind::Title ? 220 : block.headingLevel == 2 ? 100 : block.headingLevel == 3 ? 60 : 40;
    if (block.headingLevel == 2) { paragraph.dwMask |= PFM_SPACEBEFORE; paragraph.dySpaceBefore = 180; }
    if (block.headingLevel == 3) { paragraph.dwMask |= PFM_SPACEBEFORE; paragraph.dySpaceBefore = 100; }
    if (block.kind == MarkdownBlockKind::List) { paragraph.dwMask |= PFM_STARTINDENT | PFM_OFFSET; paragraph.dxStartIndent = 240; paragraph.dxOffset = -120; }
    if (block.kind == MarkdownBlockKind::Code || block.kind == MarkdownBlockKind::Table)
    {
        paragraph.dwMask |= PFM_STARTINDENT | PFM_RIGHTINDENT | PFM_TABSTOPS | PFM_SPACEBEFORE;
        paragraph.dxStartIndent = 140;
        paragraph.dxRightIndent = 140;
        paragraph.dySpaceBefore = 60;
        paragraph.dySpaceAfter = 60;
        paragraph.cTabCount = 4;
        paragraph.rgxTabs[0] = 720; paragraph.rgxTabs[1] = 1440; paragraph.rgxTabs[2] = 2160; paragraph.rgxTabs[3] = 2880;
    }
    if (block.kind == MarkdownBlockKind::Note) { paragraph.dwMask |= PFM_STARTINDENT | PFM_SPACEBEFORE; paragraph.dxStartIndent = 140; paragraph.dySpaceBefore = 40; }
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
    }
    return expected;
}

bool ReadRichEditText(HWND richEdit, CString& text)
{
    const int length = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0));
    wchar_t* buffer = text.GetBuffer(length + 1);
    const int copied = ::GetWindowTextW(richEdit, buffer, length + 1);
    text.ReleaseBuffer(copied);
    return copied == length;
}
}

namespace FbeRegexHelp
{
bool LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext context, LPCWSTR requestedLocale, std::vector<MarkdownBlock>& blocks, CString& sourcePath)
{
    sourcePath.Empty(); blocks.clear();
    const CString fileName = HelpFileName(context);
    CString locale(requestedLocale ? requestedLocale : L"");
    CString markdown;
    const CString localized = HelpPathForLocale(locale, fileName);
    if (ReadUtf8File(localized, markdown) && !markdown.IsEmpty()) { sourcePath = localized; ParseMarkdown(markdown, blocks); return !blocks.empty(); }
    const CString fallback = HelpPathForLocale(L"en-US", fileName);
    if (ReadUtf8File(fallback, markdown) && !markdown.IsEmpty()) { sourcePath = fallback; ParseMarkdown(markdown, blocks); return !blocks.empty(); }
    AddBlock(blocks, MarkdownBlockKind::Title, context == FbeSearchPresets::SearchUiContext::Source ? L"Source regular expression help" : L"Regular expression help");
    AddBlock(blocks, MarkdownBlockKind::Note, L"Help file was not found.");
    return false;
}

bool LoadMarkdown(FbeSearchPresets::SearchUiContext context, std::vector<MarkdownBlock>& blocks, CString& sourcePath)
{
    return LoadMarkdownForLocale(context, FbeRuntimeLocalization::GetPreferredRuntimeLocaleName(), blocks, sourcePath);
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
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        const MarkdownBlock& block = blocks[index];
        const int first = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0));
        CString value(RenderedBlockText(block)); value += L"\r\n";
        ::SendMessage(richEdit, EM_SETSEL, first, first);
        ::SendMessage(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(value)));
        const int last = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0)) - 2;
        const bool tableHeader = block.kind == MarkdownBlockKind::Table && (index == 0 || blocks[index - 1].kind != MarkdownBlockKind::Table);
        const bool bold = block.kind == MarkdownBlockKind::Title || block.kind == MarkdownBlockKind::Heading || tableHeader;
        const bool monospace = block.kind == MarkdownBlockKind::Code || block.kind == MarkdownBlockKind::Table;
        const bool shaded = block.kind == MarkdownBlockKind::Code || block.kind == MarkdownBlockKind::Table || block.kind == MarkdownBlockKind::Note;
        const COLORREF background = tableHeader ? HelpTableHeaderBackground() : HelpBlockBackground(block.kind);
        const CHARFORMAT2 format = MakeCharacterFormat(richEdit, bold, monospace, PointSizeForBlock(block), shaded, background);
        const PARAFORMAT2 paragraph = MakeParagraphFormat(block);
        SelectAndFormat(richEdit, first, (std::max)(first, last), format, paragraph);
        for (size_t span = 0; span < block.inlineCode.size(); ++span)
        {
            CHARFORMAT2 code = MakeCharacterFormat(richEdit, false, true, 10, true, HelpBlockBackground(MarkdownBlockKind::Code));
            ::SendMessage(richEdit, EM_SETSEL, first + block.inlineCode[span].start, first + block.inlineCode[span].start + block.inlineCode[span].length);
            ::SendMessage(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&code));
        }
        for (int link = value.Find(L"http"); link >= 0; )
        {
            int end = link; while (end < value.GetLength() && value[end] != L' ' && value[end] != L')' && value[end] != L'\r' && value[end] != L'\n') ++end;
            CHARFORMAT2 hyperlink = MakeCharacterFormat(richEdit, false, false, 10);
            hyperlink.dwMask |= CFM_UNDERLINE | CFM_COLOR; hyperlink.dwEffects |= CFE_UNDERLINE; hyperlink.crTextColor = ThemeManager::AccentColor();
            ::SendMessage(richEdit, EM_SETSEL, first + link, first + end);
            ::SendMessage(richEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&hyperlink));
            link = value.Find(L"http", end);
        }
    }
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
    std::vector<MarkdownBlock> parsed, empty, malformed, unknown;
    ParseMarkdownText(L"# Title\n## Heading two\n### Heading three\nBody `inline` text\n- one\n- two\n```text\n   leading\ntrailing   \n\ttab\n\nlast\n```\n| Syntax | Meaning |\n| --- | --- |\n| \\d | digit |\n| \\w | word |\nBody after table https://example.invalid", parsed);
    ParseMarkdownText(L"", empty);
    ParseMarkdownText(L"```regex\n[", malformed);
    ParseMarkdownText(L"> unknown extension", unknown);
    bool hasTitle = false, hasHeading2 = false, hasHeading3 = false, hasBody = false, hasList = false, hasCode = false, hasTable = false, hasInlineCode = false, codeWhitespace = false;
    for (size_t index = 0; index < parsed.size(); ++index)
    {
        const MarkdownBlock& block = parsed[index];
        hasTitle = hasTitle || block.kind == MarkdownBlockKind::Title;
        hasHeading2 = hasHeading2 || (block.kind == MarkdownBlockKind::Heading && block.headingLevel == 2);
        hasHeading3 = hasHeading3 || (block.kind == MarkdownBlockKind::Heading && block.headingLevel == 3);
        hasBody = hasBody || block.kind == MarkdownBlockKind::Body;
        hasList = hasList || block.kind == MarkdownBlockKind::List;
        hasCode = hasCode || block.kind == MarkdownBlockKind::Code;
        codeWhitespace = codeWhitespace || (block.kind == MarkdownBlockKind::Code && block.text == L"   leading\ntrailing   \n\ttab\n\nlast");
        hasTable = hasTable || block.kind == MarkdownBlockKind::Table;
        hasInlineCode = hasInlineCode || !block.inlineCode.empty();
    }
    const bool parser = hasTitle && hasHeading2 && hasHeading3 && hasBody && hasList && hasCode && hasTable && hasInlineCode && codeWhitespace &&
        empty.empty() && malformed.size() == 1 && malformed[0].text == L"Malformed Markdown code block." && !unknown.empty();
    HMODULE richEditLibrary = ::LoadLibraryW(L"Msftedit.dll");
    HWND richEdit = richEditLibrary ? ::CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_POPUP | ES_MULTILINE, 0, 0, 16, 16, owner, NULL, NULL, NULL) : NULL;
    bool formatting = false, longDocuments = false;
    int formattingDetail = 0;
    int enDesignLength = 0, enSourceLength = 0, ruDesignLength = 0, ruSourceLength = 0;
    int enDesignExpectedLength = 0, enSourceExpectedLength = 0, ruDesignExpectedLength = 0, ruSourceExpectedLength = 0;
    if (richEdit)
    {
        const auto renderFullDocument = [richEdit](const std::vector<MarkdownBlock>& blocks, int& length, int& expectedLength) -> bool {
            RenderMarkdown(richEdit, blocks);
            CString rendered;
            if (!ReadRichEditText(richEdit, rendered)) return false;
            const CString expected = ExpectedRenderedText(blocks);
            length = rendered.GetLength();
            expectedLength = expected.GetLength();
            const CString& lastMeaningful = blocks.back().text;
            return length > 32767 && length == expectedLength && rendered == expected && rendered.Find(lastMeaningful) >= 0;
        };
        const bool enDesignComplete = renderFullDocument(enDesign, enDesignLength, enDesignExpectedLength);
        const bool enSourceComplete = renderFullDocument(enSource, enSourceLength, enSourceExpectedLength);
        const bool ruDesignComplete = renderFullDocument(ruDesign, ruDesignLength, ruDesignExpectedLength);
        const bool ruSourceComplete = renderFullDocument(ruSource, ruSourceLength, ruSourceExpectedLength);
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
        CString rendered; ReadRichEditText(richEdit, rendered);
        CHARFORMAT2 titleFormat = {}, heading2Format = {}, heading3Format = {}, bodyFormat = {}, codeFormat = {}, tableFormat = {}, tableDataFormat = {}, inlineFormat = {}, afterTableFormat = {}, linkFormat = {};
        PARAFORMAT2 heading2Paragraph = {}, heading3Paragraph = {}, tableParagraph = {}, listParagraph = {};
        const int titleAt = rendered.Find(L"Title"), heading2At = rendered.Find(L"Heading two"), heading3At = rendered.Find(L"Heading three"), bodyAt = rendered.Find(L"Body"),
            codeAt = rendered.Find(L"   leading"), tableAt = rendered.Find(L"Syntax"), tableDataAt = rendered.Find(L"\\d"), inlineAt = rendered.Find(L"inline"), afterTableAt = rendered.Find(L"Body after table"), linkAt = rendered.Find(L"https://example.invalid"), firstBullet = rendered.Find(L"\x2022 one"), secondBullet = rendered.Find(L"\x2022 two");
        const bool positions = titleAt >= 0 && heading2At >= 0 && heading3At >= 0 && bodyAt >= 0 && codeAt >= 0 && tableAt >= 0 && tableDataAt >= 0 && inlineAt >= 0 && afterTableAt >= 0 && linkAt >= 0 && firstBullet >= 0 && secondBullet > firstBullet;
        const bool textContract = rendered.Find(L"| --- | --- |") < 0 && rendered.Find(L"Syntax") >= 0 && rendered.Find(L"Meaning") >= 0 && rendered.Find(L"\t") >= 0;
        const bool formats = positions && formatAt(titleAt, titleFormat) && formatAt(heading2At, heading2Format) && formatAt(heading3At, heading3Format) && formatAt(bodyAt, bodyFormat) &&
            formatAt(codeAt, codeFormat) && formatAt(tableAt, tableFormat) && formatAt(tableDataAt, tableDataFormat) && formatAt(inlineAt, inlineFormat) && formatAt(afterTableAt, afterTableFormat) && formatAt(linkAt, linkFormat) && paragraphAt(heading2At, heading2Paragraph) && paragraphAt(heading3At, heading3Paragraph) && paragraphAt(tableAt, tableParagraph) && paragraphAt(firstBullet, listParagraph);
        const bool styles = formats && (titleFormat.dwEffects & CFE_BOLD) != 0 && (heading2Format.dwEffects & CFE_BOLD) != 0 && (heading3Format.dwEffects & CFE_BOLD) != 0 &&
            (bodyFormat.dwEffects & CFE_BOLD) == 0 && (codeFormat.dwEffects & CFE_BOLD) == 0 && (inlineFormat.dwEffects & CFE_BOLD) == 0 && (afterTableFormat.dwEffects & CFE_BOLD) == 0 &&
            (tableFormat.dwEffects & CFE_BOLD) != 0 && (tableDataFormat.dwEffects & CFE_BOLD) == 0;
        const bool faces = formats && ::lstrcmpiW(codeFormat.szFaceName, L"Consolas") == 0 && ::lstrcmpiW(tableFormat.szFaceName, L"Consolas") == 0 && ::lstrcmpiW(inlineFormat.szFaceName, L"Consolas") == 0;
        const bool sizes = formats && titleFormat.yHeight > heading2Format.yHeight && heading2Format.yHeight > bodyFormat.yHeight && heading3Format.yHeight >= bodyFormat.yHeight &&
            (heading2Format.yHeight != heading3Format.yHeight || heading2Paragraph.dySpaceBefore != heading3Paragraph.dySpaceBefore) && bodyFormat.yHeight >= 200 && inlineFormat.yHeight == bodyFormat.yHeight;
        const bool tabs = formats && tableParagraph.cTabCount >= 2;
        const bool backgrounds = formats && (codeFormat.dwMask & CFM_BACKCOLOR) != 0 && (inlineFormat.dwMask & CFM_BACKCOLOR) != 0 && (tableFormat.dwMask & CFM_BACKCOLOR) != 0;
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
            return (format.dwMask & CFM_BACKCOLOR) != 0 && (format.dwEffects & CFE_AUTOBACKCOLOR) != 0;
        };
        const bool backgroundReset = formatAt(backgroundText.Find(L"body-after-code"), bodyAfterCode) &&
            formatAt(backgroundText.Find(L"body-after-table"), bodyAfterTable) &&
            formatAt(backgroundText.Find(L"body-after-note"), bodyAfterNote) &&
            hasAutomaticBodyBackground(bodyAfterCode) && hasAutomaticBodyBackground(bodyAfterTable) && hasAutomaticBodyBackground(bodyAfterNote);
        const bool hangingIndent = formats && listParagraph.dxStartIndent > 0 && listParagraph.dxOffset < 0;
        const bool link = formats && (linkFormat.dwEffects & CFE_UNDERLINE) != 0 && linkFormat.crTextColor == ThemeManager::AccentColor();
        formattingDetail = (positions ? 1 : 0) | (textContract ? 2 : 0) | (formats ? 4 : 0) | (styles ? 8 : 0) | (faces ? 16 : 0) | (sizes ? 32 : 0) | (tabs ? 64 : 0) | (backgrounds ? 128 : 0) | (hangingIndent ? 256 : 0) | (link ? 512 : 0) | (backgroundReset ? 1024 : 0);
        formatting = positions && textContract && formats && styles && faces && sizes && tabs && backgrounds && backgroundReset && hangingIndent && link;
        ::DestroyWindow(richEdit);
    }
    if (richEditLibrary) ::FreeLibrary(richEditLibrary);
    const bool passed = designLoaded && sourceLoaded && ruDesignLoaded && ruSourceLoaded && fallbackLoaded && content && parser && formatting && longDocuments;
    report.Format("design=%d\nsource=%d\nru_design=%d\nru_source=%d\nfallback=%d\ncontent=%d\nparser=%d\nformat=%d\nformat_detail=%d\nlong=%d\nen_design_length=%d\nen_design_expected_length=%d\nen_source_length=%d\nen_source_expected_length=%d\nru_design_length=%d\nru_design_expected_length=%d\nru_source_length=%d\nru_source_expected_length=%d\nresult=%s\n",
        designLoaded, sourceLoaded, ruDesignLoaded, ruSourceLoaded, fallbackLoaded, content, parser, formatting, formattingDetail, longDocuments,
        enDesignLength, enDesignExpectedLength, enSourceLength, enSourceExpectedLength, ruDesignLength, ruDesignExpectedLength, ruSourceLength, ruSourceExpectedLength, passed ? "pass" : "fail");
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
