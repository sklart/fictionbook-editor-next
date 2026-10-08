#pragma once

#include "..\\SearchPreset.h"
#include <vector>

namespace FbeRegexHelp
{
enum class MarkdownBlockKind { Title, Heading, Body, List, Code, Regex, Table, Note, Example };

struct MarkdownInlineCode
{
    int start;
    int length;
};

// A table is semantic content, rather than a preformatted line with tab stops.
// The renderer owns the responsive presentation of its columns.
struct MarkdownTable
{
    std::vector<CString> headers;
    std::vector<std::vector<CString> > rows;
};

struct MarkdownBlock
{
    MarkdownBlockKind kind;
    int headingLevel;
    CString text;
    std::vector<MarkdownInlineCode> inlineCode;
    MarkdownTable table;
    bool warning = false;
};

struct MarkdownLoadMetrics
{
    ULONGLONG markdownReadMs = 0;
    ULONGLONG parseMs = 0;
    size_t blockCount = 0;
    bool cacheHit = false;
};

// Reads Help/<locale>/regex-*.md directly. It never writes help files.
bool LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext context, LPCWSTR locale, std::vector<MarkdownBlock>& blocks, CString& sourcePath, MarkdownLoadMetrics* metrics = NULL);
// Parses the supported deterministic Markdown subset without file I/O.
void ParseMarkdownText(const CString& text, std::vector<MarkdownBlock>& blocks);
// Uses the current runtime locale, falls back to en-US, and caches parsed
// Markdown by locale and Design/Source context for the process lifetime.
bool LoadMarkdown(FbeSearchPresets::SearchUiContext context, std::vector<MarkdownBlock>& blocks, CString& sourcePath, MarkdownLoadMetrics* metrics = NULL);
// Test-only callable smoke for parser, fallback and RichEdit character formatting.
bool RunRuntimeSmoke(HWND owner, CStringA& report);
bool RunMissingFilesRuntimeSmoke(HWND owner, CStringA& report);
void RenderMarkdown(HWND richEdit, const std::vector<MarkdownBlock>& blocks);
}
