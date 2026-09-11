#pragma once

#include "TableGrid.h"

namespace FbeTable
{
MSHTML::IHTMLElementPtr CreateCell(MSHTML::IHTMLDocument2Ptr document, const wchar_t* tagName);
MSHTML::IHTMLElementPtr CreateRowLike(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& sourceRow);

bool InsertRow(MSHTML::IHTMLDocument2Ptr document, const Grid& grid, long rowIndex, bool below, const wchar_t* fallbackTag);
bool DeleteRow(const Grid& grid, long rowIndex);
bool InsertColumn(MSHTML::IHTMLDocument2Ptr document, const Grid& grid, long selectedCell, bool before, const wchar_t* fallbackTag);
bool DeleteColumn(const Grid& grid, long column);
bool ToggleHeaderCell(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& cell);
bool ReplaceCells(MSHTML::IHTMLDocument2Ptr document, const std::vector<MSHTML::IHTMLElementPtr>& cells, const wchar_t* targetName);
}
