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
    CString text;
    std::vector<MarkdownInlineCode> inlineCode;
};

// Reads Help/<locale>/regex-*.md on every call.  It never writes help files.
bool LoadMarkdown(FbeSearchPresets::SearchUiContext context, std::vector<MarkdownBlock>& blocks, CString& sourcePath);
void RenderMarkdown(HWND richEdit, const std::vector<MarkdownBlock>& blocks);
}
