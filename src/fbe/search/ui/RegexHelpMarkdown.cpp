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

void AddBlock(std::vector<MarkdownBlock>& blocks, MarkdownBlockKind kind, const CString& value)
{
    CString text(value); text.Trim();
    if (text.IsEmpty()) return;
    MarkdownBlock block = {}; block.kind = kind; block.text = text;
    if (kind != MarkdownBlockKind::Code) ParseInlineCode(block.text, block.inlineCode);
    blocks.push_back(block);
}

void ParseMarkdown(const CString& source, std::vector<MarkdownBlock>& blocks)
{
    blocks.clear();
    bool codeFence = false;
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
            if (codeFence) { AddBlock(blocks, MarkdownBlockKind::Code, code); code.Empty(); }
            codeFence = !codeFence;
            continue;
        }
        if (codeFence) { if (!code.IsEmpty()) code += L"\n"; code += line; continue; }
        CString trimmed(line); trimmed.Trim();
        if (trimmed.IsEmpty()) { flushParagraph(); continue; }
        int hashes = 0; while (hashes < trimmed.GetLength() && trimmed[hashes] == L'#') ++hashes;
        if (hashes > 0 && hashes <= 3 && hashes < trimmed.GetLength() && trimmed[hashes] == L' ')
        {
            flushParagraph(); AddBlock(blocks, hashes == 1 ? MarkdownBlockKind::Title : MarkdownBlockKind::Heading, trimmed.Mid(hashes + 1)); continue;
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
    if (codeFence) AddBlock(blocks, MarkdownBlockKind::Note, L"Unclosed code fence in help file.");
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

CHARFORMAT2 MakeCharacterFormat(HWND richEdit, bool bold, bool monospace, bool title)
{
    CHARFORMAT2 format = {}; format.cbSize = sizeof(format); format.dwMask = CFM_BOLD | CFM_FACE | CFM_COLOR | CFM_SIZE;
    format.dwEffects = bold ? CFE_BOLD : 0; format.crTextColor = ThemeManager::TextColor();
    HDC dc = richEdit ? ::GetDC(richEdit) : NULL;
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ::ReleaseDC(richEdit, dc);
    format.yHeight = (std::max)(1, ::MulDiv(title ? 11 : 9, 1440, dpi));
    if (monospace) ::lstrcpynW(format.szFaceName, L"Consolas", LF_FACESIZE);
    else { HFONT font = UiMetrics::DialogFont(); LOGFONTW logFont = {}; if (font && ::GetObjectW(font, sizeof(logFont), &logFont)) ::lstrcpynW(format.szFaceName, logFont.lfFaceName, LF_FACESIZE); }
    return format;
}

PARAFORMAT2 MakeParagraphFormat(MarkdownBlockKind kind)
{
    PARAFORMAT2 paragraph = {}; paragraph.cbSize = sizeof(paragraph); paragraph.dwMask = PFM_SPACEAFTER;
    paragraph.dySpaceAfter = kind == MarkdownBlockKind::Title ? 140 : kind == MarkdownBlockKind::Heading ? 60 : 25;
    if (kind == MarkdownBlockKind::Heading) { paragraph.dwMask |= PFM_SPACEBEFORE; paragraph.dySpaceBefore = 120; }
    if (kind == MarkdownBlockKind::List) { paragraph.dwMask |= PFM_STARTINDENT; paragraph.dxStartIndent = 180; }
    if (kind == MarkdownBlockKind::Code || kind == MarkdownBlockKind::Table) { paragraph.dwMask |= PFM_STARTINDENT; paragraph.dxStartIndent = 140; }
    return paragraph;
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
    ::SetWindowTextW(richEdit, L"");
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        const MarkdownBlock& block = blocks[index];
        const int first = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0));
        CString value(block.text); value += L"\r\n";
        ::SendMessage(richEdit, EM_SETSEL, first, first);
        ::SendMessage(richEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(value)));
        const int last = static_cast<int>(::SendMessage(richEdit, WM_GETTEXTLENGTH, 0, 0)) - 2;
        const bool bold = block.kind == MarkdownBlockKind::Title || block.kind == MarkdownBlockKind::Heading;
        const bool monospace = block.kind == MarkdownBlockKind::Code;
        const CHARFORMAT2 format = MakeCharacterFormat(richEdit, bold, monospace, block.kind == MarkdownBlockKind::Title);
        const PARAFORMAT2 paragraph = MakeParagraphFormat(block.kind);
        SelectAndFormat(richEdit, first, (std::max)(first, last), format, paragraph);
        for (size_t span = 0; span < block.inlineCode.size(); ++span)
        {
            CHARFORMAT2 code = MakeCharacterFormat(richEdit, false, true, false);
            PARAFORMAT2 inlineParagraph = MakeParagraphFormat(block.kind);
            SelectAndFormat(richEdit, first + block.inlineCode[span].start, first + block.inlineCode[span].start + block.inlineCode[span].length, code, inlineParagraph);
        }
    }
    ::SendMessage(richEdit, EM_SETSEL, 0, 0);
    ::SendMessage(richEdit, EM_SCROLLCARET, 0, 0);
}
bool RunRuntimeSmoke(HWND owner, CStringA& report)
{
    report.Empty();
    std::vector<MarkdownBlock> design, source, fallback;
    CString designPath, sourcePath, fallbackPath;
    const bool designLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"en-US", design, designPath);
    const bool sourceLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Source, L"en-US", source, sourcePath);
    const bool fallbackLoaded = LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext::Design, L"zz-ZZ", fallback, fallbackPath) && fallbackPath.Find(L"Help\\en-US\\regex-design.md") >= 0;
    const bool content = !design.empty() && !source.empty() && design[0].kind == MarkdownBlockKind::Title && source[0].kind == MarkdownBlockKind::Title;
    std::vector<MarkdownBlock> parsed, empty, malformed, unknown;
    ParseMarkdownText(L"# Заголовок\n## Раздел\nОбычный `код`\n- пункт\n```regex\n\\bслово\\b\n```\n```xml\n<p/>\n```\n| Syntax | Meaning |\n| --- | --- |\n| x | y |", parsed);
    ParseMarkdownText(L"", empty);
    ParseMarkdownText(L"```regex\n[", malformed);
    ParseMarkdownText(L"> unknown extension", unknown);
    bool title = false, heading = false, body = false, list = false, code = false, table = false, inlineCode = false;
    for (size_t index = 0; index < parsed.size(); ++index)
    {
        const MarkdownBlock& block = parsed[index];
        title = title || block.kind == MarkdownBlockKind::Title;
        heading = heading || block.kind == MarkdownBlockKind::Heading;
        body = body || block.kind == MarkdownBlockKind::Body;
        list = list || block.kind == MarkdownBlockKind::List;
        code = code || block.kind == MarkdownBlockKind::Code;
        table = table || block.kind == MarkdownBlockKind::Table;
        inlineCode = inlineCode || !block.inlineCode.empty();
    }
    const bool parser = title && heading && body && list && code && table && inlineCode && empty.empty() && !malformed.empty() && !unknown.empty();
    HMODULE richEditLibrary = ::LoadLibraryW(L"Msftedit.dll");
    HWND richEdit = richEditLibrary ? ::CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_POPUP | ES_MULTILINE, 0, 0, 16, 16, owner, NULL, NULL, NULL) : NULL;
    bool formatting = false;
    if (richEdit)
    {
        std::vector<MarkdownBlock> blocks;
        AddBlock(blocks, MarkdownBlockKind::Title, L"Title");
        AddBlock(blocks, MarkdownBlockKind::Heading, L"Heading");
        AddBlock(blocks, MarkdownBlockKind::Body, L"Body");
        AddBlock(blocks, MarkdownBlockKind::Code, L"Code");
        RenderMarkdown(richEdit, blocks);
        auto formatAt = [richEdit](int position, CHARFORMAT2& format) -> bool {
            format = {}; format.cbSize = sizeof(format);
            ::SendMessage(richEdit, EM_SETSEL, position, position + 1);
            return ::SendMessage(richEdit, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format)) != 0;
        };
        const int textLength = ::GetWindowTextLengthW(richEdit);
        CString rendered; wchar_t* buffer = rendered.GetBuffer(textLength + 1); ::GetWindowTextW(richEdit, buffer, textLength + 1); rendered.ReleaseBuffer();
        CHARFORMAT2 title = {}, heading = {}, body = {}, code = {};
        const int titleAt = rendered.Find(L"Title"), headingAt = rendered.Find(L"Heading"), bodyAt = rendered.Find(L"Body"), codeAt = rendered.Find(L"Code");
        formatting = titleAt >= 0 && headingAt >= 0 && bodyAt >= 0 && codeAt >= 0 && formatAt(titleAt, title) && formatAt(headingAt, heading) && formatAt(bodyAt, body) && formatAt(codeAt, code) &&
            (title.dwEffects & CFE_BOLD) != 0 && (heading.dwEffects & CFE_BOLD) != 0 && (body.dwEffects & CFE_BOLD) == 0 &&
            (code.dwEffects & CFE_BOLD) == 0 && ::lstrcmpiW(code.szFaceName, L"Consolas") == 0 && title.yHeight > body.yHeight;
        ::DestroyWindow(richEdit);
    }
    if (richEditLibrary) ::FreeLibrary(richEditLibrary);
    const bool passed = designLoaded && sourceLoaded && fallbackLoaded && content && parser && formatting;
    report.Format("design=%d\nsource=%d\nfallback=%d\ncontent=%d\nparser=%d\nformat=%d\nresult=%s\n", designLoaded, sourceLoaded, fallbackLoaded, content, parser, formatting, passed ? "pass" : "fail");
    return passed;
}
}
