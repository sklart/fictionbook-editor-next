#pragma once

#include <mshtml.h>
#include <atlstr.h>
#include <vector>

namespace FbeTable
{
struct LogicalCell
{
	MSHTML::IHTMLElementPtr element;
	long sourceRow;
	long startColumn;
	long colspan;
	long rowspan;
};

struct Grid
{
	std::vector<MSHTML::IHTMLElementPtr> rows;
	std::vector<LogicalCell> cells;
	std::vector<std::vector<long> > slots;
	long columns;

	Grid() : columns(0) {}
	long At(long row, long column) const;
	void Ensure(long row, long column);
};

MSHTML::IHTMLElementPtr FindTableElement(MSHTML::IHTMLElementPtr element);
MSHTML::IHTMLElementPtr FindTableRow(MSHTML::IHTMLElementPtr element);
MSHTML::IHTMLElementPtr FindTableCell(MSHTML::IHTMLElementPtr element);
bool IsTableCell(const MSHTML::IHTMLElementPtr& element);
void GetDirectCells(const MSHTML::IHTMLElementPtr& row, std::vector<MSHTML::IHTMLElementPtr>& cells);
void GetCells(const MSHTML::IHTMLElementPtr& table, std::vector<MSHTML::IHTMLElementPtr>& cells);
long GetSpan(const MSHTML::IHTMLElementPtr& cell, const wchar_t* fbName, const wchar_t* htmlName);
void SetSpan(const MSHTML::IHTMLElementPtr& cell, const wchar_t* fbName, const wchar_t* htmlName, long span);
bool BuildGrid(const MSHTML::IHTMLElementPtr& table, Grid& grid);
long FindCell(const Grid& grid, const MSHTML::IHTMLElementPtr& element);
bool GetCellRectangle(const MSHTML::IHTMLElementPtr& firstCell, const MSHTML::IHTMLElementPtr& lastCell, std::vector<MSHTML::IHTMLElementPtr>& result);
bool GetSelectedCells(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& currentCell, std::vector<MSHTML::IHTMLElementPtr>& result);
CStringA BuildStructuralSnapshot(const Grid& grid);

namespace GridDiagnostics
{
	void ResetBuildCount();
	long BuildCount();
}
}
