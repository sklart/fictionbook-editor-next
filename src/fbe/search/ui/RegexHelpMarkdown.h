#pragma once

#include "..\\SearchPreset.h"
#include <vector>

namespace FbeRegexHelp
{
enum class MarkdownBlockKind { Title, Heading, Body, List, Code, Table, Note };

struct MarkdownInlineCode
{
    int start;
    int length;
};

struct MarkdownBlock
{
    MarkdownBlockKind kind;
    int headingLevel;
    CString text;
    std::vector<MarkdownInlineCode> inlineCode;
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
