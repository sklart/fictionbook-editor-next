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

// Reads Help/<locale>/regex-*.md on every call. It never writes help files.
bool LoadMarkdownForLocale(FbeSearchPresets::SearchUiContext context, LPCWSTR locale, std::vector<MarkdownBlock>& blocks, CString& sourcePath);
// Parses the supported deterministic Markdown subset without file I/O.
void ParseMarkdownText(const CString& text, std::vector<MarkdownBlock>& blocks);
// Uses the current runtime locale and falls back to en-US.
bool LoadMarkdown(FbeSearchPresets::SearchUiContext context, std::vector<MarkdownBlock>& blocks, CString& sourcePath);
// Test-only callable smoke for parser, fallback and RichEdit character formatting.
bool RunRuntimeSmoke(HWND owner, CStringA& report);
bool RunMissingFilesRuntimeSmoke(HWND owner, CStringA& report);
void RenderMarkdown(HWND richEdit, const std::vector<MarkdownBlock>& blocks);
}
