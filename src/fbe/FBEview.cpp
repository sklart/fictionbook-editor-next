// FBEView.cpp : implementation of the CFBEView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ImageImport.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBEView.h"
#include "SearchReplace.h"
#include "Scintilla.h"
#include "ElementDescMnr.h"
#include "StartupTrace.h"
#include "RuntimeLocalization.h"
#include <vector>

extern CElementDescMnr _EDMnr;

static bool IsSecondSetExternalFaultEnabled()
{
	wchar_t testMode[4] = {};
	const DWORD testModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode));
	if (!StartupTrace::Enabled() || testModeLength != 1 || testMode[0] != L'1') return false;
	wchar_t fault[64] = {};
	const DWORD faultLength = ::GetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", fault, _countof(fault));
	return faultLength == 19 && _wcsicmp(fault, L"second-set-external") == 0;
}

// normalization helpers
static void PackText(MSHTML::IHTMLElement2Ptr elem,MSHTML::IHTMLDocument2 *doc);
static void KillDivs(MSHTML::IHTMLElement2Ptr elem);
static void FixupParagraphs(MSHTML::IHTMLElement2Ptr elem);
static void RelocateParagraphs(MSHTML::IHTMLDOMNode *node);
static void KillStyles(MSHTML::IHTMLElement2Ptr elem);

// В живой сборке FBE regex режима «Дизайн» всегда идёт через наш wrapper
// поверх PCRE2, поэтому здесь больше не нужна развилка на VBScript.RegExp.
static AU::RegExp CreateSearchRegExp()
{
	return new AU::IRegExp2();
}

static AU::ReMatches ExecuteSearchRegExp(AU::RegExp re, const CString& text)
{
	return re->Execute(text);
}

static AU::ReMatches ExecuteSearchRegExp(AU::RegExp re, MSHTML::IHTMLTxtRangePtr range)
{
	_bstr_t rangeText(range->text);
	return ExecuteSearchRegExp(re, CString(static_cast<const wchar_t*>(rangeText)));
}

static bool IsParagraphElement(MSHTML::IHTMLElementPtr elem)
{
	return (bool)elem && U::scmp(elem->tagName, L"P") == 0;
}

static const wchar_t* HResultName(HRESULT result)
{
	if (result == S_OK) return L"S_OK";
	if (result == E_FAIL) return L"E_FAIL";
	if (result == E_NOINTERFACE) return L"E_NOINTERFACE";
	if (result == E_POINTER) return L"E_POINTER";
	if (result == E_INVALIDARG) return L"E_INVALIDARG";
	if (result == E_UNEXPECTED) return L"E_UNEXPECTED";
	if (result == E_ACCESSDENIED) return L"E_ACCESSDENIED";
	if (result == E_OUTOFMEMORY) return L"E_OUTOFMEMORY";
	return L"unknown";
}

static void TraceSelectionContainerFailure(const wchar_t* comOperation, HRESULT result,
	const wchar_t* selectionType, long controlLength = -1)
{
	CString details;
	details.Format(L"component=MSHTML; operation=SelectionContainer; com-operation=%s; HRESULT=0x%08lX; HRESULT_NAME=%s; selection.type=%s; view=BODY; documentTree=%s; treeImages=%s",
		comOperation,
		static_cast<unsigned long>(result),
		HResultName(result),
		selectionType,
		_Settings.ViewDocumentTree() ? L"enabled" : L"disabled",
		_Settings.GetDocTreeItemState(L"Image", true) ? L"enabled" : L"disabled");
	if (controlLength >= 0)
	{
		CString length;
		length.Format(L"; control.length=%ld", controlLength);
		details += length;
	}
	StartupTrace::HResult(L"mshtml", L"SC100", result, details);
}
// The visual view uses native HTML tables.  Keep table mutations here instead
// of relying on MSHTML's legacy editing commands: those commands may insert
// HTML that has no FB2 counterpart and do not group a whole operation in one
// undo item.
static bool IsTableCellElement(const MSHTML::IHTMLElementPtr& element)
{
	return (bool)element && (U::scmp(element->tagName, L"TD") == 0 || U::scmp(element->tagName, L"TH") == 0);
}

// MSHTML tracks attribute and child-list edits in an IMarkupServices undo
// unit, but not replacement of a table-cell element with a different tag.
// Keep that replacement as one native IOleUndoUnit so Ctrl+Z/Ctrl+Y retains
// the exact TD/TH conversion without serialising any editor-only state.
struct TableCellReplacement { MSHTML::IHTMLElementPtr active, inactive; };

class CTableCellToggleUndoUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit
{
public:
	BEGIN_COM_MAP(CTableCellToggleUndoUnit)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
	END_COM_MAP()

	void Initialize(const std::vector<TableCellReplacement>& replacements)
	{
		m_replacements = replacements;
	}

	STDMETHOD(Do)(IOleUndoManager* undoManager)
	{
		for (size_t index = 0; index < m_replacements.size(); ++index) {
			TableCellReplacement& replacement = m_replacements[index];
			if (!replacement.active || !replacement.inactive || !replacement.active->parentElement) return E_UNEXPECTED;
			MSHTML::IHTMLDOMNodePtr(replacement.active->parentElement)->replaceChild(MSHTML::IHTMLDOMNodePtr(replacement.inactive), MSHTML::IHTMLDOMNodePtr(replacement.active));
			MSHTML::IHTMLElementPtr previousActive(replacement.active);
			replacement.active = replacement.inactive;
			replacement.inactive = previousActive;
		}
		return undoManager ? undoManager->Add(this) : S_OK;
	}

	STDMETHOD(GetDescription)(BSTR* description)
	{
		if (!description) return E_POINTER;
		*description = ::SysAllocString(L"toggle table header cell");
		return *description ? S_OK : E_OUTOFMEMORY;
	}

	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id)
	{
		if (!classId || !id) return E_POINTER;
		*classId = CLSID_NULL; *id = 0;
		return S_OK;
	}

	STDMETHOD(OnNextAdd)() { return S_OK; }

private:
	std::vector<TableCellReplacement> m_replacements;
};

static HRESULT AddTableCellToggleUndoUnit(MSHTML::IHTMLDocument2Ptr document, const std::vector<TableCellReplacement>& replacements)
{
	IServiceProviderPtr serviceProvider(document);
	CComPtr<IOleUndoManager> undoManager;
	if (!serviceProvider || FAILED(serviceProvider->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, (void**)&undoManager))) return E_NOINTERFACE;
	CComObject<CTableCellToggleUndoUnit>* undoUnit = NULL;
	HRESULT hr = CComObject<CTableCellToggleUndoUnit>::CreateInstance(&undoUnit);
	if (FAILED(hr)) return hr;
	undoUnit->AddRef();
	undoUnit->Initialize(replacements);
	hr = undoManager->Add(undoUnit);
	undoUnit->Release();
	return hr;
}

// A native table is a structural child of an FB2 visual DIV, just like a
// paragraph or a nested DIV.  It must never be collected into an implicitly
// created paragraph by PackText().
static bool IsNativeTableBlockName(const _bstr_t& name)
{
	return U::scmp(name, L"TABLE") == 0;
}

static MSHTML::IHTMLElementPtr FindTableElement(MSHTML::IHTMLElementPtr element)
{
	while (element)
	{
		if (U::scmp(element->tagName, L"TABLE") == 0 && U::scmp(element->className, L"table") == 0)
			return element;
		element = element->parentElement;
	}
	return MSHTML::IHTMLElementPtr();
}

static MSHTML::IHTMLElementPtr FindTableRow(MSHTML::IHTMLElementPtr element)
{
	while (element)
	{
		if (U::scmp(element->tagName, L"TR") == 0 && U::scmp(element->className, L"tr") == 0)
			return element;
		element = element->parentElement;
	}
	return MSHTML::IHTMLElementPtr();
}

static MSHTML::IHTMLElementPtr FindTableCell(MSHTML::IHTMLElementPtr element)
{
	while (element)
	{
		if (IsTableCellElement(element)) return element;
		element = element->parentElement;
	}
	return MSHTML::IHTMLElementPtr();
}

static bool SelectTableCellRange(const MSHTML::IHTMLDocument2Ptr& document,
	const MSHTML::IHTMLElementPtr& firstCell, const MSHTML::IHTMLElementPtr& lastCell)
{
	try
	{
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		if (!body || !firstCell || !lastCell || FindTableElement(firstCell) != FindTableElement(lastCell)) return false;
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!range || !end) return false;
		range->moveToElementText(firstCell);
		if (firstCell != lastCell)
		{
			end->moveToElementText(lastCell);
			range->setEndPoint(L"EndToEnd", end);
		}
		else
			range->collapse(VARIANT_TRUE);
		range->select();
		return true;
	}
	catch (const _com_error&) { return false; }
}

static void GetDirectTableCells(const MSHTML::IHTMLElementPtr& row, std::vector<MSHTML::IHTMLElementPtr>& cells)
{
	cells.clear();
	if (!row) return;
	for (MSHTML::IHTMLDOMNodePtr node(MSHTML::IHTMLDOMNodePtr(row)->firstChild); node; node = node->nextSibling)
	{
		if (node->nodeType != NODE_ELEMENT) continue;
		MSHTML::IHTMLElementPtr element(node);
		if (IsTableCellElement(element)) cells.push_back(element);
	}
}

static void GetTableCells(const MSHTML::IHTMLElementPtr& table, std::vector<MSHTML::IHTMLElementPtr>& cells)
{
	cells.clear();
	if (!table) return;
	MSHTML::IHTMLElementCollectionPtr rows(MSHTML::IHTMLElement2Ptr(table)->getElementsByTagName(L"TR"));
	if (!rows) return;
	for (long rowIndex = 0; rowIndex < rows->length; ++rowIndex)
	{
		_variant_t itemIndex(rowIndex);
		MSHTML::IHTMLElementPtr row(rows->item(itemIndex, _variant_t()));
		std::vector<MSHTML::IHTMLElementPtr> rowCells;
		GetDirectTableCells(row, rowCells);
		cells.insert(cells.end(), rowCells.begin(), rowCells.end());
	}
}

static MSHTML::IHTMLElementPtr CreateTableCell(MSHTML::IHTMLDocument2Ptr document, const wchar_t* tagName);

struct LogicalTableCell { MSHTML::IHTMLElementPtr element; long sourceRow, startColumn, colspan, rowspan; };
struct LogicalTableGrid
{
	std::vector<MSHTML::IHTMLElementPtr> rows;
	std::vector<LogicalTableCell> cells;
	std::vector<std::vector<long> > slots;
	long columns;
	LogicalTableGrid() : columns(0) {}
	long At(long row, long column) const { return row >= 0 && row < static_cast<long>(slots.size()) && column >= 0 && column < static_cast<long>(slots[row].size()) ? slots[row][column] : -1; }
	void Ensure(long row, long column) { while (static_cast<long>(slots.size()) <= row) slots.push_back(std::vector<long>()); if (static_cast<long>(slots[row].size()) <= column) slots[row].resize(column + 1, -1); }
};
static long GetTableSpan(const MSHTML::IHTMLElementPtr& cell, const wchar_t* fbName, const wchar_t* htmlName)
{
	CString value(AU::GetAttrCS(cell, fbName)); if (value.IsEmpty()) value = AU::GetAttrCS(cell, htmlName);
	const long span = _wtol(value); return span > 0 ? span : 1;
}
static void SetTableSpan(const MSHTML::IHTMLElementPtr& cell, const wchar_t* fbName, const wchar_t* htmlName, long span)
{
	if (span <= 1) {
		cell->removeAttribute(fbName, 0);
		cell->removeAttribute(htmlName, 0);
	}
	else {
		CString value; value.Format(L"%ld", span);
		const _variant_t attributeValue((const wchar_t*)value);
		cell->setAttribute(fbName, attributeValue, 0);
		cell->setAttribute(htmlName, attributeValue, 0);
	}
}
static long g_tableGridBuildCount = 0;

static bool IsTableGridInstrumentationEnabled()
{
	static const bool enabled = []() -> bool {
		wchar_t testMode[4] = {};
		return ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MODE", testMode, _countof(testMode)) == 1 && testMode[0] == L'1';
	}();
	return enabled;
}

static bool BuildLogicalTableGrid(const MSHTML::IHTMLElementPtr& table, LogicalTableGrid& grid)
{
	if (IsTableGridInstrumentationEnabled()) ++g_tableGridBuildCount;
	if (!table) return false;
	MSHTML::IHTMLElementCollectionPtr tableRows(MSHTML::IHTMLElement2Ptr(table)->getElementsByTagName(L"TR")); if (!tableRows) return false;
	for (long rowIndex = 0; rowIndex < tableRows->length; ++rowIndex) {
		MSHTML::IHTMLElementPtr row(tableRows->item(_variant_t(rowIndex), _variant_t())); grid.rows.push_back(row);
		std::vector<MSHTML::IHTMLElementPtr> rowCells; GetDirectTableCells(row, rowCells); long column = 0;
		for (size_t physicalIndex = 0; physicalIndex < rowCells.size(); ++physicalIndex) {
			grid.Ensure(rowIndex, column); while (grid.At(rowIndex, column) >= 0) { ++column; grid.Ensure(rowIndex, column); }
			LogicalTableCell cell = { rowCells[physicalIndex], rowIndex, column, GetTableSpan(rowCells[physicalIndex], L"fbcolspan", L"colspan"), GetTableSpan(rowCells[physicalIndex], L"fbrowspan", L"rowspan") };
			const long cellIndex = static_cast<long>(grid.cells.size()); grid.cells.push_back(cell);
			for (long coveredRow = rowIndex; coveredRow < rowIndex + cell.rowspan; ++coveredRow) for (long coveredColumn = column; coveredColumn < column + cell.colspan; ++coveredColumn) { grid.Ensure(coveredRow, coveredColumn); grid.slots[coveredRow][coveredColumn] = cellIndex; }
			column += cell.colspan;
		}
	}
	for (size_t row = 0; row < grid.slots.size(); ++row) if (static_cast<long>(grid.slots[row].size()) > grid.columns) grid.columns = static_cast<long>(grid.slots[row].size());
	return !grid.rows.empty();
}
static long FindLogicalCell(const LogicalTableGrid& grid, const MSHTML::IHTMLElementPtr& element)
{
	for (size_t index = 0; index < grid.cells.size(); ++index) if (grid.cells[index].element == element) return static_cast<long>(index); return -1;
}

static bool GetTableCellRectangle(const MSHTML::IHTMLElementPtr& firstCell,
	const MSHTML::IHTMLElementPtr& lastCell, std::vector<MSHTML::IHTMLElementPtr>& result)
{
	result.clear();
	MSHTML::IHTMLElementPtr table(FindTableElement(firstCell));
	if (!table || table != FindTableElement(lastCell)) return false;
	LogicalTableGrid grid;
	if (!BuildLogicalTableGrid(table, grid)) return false;
	const long first = FindLogicalCell(grid, firstCell), last = FindLogicalCell(grid, lastCell);
	if (first < 0 || last < 0) return false;
	const LogicalTableCell& a = grid.cells[first]; const LogicalTableCell& b = grid.cells[last];
	const long rowStart = min(a.sourceRow, b.sourceRow), rowEnd = max(a.sourceRow + a.rowspan - 1, b.sourceRow + b.rowspan - 1);
	const long columnStart = min(a.startColumn, b.startColumn), columnEnd = max(a.startColumn + a.colspan - 1, b.startColumn + b.colspan - 1);
	std::vector<bool> selected(grid.cells.size(), false);
	for (long row = rowStart; row <= rowEnd; ++row) for (long column = columnStart; column <= columnEnd; ++column) {
		const long owner = grid.At(row, column); if (owner >= 0) selected[owner] = true;
	}
	for (size_t index = 0; index < grid.cells.size(); ++index) if (selected[index]) result.push_back(grid.cells[index].element);
	return !result.empty();
}

static void SetTableCellHighlight(const MSHTML::IHTMLElementPtr& cell, const wchar_t* color)
{
	try {
		IDispatchPtr dispatch(cell);
		OLECHAR* propertyName = L"runtimeStyle";
		DISPID propertyId = DISPID_UNKNOWN;
		if (!dispatch || FAILED(dispatch->GetIDsOfNames(IID_NULL, &propertyName, 1, LOCALE_USER_DEFAULT, &propertyId))) return;
		DISPPARAMS arguments = {};
		_variant_t runtimeStyle;
		if (FAILED(dispatch->Invoke(propertyId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &arguments, &runtimeStyle, NULL, NULL)) || runtimeStyle.vt != VT_DISPATCH) return;
		MSHTML::IHTMLStylePtr style;
		style = runtimeStyle.pdispVal;
		if (style) style->backgroundColor = color;
	}
	catch (const _com_error&) { }
}

static void UpdateTableCellHighlights(std::vector<MSHTML::IHTMLElementPtr>& previous,
	const std::vector<MSHTML::IHTMLElementPtr>& current)
{
	for (size_t index = 0; index < previous.size(); ++index) SetTableCellHighlight(previous[index], L"");
	previous = current;
	for (size_t index = 0; index < previous.size(); ++index) SetTableCellHighlight(previous[index], L"#B8D6FB");
}

static const wchar_t* TableCellTagAt(const LogicalTableGrid& grid, long row, long column, const wchar_t* fallback)
{
	const long index = grid.At(row, column);
	return index >= 0 && index < static_cast<long>(grid.cells.size()) && U::scmp(grid.cells[index].element->tagName, L"TH") == 0 ? L"TH" :
		index >= 0 && index < static_cast<long>(grid.cells.size()) ? L"TD" : fallback;
}
static void InsertCellAtLogicalColumn(MSHTML::IHTMLDocument2Ptr document, const LogicalTableGrid& grid, long rowIndex, long column, const wchar_t* tagName)
{
	if (rowIndex < 0 || rowIndex >= static_cast<long>(grid.rows.size())) return;
	MSHTML::IHTMLElementPtr cell(CreateTableCell(document, tagName)); long before = -1;
	for (size_t index = 0; index < grid.cells.size(); ++index) if (grid.cells[index].sourceRow == rowIndex && grid.cells[index].startColumn >= column && (before < 0 || grid.cells[index].startColumn < grid.cells[before].startColumn)) before = static_cast<long>(index);
	if (before >= 0) MSHTML::IHTMLElement2Ptr(grid.cells[before].element)->insertAdjacentElement(L"beforeBegin", cell); else MSHTML::IHTMLElement2Ptr(grid.rows[rowIndex])->insertAdjacentElement(L"beforeEnd", cell);
}

static MSHTML::IHTMLElementPtr CreateTableCell(MSHTML::IHTMLDocument2Ptr document, const wchar_t* tagName)
{
	MSHTML::IHTMLElementPtr cell(document->createElement(tagName));
	cell->className = U::scmp(tagName, L"TH") == 0 ? L"th" : L"td";
	return cell;
}

static MSHTML::IHTMLElementPtr CreateTableRowLike(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& sourceRow)
{
	MSHTML::IHTMLElementPtr row(document->createElement(L"TR"));
	row->className = L"tr";
	std::vector<MSHTML::IHTMLElementPtr> cells;
	GetDirectTableCells(sourceRow, cells);
	for (size_t index = 0; index < cells.size(); ++index)
		MSHTML::IHTMLElement2Ptr(row)->insertAdjacentElement(L"beforeEnd", CreateTableCell(document, cells[index]->tagName));
	return row;
}

static CString GetLocalizedMainMenuText(UINT commandId, const wchar_t* fallback)
{
	const wchar_t* runtimeKey = NULL;
	switch (commandId)
	{
	case ID_TABLE_INSERT_ROW_ABOVE: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_row_above"; break;
	case ID_TABLE_INSERT_ROW_BELOW: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_row_below"; break;
	case ID_TABLE_DELETE_ROW: runtimeKey = L"fbe.menu.idr_mainframe.table.delete_row"; break;
	case ID_TABLE_INSERT_COLUMN_LEFT: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_column_left"; break;
	case ID_TABLE_INSERT_COLUMN_RIGHT: runtimeKey = L"fbe.menu.idr_mainframe.table.insert_column_right"; break;
	case ID_TABLE_DELETE_COLUMN: runtimeKey = L"fbe.menu.idr_mainframe.table.delete_column"; break;
	case ID_TABLE_MAKE_HEADER_CELLS: runtimeKey = L"fbe.menu.idr_mainframe.table.make_header_cells"; break;
	case ID_TABLE_MAKE_NORMAL_CELLS: runtimeKey = L"fbe.menu.idr_mainframe.table.make_normal_cells"; break;
	}
	if (runtimeKey != NULL)
		return FbeLoadRuntimeStringByKey(runtimeKey, fallback);

	CMenu mainMenu;
	if (mainMenu.LoadMenu(IDR_MAINFRAME))
	{
		wchar_t text[256] = {};
		if (mainMenu.GetMenuString(commandId, text, _countof(text), MF_BYCOMMAND) > 0)
			return CString(text);
	}
	return CString(fallback);
}

static void CopyTableCellAttribute(const MSHTML::IHTMLElementPtr& source, const MSHTML::IHTMLElementPtr& destination, const wchar_t* name)
{
	_variant_t value(source->getAttribute(name, 0));
	if (value.vt != VT_EMPTY && value.vt != VT_NULL)
		destination->setAttribute(name, value, 0);
}

// TD/TH replacement must preserve the same visual and FB2-facing cell data,
// regardless of whether it is initiated by the single-cell toggle or bulk action.
static const wchar_t* const kTableCellReplacementAttributes[] = {
	L"id", L"style", L"fbstyle", L"colspan", L"fbcolspan", L"rowspan", L"fbrowspan",
	L"align", L"fbalign", L"valign", L"fbvalign"
};

static void CopyTableCellReplacementAttributes(const MSHTML::IHTMLElementPtr& source, const MSHTML::IHTMLElementPtr& destination)
{
	for (size_t index = 0; index < _countof(kTableCellReplacementAttributes); ++index)
		CopyTableCellAttribute(source, destination, kTableCellReplacementAttributes[index]);
	// MSHTML can keep a runtime CSS declaration in IHTMLStyle without exposing
	// it through getAttribute(L"style"). Preserve that visual-DOM state too.
	MSHTML::IHTMLStylePtr sourceStyle(source ? source->style : MSHTML::IHTMLStylePtr());
	MSHTML::IHTMLStylePtr destinationStyle(destination ? destination->style : MSHTML::IHTMLStylePtr());
	if (sourceStyle && destinationStyle) {
		_bstr_t cssText(sourceStyle->cssText);
		if (cssText.length()) destinationStyle->cssText = (const wchar_t*)cssText;
	}
}

static void NotifyWrappedSearch(bool wrapped)
{
	if(wrapped)
		::MessageBeep(MB_ICONASTERISK);
}

static void ReleaseSearchRegExp(AU::RegExp& re)
{
	delete re;
	re = NULL;
}

// ��������� RAII-������ ��� regex wrapper-� ������ ��������.
// ������ ����������� ��������� ������������� �������������,
// � ������� ���������� ��� ������������� ���������� �������� � m_fo.match.
class ScopedSearchRegExp
{
public:
	ScopedSearchRegExp()
		: m_re(CreateSearchRegExp())
	{
	}

	~ScopedSearchRegExp()
	{
		ReleaseSearchRegExp(m_re);
	}

	AU::RegExp get() const
	{
		return m_re;
	}

	AU::IRegExp2* operator->() const
	{
		return m_re;
	}

private:
	AU::RegExp m_re;
};

static void InitSearchRegExp(AU::RegExp re, int flags, const CString& pattern)
{
	re->IgnoreCase = flags & 4 ? VARIANT_FALSE : VARIANT_TRUE;
	re->Global = VARIANT_TRUE;
	re->Pattern = (const wchar_t*)pattern;
}

static void NormalizeSearchPatternNbsp(CString& pattern)
{
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		pattern.Replace(L"\u00A0", _Settings.GetNBSPChar());
}

static void NormalizeReplacementNbsp(CString& replacement)
{
	if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
		replacement.Replace(L"\u00A0", _Settings.GetNBSPChar());
}

_ATL_FUNC_INFO CFBEView::DocumentCompleteInfo=
  { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, (VT_BYREF | VT_VARIANT) } };
_ATL_FUNC_INFO CFBEView::BeforeNavigateInfo=
  { CC_STDCALL, VT_EMPTY, 7, {
      VT_DISPATCH,
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_VARIANT),
      (VT_BYREF | VT_BOOL),
    }
  };
_ATL_FUNC_INFO CFBEView::NavigateErrorInfo=
  { CC_STDCALL, VT_EMPTY, 5, { VT_DISPATCH, (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_VARIANT), (VT_BYREF | VT_BOOL) } };_ATL_FUNC_INFO CFBEView::VoidInfo=
  { CC_STDCALL, VT_EMPTY, 0 };
_ATL_FUNC_INFO CFBEView::EventInfo=
  { CC_STDCALL, VT_BOOL, 1, { VT_DISPATCH } };
_ATL_FUNC_INFO CFBEView::VoidEventInfo=
  { CC_STDCALL, VT_EMPTY, 1, { VT_DISPATCH } };

LRESULT CFBEView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
  if (DefWindowProc(uMsg,wParam,lParam))
    return 1;
  HRESULT hr = QueryControl(&m_browser);
  StartupTrace::HResult(L"webbrowser", L"WB111", hr, L"QueryControl(IWebBrowser2)");
  if (FAILED(hr) || !m_browser)
    return 1;

  hr = BrowserEvents::DispEventAdvise(m_browser, &DIID_DWebBrowserEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB112", hr, L"BrowserEvents::DispEventAdvise");
  if (FAILED(hr))
    return 1;
  m_last_browser_event = L"BrowserEventsAdvised";
  return 0;
}

CFBEView::~CFBEView()
{
	if(HasDoc())
	{
		// Init can fail after acquiring the document but before all event sinks and
		// markup services are available. Teardown must be best-effort in that case.
		DocumentEvents::DispEventUnadvise(Document(), &DIID_HTMLDocumentEvents2);
		try
		{
			MSHTML::IHTMLElementPtr body;
			if (SUCCEEDED(m_hdoc->get_body(&body)) && body)
				TextEvents::DispEventUnadvise(body, &DIID_HTMLTextContainerEvents2);
		}
		catch (const _com_error&)
		{
		}
		if (m_mkc && m_dirtyRangeCookie)
			m_mkc->UnRegisterForDirtyRange(m_dirtyRangeCookie);
	}
	if(m_browser)
		BrowserEvents::DispEventUnadvise(m_browser, &DIID_DWebBrowserEvents2);

	if(m_find_dlg)
	{
		CloseFindDialog(m_find_dlg);
		delete m_find_dlg;
	}
}

BOOL CFBEView::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB &&
		MoveTableCell((::GetKeyState(VK_SHIFT) & 0x8000) != 0))
		return TRUE;
	return SendMessage(WM_FORWARDMSG,0,(LPARAM)pMsg)!=0;
}

// editing commands
LRESULT CFBEView::ExecCommand(int cmd) {
  IOleCommandTargetPtr	  ct(m_browser);
  if (ct)
    ct->Exec(&CGID_MSHTML,cmd,0,NULL,NULL);
  return 0;
}

void	  CFBEView::QueryStatus(OLECMD *cmd,int ncmd) {
  IOleCommandTargetPtr	  ct(m_browser);
  if (ct)
    ct->QueryStatus(&CGID_MSHTML,ncmd,cmd,NULL);
}

CString	  CFBEView::QueryCmdText(int cmd) {
  IOleCommandTargetPtr	  ct(m_browser);
  if (ct) {
    OLECMD oc={static_cast<ULONG>(cmd)};
    struct {
      OLECMDTEXT	oct;
      wchar_t		buffer[512];
    } oct={ { OLECMDTEXTF_NAME, 0, 512 } };
    if (SUCCEEDED(ct->QueryStatus(&CGID_MSHTML,1,&oc,&oct.oct)))
      return oct.oct.rgwz;
  }
  return CString();
}

LRESULT CFBEView::OnStyleLink(WORD, WORD, HWND, BOOL&) {
  try {
    if (Document()->execCommand(L"CreateLink",VARIANT_FALSE,_variant_t(L""))==VARIANT_TRUE)
    {
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(IDC_HREF,IDN_WANTFOCUS),(LPARAM)m_hWnd);
    }
  }
  catch (_com_error&) { }
  return 0;
}

LRESULT CFBEView::OnStyleFootnote(WORD, WORD, HWND, BOOL&) {
  try {
    m_mk_srv->BeginUndoUnit(L"Create Footnote");
    if (Document()->execCommand(L"CreateLink",VARIANT_FALSE,_variant_t(L""))==VARIANT_TRUE) {
      MSHTML::IHTMLTxtRangePtr  r(Document()->selection->createRange());
      MSHTML::IHTMLElementPtr	pe(r->parentElement());
      if (U::scmp(pe->tagName,L"A")==0)
	pe->className=L"note";
    }
    m_mk_srv->EndUndoUnit();
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(IDC_HREF,IDN_WANTFOCUS),(LPARAM)m_hWnd);
  }
  catch (_com_error&) { }
  return 0;
}

bool CFBEView::CheckCommand(WORD wID) {
  if (!HasDoc())
    return false;
  if (!m_normalize)
    return false;
  switch (wID) {
  case ID_EDIT_ADD_BODY:
    return true;
  case ID_EDIT_ADD_TITLE:
    return bCall(L"AddTitle",SelectionStructCon());
  case ID_EDIT_CLONE:
    return bCall(L"CloneContainer",SelectionStructCon());
  case ID_STYLE_NORMAL:
    return bCall(L"StyleNormal",SelectionStructCon());
  case ID_STYLE_SUBTITLE:
    return bCall(L"StyleSubtitle",SelectionStructCon());
  case ID_STYLE_TEXTAUTHOR:
    return bCall(L"StyleTextAuthor",SelectionStructCon());
  case ID_EDIT_INS_IMAGE:
    return bCall(L"InsImage") && !SelectionStructCode() && !SelectionHasTags(L"SPAN");
  case ID_EDIT_INS_INLINEIMAGE:
    return bCall(L"InsInlineImage");
  case ID_EDIT_ADD_IMAGE:
    return bCall(L"AddImage", SelectionStructCon()) && !SelectionStructCode() && !SelectionHasTags(L"SPAN");
  case ID_EDIT_ADD_EPIGRAPH:
    return bCall(L"AddEpigraph",SelectionStructCon());
  case ID_EDIT_ADD_ANN:
    return bCall(L"AddAnnotation",SelectionStructCon());
  case ID_EDIT_SPLIT:
    return SplitContainer(true);
  case ID_EDIT_INS_POEM:
    return InsertPoem(true);
  case ID_EDIT_INS_CITE:
    return InsertCite(true);
	case ID_EDIT_CODE:
		{
			_variant_t params[3] =
			{
				Document()->selection->createRange().GetInterfacePtr(),
				SelectionStructCon().GetInterfacePtr(),
				true
			};
			return bCall(L"StyleCode", 3, params);
		}
  case ID_INSERT_TABLE:
	  return InsertTable(true);
  case ID_TABLE_INSERT_ROW_ABOVE:
  case ID_TABLE_INSERT_ROW_BELOW:
  case ID_TABLE_DELETE_ROW:
  case ID_TABLE_INSERT_COLUMN_LEFT:
  case ID_TABLE_INSERT_COLUMN_RIGHT:
  case ID_TABLE_DELETE_COLUMN:
  case ID_TABLE_TOGGLE_HEADER_CELL:
	case ID_TABLE_MAKE_HEADER_CELLS:
	case ID_TABLE_MAKE_NORMAL_CELLS:
	  return (bool)SelectionStructTableCon();
  case ID_GOTO_FOOTNOTE:
	  {
		  const bool footnoteFound = GoToFootnote(true);
		  const bool referenceFound = GoToReference(true);
		  return footnoteFound || referenceFound;
	  }
  case ID_GOTO_REFERENCE:
	  return GoToReference(true);
  case ID_EDIT_ADD_TA:
    return bCall(L"AddTA",SelectionStructCon());
  case ID_EDIT_MERGE:
    return bCall(L"MergeContainers",SelectionStructCon());
  case ID_EDIT_REMOVE_OUTER_SECTION:
    return bCall(L"RemoveOuterContainer",SelectionStructCon());
  case ID_STYLE_LINK:
  case ID_STYLE_NOTE:
    try {
      return Document()->queryCommandEnabled(L"CreateLink")==VARIANT_TRUE;
    }
    catch (const _com_error&) { }
    break;
  }
  return false;
}

bool	CFBEView::CheckSetCommand(WORD wID) {
	if (!m_normalize)
		return false;

	switch (wID)
	{
		case ID_EDIT_CODE:
			return bCall(L"IsCode", SelectionStructCode());
	}

	return false;
}

// changes tracking
MSHTML::IHTMLDOMNodePtr	  CFBEView::GetChangedNode() {
  MSHTML::IMarkupPointerPtr	  p1,p2;
  m_mk_srv->CreateMarkupPointer(&p1);
  m_mk_srv->CreateMarkupPointer(&p2);

  m_mkc->GetAndClearDirtyRange(m_dirtyRangeCookie,p1,p2);

  MSHTML::IHTMLElementPtr	  e1,e2;
  p1->CurrentScope(&e1);
  p2->CurrentScope(&e2);
  p1.Release();
  p2.Release();

  while ((bool)e1 && e1!=e2 && e1->contains(e2)!=VARIANT_TRUE)
    e1=e1->parentElement;

  return e1;
}

static bool IsEmptyNode(MSHTML::IHTMLDOMNode *node) {
	if (node->nodeType!=1)
		return false;

	_bstr_t   name(node->nodeName);

	if (U::scmp(name,L"BR")==0)
		return false;

	if (U::scmp(name,L"P")==0) // the editor uses empty Ps to represent empty lines
		return false;

	/* if (U::scmp(name,L"EM")==0) // ���������� ������ ��������� ������� ������ <emphasis> � <strong>
	return false;

	if (U::scmp(name,L"STRONG")==0) // ���������� ������ ��������� ������� ������ <emphasis> � <strong>
	return false;*/

	// images are always empty
	if (U::scmp(name,L"DIV")==0 && U::scmp(MSHTML::IHTMLElementPtr(node)->className,L"image")==0)
		return false;
	if (U::scmp(name,L"IMG")==0)
		return false;

	if (node->hasChildNodes()==VARIANT_FALSE)
		return true;

	if (U::scmp(name,L"A")==0) // links can be meaningful even if the contain only ws
		return false;

	if ((bool)node->firstChild->nextSibling)
		return false;

	if (node->firstChild->nodeType!=3)
		return false;

	if (U::is_whitespace(node->firstChild->nodeValue.bstrVal))
		return true;

	return false;
}

// Remove empty leaf nodes
static void RemoveEmptyNodes(MSHTML::IHTMLDOMNode *node) {
	if (node->nodeType!=1)
		return;
	_bstr_t nodeName(node->nodeName);
	if (U::scmp(nodeName, L"TABLE") == 0 || U::scmp(nodeName, L"TBODY") == 0 ||
		U::scmp(nodeName, L"TR") == 0 || U::scmp(nodeName, L"TD") == 0 || U::scmp(nodeName, L"TH") == 0)
		return;

	MSHTML::IHTMLDOMNodePtr cur(node->firstChild);
	while (cur)
	{
		MSHTML::IHTMLDOMNodePtr next;
		try { next = cur->nextSibling; } catch(...) { return; }

		RemoveEmptyNodes(cur);
		if(IsEmptyNode(cur))
			cur->removeNode(VARIANT_TRUE);
		cur=next;
	}
}

// Find parent DIV
static MSHTML::IHTMLElementPtr GetHP(MSHTML::IHTMLElementPtr hp)
{
	while((bool)hp && U::scmp(hp->tagName,L"DIV"))
		hp = hp->parentElement;
	return hp;
}

// Splitting
bool CFBEView::SplitContainer(bool fCheck)
{
	try
	{
		MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
		if(!(bool)rng)
			return false;

		MSHTML::IHTMLElementPtr pe(rng->parentElement());
		while((bool)pe && U::scmp(pe->tagName, L"DIV"))
			pe = pe->parentElement;

		if(!(bool)pe || (U::scmp(pe->className, L"section") && U::scmp(pe->className, L"stanza")))
			return false;

		MSHTML::IHTMLTxtRangePtr r2(rng->duplicate());
			r2->moveToElementText(pe);

		if(rng->compareEndPoints(L"StartToStart", r2) == 0)
			return false;

		MSHTML::IHTMLTxtRangePtr r3(rng->duplicate());
		r3->collapse(VARIANT_TRUE);
		MSHTML::IHTMLTxtRangePtr r4(rng->duplicate());
		r4->collapse(VARIANT_FALSE);

		if(!(bool)pe || GetHP(r3->parentElement()) != pe || GetHP(r4->parentElement()) != pe)
			return false;

		if(fCheck)
			return true;

		// At this point we are ready to split

		// Create an undo unit
		CString name(L"split ");
		name += (const wchar_t*)pe->className;
		m_mk_srv->BeginUndoUnit((TCHAR*)(const TCHAR*)name);

		//// Create a new element
		MSHTML::IHTMLElementPtr ne(Document()->createElement(L"DIV"));
		ne->className = pe->className;
		_bstr_t className = pe->className;
		// SeNS: issue #153

		MSHTML::IHTMLElementPtr peTitle(Document()->createElement(L"DIV"));
		MSHTML::IHTMLElementCollectionPtr peColl = pe->children;
		{
			MSHTML::IHTMLElementPtr peChild = peColl->item(0);
			if(!U::scmp(peChild->tagName, L"DIV") && !U::scmp(peChild->className, L"title"))
				peTitle->innerHTML = peChild->outerHTML;
			else
				peTitle = NULL;
		}

		// Create and position markup pointers
		MSHTML::IMarkupPointerPtr selstart, selend, elembeg, elemend;

		m_mk_srv->CreateMarkupPointer(&selstart);
		m_mk_srv->CreateMarkupPointer(&selend);
		m_mk_srv->CreateMarkupPointer(&elembeg);
		m_mk_srv->CreateMarkupPointer(&elemend);

		MSHTML::IHTMLTxtRangePtr titleRng(rng->duplicate());
		m_mk_srv->MovePointersToRange(titleRng, selstart, selend);
		U::ElTextHTML title(titleRng->htmlText, titleRng->text);

		MSHTML::IHTMLTxtRangePtr preRng(rng->duplicate());
		elembeg->MoveAdjacentToElement(pe, MSHTML::ELEM_ADJ_AfterBegin);
		m_mk_srv->MoveRangeToPointers(elembeg, selstart, preRng);
		U::ElTextHTML pre(preRng->htmlText, preRng->text);

		MSHTML::IHTMLElementCollectionPtr peChilds = pe->children;
		MSHTML::IHTMLElementPtr elLast = peChilds->item(peChilds->length - 1);
		if(U::scmp(elLast->innerText, L"") == 0)
			elLast->innerText = L"123";

		MSHTML::IHTMLTxtRangePtr postRng(rng->duplicate());
		elemend->MoveAdjacentToElement(pe, MSHTML::ELEM_ADJ_BeforeEnd);
		m_mk_srv->MoveRangeToPointers(selend, elemend, postRng);
		U::ElTextHTML post(postRng->htmlText, postRng->text);

		// Check if title needs to be created and further text to be copied
		bool fTitle = !title.text.IsEmpty();
		bool fContent = !post.html.IsEmpty();

		_bstr_t id;
		if(fContent)
			id = pe->id;
		pe->id = L"";

		if(fTitle && title.html.Find(L"<P") == -1)
			title.html = CString(L"<P>") + title.html + CString(L"</P>");
		if(fContent && post.html.Find(L"<P") == -1)
			post.html = CString(L"<P>") + post.html + CString(L"</P>");

		title.html.Remove(L'\r');
		title.html.Remove(L'\n');
		post.html.Remove(L'\r');
		post.html.Remove(L'\n');

		if(post.html.Find(L"<P>&nbsp;</P>") == 0
			&& post.html.GetLength() > 13
			&& fTitle
			&& title.html.Find(L"<P>&nbsp;</P>") != title.html.GetLength() -14)
			post.html.Delete(0, 13);
		if(post.html.Find(L"<P>123</P>") != -1)
			post.html.Replace(L"<P>123</P>", L"<P>&nbsp;</P>");

		// Insert it after pe
		MSHTML::IHTMLElement2Ptr(pe)->insertAdjacentElement(L"afterEnd", ne);

		// Move content or create new
		if(fContent)
		{
			// Create and position destination markup pointer
			if(post.html == L"<P>&nbsp;</P>")
				post.html += L"<P>&nbsp;</P>";
			ne->innerHTML = post.html.AllocSysString();
			// SeNS: issue #153
			ne->id = id;
		}
		else
		{
			MSHTML::IHTMLElementPtr para(Document()->createElement(L"P"));
			MSHTML::IHTMLElement3Ptr(para)->inflateBlock = VARIANT_TRUE;
			MSHTML::IHTMLElement2Ptr(ne)->insertAdjacentElement(L"beforeEnd", para);
		}

		// Create and move title if needed
		if(fTitle)
		{
			MSHTML::IHTMLElementPtr elTitle(Document()->createElement(L"DIV"));
			elTitle->className = L"title";
			MSHTML::IHTMLElement2Ptr(ne)->insertAdjacentElement(L"afterBegin", elTitle);

			// Create and position destination markup pointer
			elTitle->innerHTML = title.html.AllocSysString();

			// Delete all containers from title
			KillDivs(elTitle);
			KillStyles(elTitle);
		}

		if(pre.html.Find(L"<P") == -1)
		{
			if(pre.html.IsEmpty())
				pre.html = L"<P>&nbsp;</P>";
			else
				pre.html = CString(L"<P>") + pre.html + CString(L"</P>");
		}
		pe->innerHTML = pre.html.AllocSysString();

		// Ensure we have good html
		FixupParagraphs(ne);
		PackText(ne, Document());

		peColl = pe->children;
		if(peColl->length == 1)
		{
			MSHTML::IHTMLElementPtr peChild = peColl->item(0);
			if(!U::scmp(peChild->tagName, L"DIV") && !U::scmp(peChild->className, className.GetBSTR()))
				m_mk_srv->RemoveElement(peChild);
		}

		MSHTML::IHTMLElementCollectionPtr neColl = ne->children;
		if(neColl->length == 1)
		{
			MSHTML::IHTMLElementPtr neChild = neColl->item(0);
			if(!U::scmp(neChild->tagName, L"DIV") && !U::scmp(neChild->className, className.GetBSTR()))
				m_mk_srv->RemoveElement(neChild);
		}

		CString peTitSect;
		if(peTitle)
		{
			peTitSect = peTitle->innerHTML.GetBSTR();
			peTitSect += L"<P>&nbsp;</P>";
		}

		CString b = pe->innerText;
		b.Remove(L'\r');
		b.Remove(L'\n');
		CString c = peTitle ? peTitle->innerText : L"";
		c.Remove(L'\r');
		c.Remove(L'\n');

		if(peTitle && !U::scmp(b, c))
			pe->innerHTML = peTitSect.AllocSysString();

		// Close undo unit
		m_mk_srv->EndUndoUnit();

		// Move cursor to newly created item
		GoTo(ne, false);
	}
	catch (_com_error& e)
	{
		U::ReportError(e);
	}

	return false;
}

// cleaning up html
static void KillDivs(MSHTML::IHTMLElement2Ptr elem) {
	MSHTML::IHTMLElementCollectionPtr	  divs(elem->getElementsByTagName(L"DIV"));
	while (divs->length>0)
		MSHTML::IHTMLDOMNodePtr(divs->item(0L))->removeNode(VARIANT_FALSE);
}

static void KillStyles(MSHTML::IHTMLElement2Ptr elem) {
	MSHTML::IHTMLElementCollectionPtr	  ps(elem->getElementsByTagName(L"P"));
	for (long l=0;l<ps->length;++l)
		CheckError(MSHTML::IHTMLElementPtr(ps->item(l))->put_className(NULL));
}

//////////////////////////////////////////////////////////////////////////////
/// @fn static bool	MergeEqualHTMLElements(MSHTML::IHTMLDOMNode *node)
///
/// ������� ���������� ������� ����� ���������� HTML ��������
///
/// @params MSHTML::IHTMLDOMNode *node [in, out] - ����, ������ ������� ����� ������������� ��������������
///
/// @note ��������� ��������� ��������: EM, STRONG
/// ��� ���� ���������� �������, ��������������� ����� ����������� � ����������� ������ ��������, �.�. 
/// '<EM>�������</EM> <EM>������</EM>' ������������� � '<EM>������� ������</EM>'
///
/// @author ����� ���� @date 31.03.08
//////////////////////////////////////////////////////////////////////////////
static bool	MergeEqualHTMLElements(MSHTML::IHTMLDOMNode *node, MSHTML::IHTMLDocument2 *doc)
{
	if (node->nodeType != 1) // Element node
		return false;

	bool	fRet=false;


	MSHTML::IHTMLDOMNodePtr   cur(node->firstChild);
	while ((bool)cur) 
	{
		MSHTML::IHTMLDOMNodePtr next;
		try { next = cur->nextSibling; } catch(...) { return false; }

		if (MergeEqualHTMLElements(cur,doc))
		{
			cur = node->firstChild;
			continue;
		}

		// ���� ��� ���������� ��������, �� ������� ����� ������
		if(!(bool)next)
			return false;

		_bstr_t	name(cur->nodeName);	

		if (U::scmp(name,L"EM")==0 || U::scmp(name,L"STRONG")==0) 
		{
			MSHTML::IHTMLElementPtr	curelem(cur);
			// ����������� �������� � ��������, ����������� ������ EM �.�.
			bstr_t curText = curelem->innerText;
			if(curText.length() == 0 || U::is_whitespace(curelem->innerText))
			{
				// ������� ����������� ����				
				MSHTML::IHTMLDOMNodePtr prev = cur->previousSibling;
				if((bool)prev)
				{
					if(prev->nodeType == 3)//text
					{
						prev->nodeValue = (bstr_t)prev->nodeValue.bstrVal + curelem->innerText;						
					}
					else
					{
						MSHTML::IHTMLElementPtr prevElem(prev);
						prevElem->innerHTML = prevElem->innerHTML + curelem->innerText;
					}
					cur->removeNode(VARIANT_TRUE);
					cur = prev;
					continue;
				}

				if((bool)next)
				{
					MSHTML::IHTMLDOMNodePtr parent = cur->parentNode;
					if(next->nodeType == 3)//text
					{
						next->nodeValue = (bstr_t)curelem->innerText + next->nodeValue.bstrVal;
					}
					else
					{
						MSHTML::IHTMLElementPtr nextElem(next);
						nextElem->innerHTML = curelem->innerText + nextElem->innerHTML;
					}
					cur->removeNode(VARIANT_TRUE);
					cur = parent->firstChild;
					continue;
				}
			}

			if(next->nodeType == 3) // TextNode
			{
				MSHTML::IHTMLDOMNodePtr afterNext(next->nextSibling);
				if(!(bool)afterNext)
				{
					cur = next;
					continue;
				}

				MSHTML::IHTMLElementPtr	afterNextElem(afterNext);

				bstr_t afterNextName = afterNext->nodeName;
				if(U::scmp(name, afterNextName))// ���� ��������� ������� ������� ����
				{
					cur = next;
					continue;
				}

				// ��������� ����� ����������� ���������� ����� ���� �������
				if(!U::is_whitespace(next->nodeValue.bstrVal))
				{
					cur = next;
					continue; // <EM>123</EM>45<EM>678</EM> ��������� ���������� ��������
				}

				// ���������� ��������
				MSHTML::IHTMLElementPtr	newelem(doc->createElement(name));
				MSHTML::IHTMLDOMNodePtr	newnode(newelem);
				newelem->innerHTML = curelem->innerHTML + next->nodeValue.bstrVal + afterNextElem->innerHTML;
				cur->replaceNode(newnode);
				afterNext->removeNode(VARIANT_TRUE);
				next->removeNode(VARIANT_TRUE);
				cur = newnode;
				fRet=true;
			}
			else
			{
				bstr_t nextName(next->nodeName);
				if(U::scmp(name, nextName))// ���� ��������� ������� ������� ����
				{
					cur = next;
					continue;
				}

				// ���������� ��������
				MSHTML::IHTMLElementPtr	nextElem(next);
				MSHTML::IHTMLElementPtr	newelem(doc->createElement(name));
				MSHTML::IHTMLDOMNodePtr	newnode(newelem);
				newelem->innerHTML = curelem->innerHTML + nextElem->innerHTML;
				cur->replaceNode(newnode);
				next->removeNode(VARIANT_TRUE);
				cur = newnode;
				fRet=true;
				continue;
			}
		}
		cur=next;
	}
	return fRet;
}
static bool   RemoveUnk(MSHTML::IHTMLDOMNode *node, MSHTML::IHTMLDocument2 *doc) {
	if (node->nodeType!=1) // Element node
		return false;

	bool	fRet=false;

restart:
	MSHTML::IHTMLDOMNodePtr   cur(node->firstChild);
	while ((bool)cur) 
	{
		MSHTML::IHTMLDOMNodePtr next;
		try { next = cur->nextSibling; } catch(...) { return false; }

		if (RemoveUnk(cur,doc))
			goto restart;

		_bstr_t			name(cur->nodeName);
		MSHTML::IHTMLElementPtr	curelem(cur);

		if (U::scmp(name,L"B")==0 || U::scmp(name,L"I")==0) {
			const wchar_t		*newname=U::scmp(name,L"B")==0 ? L"STRONG" : L"EM";
			MSHTML::IHTMLElementPtr	newelem(doc->createElement(newname));
			MSHTML::IHTMLDOMNodePtr	newnode(newelem);
			newelem->innerHTML=curelem->innerHTML;
			cur->replaceNode(newnode);
			cur=newnode;
			fRet=true;
			goto restart;
		}

		CString text;
		if (curelem != NULL)
			text.SetString(curelem->outerHTML);

		if (U::scmp(name,L"P") && U::scmp(name,L"STRONG") && 
			U::scmp(name,L"STRIKE") && U::scmp(name,L"SUP") && U::scmp(name,L"SUB") && 
			U::scmp(name,L"EM") && U::scmp(name,L"A") &&
			U::scmp(name,L"TABLE") && U::scmp(name,L"TBODY") && U::scmp(name,L"TR") &&
			U::scmp(name,L"TD") && U::scmp(name,L"TH") &&
			(U::scmp(name,L"SPAN") || U::scmp(curelem->className, L"code")) &&
			U::scmp(name,L"#text") && U::scmp(name,L"BR") &&
			(U::scmp(name,L"IMG") || U::scmp(curelem->parentElement->className, L"image")) &&
			// Added by SeNS: inline images support
			(U::scmp(name,L"SPAN") || U::scmp(curelem->className, L"image")))
		{
			if (U::scmp(name,L"DIV")==0) {
				_bstr_t	  cls(curelem->className);
				_bstr_t	  id(curelem->id);
				if (!(U::scmp(cls,L"body") && U::scmp(cls,L"section") &&
					U::scmp(cls,L"table") && U::scmp(cls,L"tr") && U::scmp(cls,L"th") && U::scmp(cls,L"td") && 
					U::scmp(cls,L"output") && U::scmp(cls,L"part") && U::scmp(cls,L"output-document-class") &&
					U::scmp(cls,L"annotation") && U::scmp(cls,L"title") && U::scmp(cls,L"epigraph") &&
					U::scmp(cls,L"poem") && U::scmp(cls,L"stanza") && U::scmp(cls,L"cite") &&
					U::scmp(cls,L"date") &&
					U::scmp(cls,L"history") && U::scmp(cls,L"image")&&
					U::scmp(cls,L"code") &&
					U::scmp(id,L"fbw_desc") && U::scmp(id,L"fbw_body") && U::scmp(id,L"fbw_updater")))
					goto ok;
			}

			CElementDescriptor* ED;
			if(_EDMnr.GetElementDescriptor(cur, &ED))
				goto ok;
			MSHTML::IHTMLDOMNodePtr ce(cur->previousSibling);
			cur->removeNode(VARIANT_FALSE);
			if (ce)
				next=ce->nextSibling;
			else
				next=node->firstChild;
		}
ok:

		cur=next;
	}
	return fRet;
}

// move the paragraph up one level
void MoveUp(bool fCopyFmt,MSHTML::IHTMLDOMNodePtr& node) {
	MSHTML::IHTMLDOMNodePtr   parent(node->parentNode);
	MSHTML::IHTMLElement2Ptr  elem(parent);

	// clone parent (it can be A/EM/STRONG/SPAN)
	if (fCopyFmt) {
		MSHTML::IHTMLDOMNodePtr   clone(parent->cloneNode(VARIANT_FALSE));
		while ((bool)node->firstChild)
			clone->appendChild(node->firstChild);
		node->appendChild(clone);
	}

	// clone parent once more and move siblings after node to it
	if ((bool)node->nextSibling) {
		MSHTML::IHTMLDOMNodePtr   clone(parent->cloneNode(VARIANT_FALSE));
		while ((bool)node->nextSibling)
			clone->appendChild(node->nextSibling);
		elem->insertAdjacentElement(L"afterEnd",MSHTML::IHTMLElementPtr(clone));
		if (U::scmp(parent->nodeName,L"P")==0)
			MSHTML::IHTMLElement3Ptr(clone)->inflateBlock=VARIANT_TRUE;
	}

	// now move node to parent level, the tree may be in some weird state
	node->removeNode(VARIANT_TRUE); // delete from tree
	node=elem->insertAdjacentElement(L"afterEnd",MSHTML::IHTMLElementPtr(node));
}

void BubbleUp(MSHTML::IHTMLDOMNode *node,const wchar_t *name) {
	MSHTML::IHTMLElement2Ptr	    elem(node);
	MSHTML::IHTMLElementCollectionPtr elements(elem->getElementsByTagName(name));
	long				    len=elements->length;
	for (long i=0;i<len;++i) {
		MSHTML::IHTMLDOMNodePtr	  ce(elements->item(i));
		if (!(bool)ce)
			break;
		for (int ll=0;ce->parentNode!=node && ll<30;++ll)
			MoveUp(true,ce);
		MoveUp(false,ce);
	}
}

#if (1)
// split paragraphs containing BR elements
static void   SplitBRs(MSHTML::IHTMLElement2Ptr elem) 
{
	CString text = MSHTML::IHTMLElementPtr(elem)->outerHTML;
	if (text.Replace(L"<BR>", L"</P><P>") > 0)
		MSHTML::IHTMLElementPtr(elem)->outerHTML = text.AllocSysString();
}
#else
static void   SplitBRs(MSHTML::IHTMLElement2Ptr elem) {
	MSHTML::IHTMLElementCollectionPtr BRs(elem->getElementsByTagName(L"BR"));
	while (BRs->length>0) {
		MSHTML::IHTMLDOMNodePtr	  ce(BRs->item(0L));
		if (!(bool)ce)
			break;
		for (;;) {
			MSHTML::IHTMLDOMNodePtr	parent(ce->parentNode);
			if (!(bool)parent) // no parent? huh?
				goto blowit;
			_bstr_t	  name(parent->nodeName);
			if (U::scmp(name,L"P")==0 || U::scmp(name,L"DIV")==0)
				break;
			if (U::scmp(name,L"BODY")==0)
				goto blowit;
			MoveUp(false,ce);
		}
		MoveUp(false,ce);
blowit:
		ce->removeNode(VARIANT_TRUE);
	}
}
#endif

// this sub should locate any nested paragraphs and bubble them up
static void RelocateParagraphs(MSHTML::IHTMLDOMNode *node) {
	if (node->nodeType!=1)
		return;
	// Native tables have a deliberately different content model: paragraphs
	// belong to cells and must never be bubbled out during normalization.
	_bstr_t nodeName(node->nodeName);
	if (U::scmp(nodeName, L"TABLE") == 0 || U::scmp(nodeName, L"TBODY") == 0 ||
		U::scmp(nodeName, L"TR") == 0 || U::scmp(nodeName, L"TD") == 0 || U::scmp(nodeName, L"TH") == 0)
		return;

	MSHTML::IHTMLDOMNodePtr   cur(node->firstChild);
	while (cur) {
		if (cur->nodeType==1) {
			if (!U::scmp(cur->nodeName,L"P")) {
				BubbleUp(cur,L"P");
				BubbleUp(cur,L"DIV");
			} else
				RelocateParagraphs(cur);
		}
		cur=cur->nextSibling;
	}
}

static bool IsStanza(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"stanza")==0;
}

// Move text content in DIV items to P elements.  Native TABLE is also a
// structural DIV child and therefore stays outside automatically created P.
static void PackText(MSHTML::IHTMLElement2Ptr elem, MSHTML::IHTMLDocument2* doc)
{
	MSHTML::IHTMLElementCollectionPtr elements(elem->getElementsByTagName(L"DIV"));
	for(long i = 0; i < elements->length; ++i)
	{
		MSHTML::IHTMLDOMNodePtr div(elements->item(i));
		if(U::scmp(MSHTML::IHTMLElementPtr(div)->className, L"image") == 0)
			continue;
		MSHTML::IHTMLDOMNodePtr cur(div->firstChild);
		while((bool)cur)
		{
			_bstr_t cur_name(cur->nodeName);
			if (U::scmp(cur_name, L"P") && U::scmp(cur_name, L"DIV") && !IsNativeTableBlockName(cur_name))
			{
				// create a paragraph from a run of non-structural nodes
				MSHTML::IHTMLElementPtr newp(doc->createElement(L"P"));
				MSHTML::IHTMLDOMNodePtr newn(newp);
				cur->replaceNode(newn);
				newn->appendChild(cur);
				while ((bool)newn->nextSibling)
				{
					cur_name = newn->nextSibling->nodeName;
					if (U::scmp(cur_name, L"P") == 0 || U::scmp(cur_name, L"DIV") == 0 || IsNativeTableBlockName(cur_name))
						break;
					newn->appendChild(newn->nextSibling);
				}
				cur = newn->nextSibling;
			}
			else
				cur = cur->nextSibling;
		}
	}
}

static void FixupLinks(MSHTML::IHTMLDOMNode *dom) {
	MSHTML::IHTMLElement2Ptr  elem(dom);

	if (!(bool)elem)
		return;

	MSHTML::IHTMLElementCollectionPtr coll(elem->getElementsByTagName(L"a"));
	if (!(bool)coll)
		return;

	if (coll->length == 0) coll = elem->getElementsByTagName(L"A");

	for (long l=0;l<coll->length;++l) {
		MSHTML::IHTMLElementPtr a(coll->item(l));
		if (!(bool)a)
			continue;

		_variant_t	  href(a->getAttribute(L"href",2));
		if (V_VT(&href)==VT_BSTR && V_BSTR(&href) &&
			::SysStringLen(V_BSTR(&href))>11 &&
			wcsncmp(V_BSTR(&href), L"file://", 7)==0)
		{
			wchar_t* pos = wcschr((wchar_t*)V_BSTR(&href), L'#'); 
			if(!pos)
				continue;
			a->setAttribute(L"href",pos,0);
		}
	}
}

bool CFBEView::InsertPoem(bool fCheck)
{
	try
	{
		MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
		if(!(bool)rng)
			return false;

		MSHTML::IHTMLElementPtr pe(GetHP(rng->parentElement()));
		if(!(bool)pe)
			return false;

		// Get parents for start and end ranges and ensure they are the same as pe
		MSHTML::IHTMLTxtRangePtr tr(rng->duplicate());
		tr->collapse(VARIANT_TRUE);
		if (GetHP(tr->parentElement()) != pe)
			return false;

		// Check if it possible to insert a poem there
		_bstr_t cls(pe->className);
		if(U::scmp(cls, L"section")
			&& U::scmp(cls, L"epigraph")
			&& U::scmp(cls, L"annotation")
			&& U::scmp(cls, L"history")
			&& U::scmp(cls, L"cite"))
			return false;

		// Preventing double expanding whether checked or actual executed
		MSHTML::IHTMLElementPtr elBegin, elEnd;
		MSHTML::IHTMLDOMNodePtr begin, end;
		if(!ExpandTxtRangeToParagraphs(rng, elBegin, elEnd))
			return false;
		else
		{
			begin = elBegin;
			end = elEnd;
		}

		// All checks passed
		if(fCheck)
			return true;

		m_mk_srv->BeginUndoUnit(L"insert poem");

		CString rngHTML;
		MSHTML::IHTMLDOMNodePtr sibling = begin;
		do
		{
			rngHTML += MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR();
			if(sibling == end)
				break;
		}
		while((sibling = sibling->nextSibling));

		MSHTML::IHTMLElementPtr ne(Document()->createElement(L"<DIV class=poem>"));

		if(!U::scmp(rng->text.GetBSTR(), L""))
		{
			MSHTML::IHTMLElementPtr se(Document()->createElement(L"<DIV class=stanza>"));
			se->innerHTML = L"<P>&nbsp;</P>";
			ne->innerHTML = se->outerHTML;
		}
		else
		{
			MSHTML::IHTMLElementPtr acc(Document()->createElement(L"DIV"));
			acc->innerHTML = rngHTML.AllocSysString();

			MSHTML::IHTMLElementCollectionPtr coll = acc->children;
			bool trim = true;

			CString stanzaHTML;
			for(int i = 0; i < coll->length; ++i)
			{
				MSHTML::IHTMLElementPtr curr = coll->item(i);

				CString line = curr->innerText;
				// changed by SeNS: issue #61
				if (line.Trim().IsEmpty())
				{
					if(trim)
						continue;
					else
					{
						MSHTML::IHTMLElementPtr se(Document()->createElement(L"<DIV class=stanza>"));
						se->innerHTML = stanzaHTML.AllocSysString();
						MSHTML::IHTMLElement2Ptr(ne)->insertAdjacentElement(L"beforeEnd", se);
						stanzaHTML.Empty();
						trim = true;
					}
				}
				else
				{
					if(!U::scmp(curr->tagName, L"DIV"))
					{
						if(curr->innerText.GetBSTR())
						{
							stanzaHTML += CString(L"<P>") + curr->innerText.GetBSTR() + CString(L"</P>");
						}
						else
							continue;
					}
					else
					{
						stanzaHTML += curr->outerHTML.GetBSTR();
					}

					trim = false;
				}
			}

			if(!stanzaHTML.IsEmpty())
			{
				MSHTML::IHTMLElementPtr se(Document()->createElement(L"<DIV class=stanza>"));
				se->innerHTML = stanzaHTML.AllocSysString();
				MSHTML::IHTMLElement2Ptr(ne)->insertAdjacentElement(L"beforeEnd", se);
			}
		}


		MSHTML::IHTMLDOMNodePtr(pe)->insertBefore((MSHTML::IHTMLDOMNodePtr)ne, begin.GetInterfacePtr());

		while(begin != end)
		{
			sibling = begin->nextSibling;
			begin->removeNode(VARIANT_TRUE);
			begin = sibling;
		}
		end->removeNode(VARIANT_TRUE);

		FixupParagraphs(pe);
		PackText(pe, Document());

		rng->moveToElementText(ne);
		rng->collapse(VARIANT_FALSE);
		rng->select();

		m_mk_srv->EndUndoUnit();
	}
	catch (_com_error& err) 
	{
		U::ReportError(err);
	}
	return true;
}

bool CFBEView::InsertCite(bool fCheck)
{
	try
	{
		MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
		if(!(bool)rng)
			return false;

		MSHTML::IHTMLElementPtr pe(GetHP(rng->parentElement()));
		if(!(bool)pe)
			return false;

		// Get parents for start and end ranges and ensure they are the same as pe
		MSHTML::IHTMLTxtRangePtr tr(rng->duplicate());
		tr->collapse(VARIANT_TRUE);
		if (GetHP(tr->parentElement()) != pe)
			return false;

		// Check if it possible to insert a cite there
		_bstr_t cls(pe->className);
		if(U::scmp(cls, L"section")
			&& U::scmp(cls, L"epigraph")
			&& U::scmp(cls, L"annotation")
			&&  U::scmp(cls, L"history"))
			return false;

		// Preventing double expanding whether checked or actual executed
		MSHTML::IHTMLElementPtr elBegin, elEnd;
		MSHTML::IHTMLDOMNodePtr begin, end;
		if(!ExpandTxtRangeToParagraphs(rng, elBegin, elEnd))
			return false;
		else
		{
			begin = elBegin;
			end = elEnd;
		}

		// All checks passed
		if(fCheck)
			return true;

		m_mk_srv->BeginUndoUnit(L"insert cite");

		CString rngHTML;
		MSHTML::IHTMLDOMNodePtr sibling = begin;
		do
		{
			rngHTML += MSHTML::IHTMLElementPtr(sibling)->outerHTML.GetBSTR();
			if(sibling == end)
				break;
		}
		while((sibling = sibling->nextSibling));

		// Create cite
		MSHTML::IHTMLElementPtr ne(Document()->createElement(L"DIV"));
		ne->className = L"cite";

		MSHTML::IHTMLElementPtr acc(Document()->createElement(L"DIV"));
		acc->innerHTML = rngHTML.AllocSysString();

		MSHTML::IHTMLElementCollectionPtr coll = acc->children;

		CString citeHTML;
		for(int i = 0; i < coll->length; ++i)
		{
			MSHTML::IHTMLElementPtr curr = coll->item(i);
			if(!U::scmp(curr->tagName, L"DIV")
				&& U::scmp(curr->className, L"table")
				&& U::scmp(curr->className, L"poem"))
			{
				if(curr->innerText.GetBSTR())
				{
					citeHTML += CString(L"<P>") + curr->innerText.GetBSTR() + CString(L"</P>");
				}
				else
					continue;
			}
			else
			{
				citeHTML += curr->outerHTML.GetBSTR();
			}
		}

		ne->innerHTML = citeHTML.AllocSysString();

		MSHTML::IHTMLDOMNodePtr(pe)->insertBefore((MSHTML::IHTMLDOMNodePtr)ne, begin.GetInterfacePtr());

		while(begin != end)
		{
			sibling = begin->nextSibling;
			begin->removeNode(VARIANT_TRUE);
			begin = sibling;
		}
		end->removeNode(VARIANT_TRUE);

		FixupParagraphs(pe);
		PackText(pe, Document());

		rng->moveToElementText(ne);
		rng->collapse(VARIANT_FALSE);
		rng->select();

		m_mk_srv->EndUndoUnit();
	}
	catch (_com_error& err) 
	{
		U::ReportError(err);
	}
	return true;
}

CString CFBEView::GetClearedRangeText(const MSHTML::IHTMLTxtRangePtr &rng)const
{
	CString org_text = rng->htmlText;
	
	org_text.Replace(L"\r\n", L"\n");
	org_text.Replace(L" \n", L" ");
	org_text.Replace(L"\n ", L" ");
	org_text.Replace(L"\n", L" ");

	while(org_text[org_text.GetLength() - 1] == L' ')
		org_text = org_text.Left(org_text.GetLength() - 1);
	while(org_text[0] == L' ')
		org_text = org_text.Right(org_text.GetLength() - 1);
	org_text.Replace(L"> <", L">\r\n<");	
	return org_text;
}

// searching
void  CFBEView::StartIncSearch() {
  try {
    m_is_start=Document()->selection->createRange();
    m_is_start->collapse(VARIANT_TRUE);
  }
  catch (_com_error&) {
  }
}

void  CFBEView::CancelIncSearch() {
  if (m_is_start) {
    m_is_start->raw_select();
    m_is_start.Release();
  }
}

// script calls
void	      CFBEView::ImgSetURL(IDispatch *elem,const CString& url) {
  try {
    CComDispatchDriver	dd(Script());
    _variant_t	  ve(elem);
    _variant_t	  vu((const TCHAR *)url);
    dd.Invoke2(L"ImgSetURL",&ve,&vu);
  }
  catch (_com_error&) { }
}

IDispatchPtr  CFBEView::Call(const wchar_t *name) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  ret;
    _variant_t  vt2((false));
    dd.Invoke1(name,&vt2,&ret);
    if (V_VT(&ret)==VT_DISPATCH)
      return V_DISPATCH(&ret);
  }
  catch (_com_error&) { }
  return IDispatchPtr();
}
IDispatchPtr  CFBEView::Call(const wchar_t *name,IDispatch *pDisp) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt;
    if (pDisp)
      vt=pDisp;
    _variant_t  vt2(false);
    _variant_t  ret;
    dd.Invoke2(name,&vt,&vt2,&ret);
    if (V_VT(&ret)==VT_DISPATCH)
      return V_DISPATCH(&ret);
  }
  catch (_com_error&) { }
  return IDispatchPtr();
}
static bool vt2bool(const _variant_t& vt) {
  if (V_VT(&vt)==VT_DISPATCH)
    return V_DISPATCH(&vt)!=0;
  if (V_VT(&vt)==VT_BOOL)
    return V_BOOL(&vt)==VARIANT_TRUE;
  if (V_VT(&vt)==VT_I4)
    return V_I4(&vt)!=0;
  if (V_VT(&vt)==VT_UI4)
    return V_UI4(&vt)!=0;
  return false;
}

bool CFBEView::bCall(const wchar_t *name, int nParams, VARIANT* params)
{
	try
	{
		CComDispatchDriver dd(Script());
		_variant_t ret;
		dd.InvokeN(name, params, nParams, &ret);
		return vt2bool(ret);
	}
	catch(_com_error& err)
	{
		U::ReportError(err);
	}

	return false;
}

bool  CFBEView::bCall(const wchar_t *name,IDispatch *pDisp) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt;
    if (pDisp)
      vt=pDisp;
    _variant_t  vt2(true);
    _variant_t  ret;
    dd.Invoke2(name,&vt,&vt2,&ret);
    return vt2bool(ret);
  }
  catch (_com_error&) { }
  return false;
}

bool  CFBEView::bCall(const wchar_t *name) {
  try {
    CComDispatchDriver  dd(Script());
    _variant_t  vt2(true);
    _variant_t  ret;
    dd.Invoke1(name,&vt2,&ret);
    return vt2bool(ret);
  }
  catch (_com_error&) { }
  return false;
}

// utilities
static CString	GetPath(MSHTML::IHTMLElementPtr elem) {
  try {
    if (!(bool)elem)
      return CString();
    CString		      path;
    while (elem) {
      CString	  cur((const wchar_t *)elem->tagName);
	  CString	  cid((const wchar_t *)elem->id);
      if (cur==_T("BODY"))
        return path;
	  if(cid == _T("fbw_body"))
		  return path;
      _bstr_t	  cls(elem->className);
      if (cls.length()>0)
	cur=(const wchar_t *)cls;
      _bstr_t	  id(elem->id);
      if (id.length()>0) {
	cur+=_T(':');
	cur+=(const wchar_t *)id;
      }
      if (!path.IsEmpty())
	path=_T('/')+path;
      path=cur+path;
      elem=elem->parentElement;
    }
    return path;
  }
  catch (_com_error&) { }
  return CString();
}

CString	CFBEView::SelPath() {
  return GetPath(SelectionContainer());
}

void  CFBEView::GoTo(MSHTML::IHTMLElement *e,bool fScroll) {
  if (!e)
    return;

  if (fScroll)
    e->scrollIntoView(VARIANT_TRUE);

  MSHTML::IHTMLTxtRangePtr	r(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
  r->moveToElementText(e);
  r->collapse(VARIANT_TRUE);
  // all m$ editors like to position the pointer at the end of the preceding element,
  // which sucks. This workaround seems to work most of the time.
  if (e!=r->parentElement() && r->move(L"character",1)==1)
    r->move(L"character",-1);

  r->select();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionContainerImp()
{
	// MSHTML can briefly expose a Control selection while it is moving focus to
	// its scrollbar.  The ControlRange parent helper fails with E_FAIL on older MSHTML
	// versions in that state.  SelectionContainer is only used to synchronize
	// UI (not to modify the document), so resolve the selected control directly
	// and leave a diagnostic record if the transient query is unavailable.
	IDispatchPtr selrange;
	try
	{
		selrange = Document()->selection->createRange();
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLSelectionObject::createRange", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}

	MSHTML::IHTMLTxtRangePtr range;
	try
	{
		range = selrange;
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"QueryInterface(IHTMLTxtRange)", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}
	if (range)
	{
		try { return range->parentElement(); }
		catch (_com_error& err)
		{
			TraceSelectionContainerFailure(L"IHTMLTxtRange::parentElement", err.Error(), L"Text");
			return MSHTML::IHTMLElementPtr();
		}
	}

	MSHTML::IHTMLControlRangePtr controls;
	try { controls = selrange; }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"QueryInterface(IHTMLControlRange)", err.Error(), L"unknown");
		return MSHTML::IHTMLElementPtr();
	}
	if (!controls)
		return MSHTML::IHTMLElementPtr();

	long length = 0;
	try { length = controls->length; }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLControlRange::get_length", err.Error(), L"Control");
		return MSHTML::IHTMLElementPtr();
	}
	if (length <= 0)
		return MSHTML::IHTMLElementPtr();

	MSHTML::IHTMLElementPtr selected;
	try { selected = controls->item(0); }
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLControlRange::item(0)", err.Error(), L"Control", length);
		return MSHTML::IHTMLElementPtr();
	}
	if (!selected)
		return MSHTML::IHTMLElementPtr();

	try
	{
		// FBE selects the DIV/SPAN.image wrapper for a clicked illustration.
		// Keep that semantic container for the document tree.  Be defensive for
		// a raw IMG control selection created by another MSHTML code path.
		if (U::scmp(selected->tagName, L"IMG") == 0)
		{
			MSHTML::IHTMLElementPtr parent(selected->parentElement);
			if (parent && U::scmp(parent->className, L"image") == 0)
				return parent;
		}
		return selected;
	}
	catch (_com_error& err)
	{
		TraceSelectionContainerFailure(L"IHTMLElement::tagName/parentElement", err.Error(), L"Control", length);
	}

	return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionAnchor() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      _bstr_t	tn(cur->tagName);
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"DIV")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
	  // Added by SeNS - inline images
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"SPAN")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) { }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionAnchor(MSHTML::IHTMLElementPtr cur) {
  try {
    while (cur) {
      _bstr_t	tn(cur->tagName);
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"DIV")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
	  // Added by SeNS - inline images
      if (U::scmp(tn,L"A")==0 || (U::scmp(tn,L"SPAN")==0 && U::scmp(cur->className,L"image")==0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) { }
  return MSHTML::IHTMLElementPtr();
}


MSHTML::IHTMLElementPtr CFBEView::SelectionStructCon() {
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		while (cur)
		{
			if (U::scmp(cur->tagName, L"P") == 0 || U::scmp(cur->tagName, L"DIV") == 0)
				return cur;
			cur=cur->parentElement;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionStructNearestCon()
{
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		if(cur)
		{			
			return cur;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr CFBEView::SelectionStructCode() {
	try
	{
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		while(cur)
		{
			// changed by SeNS: inline images also have a tag SPAN
			if((U::scmp(cur->tagName, L"SPAN") == 0) && (U::scmp(cur->className,L"image")!=0))
				return cur;
			cur = cur->parentElement;
		}		
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return MSHTML::IHTMLElementPtr();
}

// Modification by Pilgrim
MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructSection() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"section")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructImage() {	
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  // changed by SeNS: inline images have a tag SPAN, regular tag DIV
      if ((U::scmp(cur->className,L"image")==0) && (U::scmp(cur->tagName,L"SPAN")!=0))
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}


MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructTable() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"table")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionStructTableCon() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
	return cur;
      cur=cur->parentElement;
    }
	MSHTML::IHTMLTxtRangePtr range(Document()->selection->createRange());
	if (range) {
		range->collapse(VARIANT_TRUE);
		return FindTableCell(range->parentElement());
	}
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleT() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
 	 _bstr_t	style(AU::GetAttrB(cur,L"fbstyle"));
      if (U::scmp(cur->className,L"table")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleTB(_bstr_t& style) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	 if (U::scmp(cur->className,L"table")==0){		
		 style = AU::GetAttrB(cur,L"fbstyle");
		 return cur;
	 }	
     cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyle() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
 	 _bstr_t	style(AU::GetAttrB(cur,L"fbstyle"));
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsStyleB(_bstr_t& style) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	 if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){		
		 style = AU::GetAttrB(cur,L"fbstyle");
		 return cur;
	 }	
     cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsColspan() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsColspanB(_bstr_t& colspan) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
	  	colspan = AU::GetAttrB(cur,L"fbcolspan");
		return cur;
	  }
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsRowspan() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsRowspanB(_bstr_t& rowspan) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
	  if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
		rowspan =  AU::GetAttrB(cur,L"fbrowspan");
		return cur;
	  }
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignTR() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"tr")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignTRB(_bstr_t& align) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"tr")==0){
			align =  AU::GetAttrB(cur,L"fbalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlign() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"tr")==0 || U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsAlignB(_bstr_t& align) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"tr")==0 || U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
			align =  AU::GetAttrB(cur,L"fbalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsVAlign() {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
      if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0)
		return cur;
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

MSHTML::IHTMLElementPtr	  CFBEView::SelectionsVAlignB(_bstr_t& valign) {
  try {
    MSHTML::IHTMLElementPtr   cur(SelectionContainer());
    while (cur) {
		if (U::scmp(cur->className,L"th")==0 || U::scmp(cur->className,L"td")==0){
			valign =  AU::GetAttrB(cur,L"fbvalign");
			return cur;
		}
      cur=cur->parentElement;
    }
  }
  catch (_com_error&) {
  }
  return MSHTML::IHTMLElementPtr();
}

void  CFBEView::Normalize(MSHTML::IHTMLDOMNodePtr dom) {
  try {
	//MSHTML::IHTMLElementCollectionPtr col = dom->childNodes;
	MSHTML::IHTMLDOMNodePtr el = dom->firstChild;
	bool found = false;

	// ������������� ����� ������ body ���������
	while(el)
	{
		MSHTML::IHTMLElementPtr hel(el);

		if(U::scmp(hel->id, L"fbw_body") == 0)
		{
			found = true;
			break;
		}
		el = el->nextSibling;
	}

	if(!found)
	{
		return;
	}

    // wrap in an undo unit
    m_mk_srv->BeginUndoUnit(L"Normalize");

    // remove unsupported elements
	RemoveUnk(el,Document());

	MergeEqualHTMLElements(el, Document());
    // get rid of nested DIVs and Ps
    RelocateParagraphs(el);
    // delete empty nodes
    
	RemoveEmptyNodes(el);
    // make sure text appears under Ps only
    PackText(el,Document());
    // get rid of nested Ps once more
    RelocateParagraphs(el);
    // convert BRs to separate paragraphs
    SplitBRs(el);
    // delete empty nodes again
    RemoveEmptyNodes(el);
    // fixup links
    FixupLinks(el);

    m_mk_srv->EndUndoUnit();
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }
}

static void FixupParagraphs(MSHTML::IHTMLElement2Ptr elem)
{
	MSHTML::IHTMLElementCollectionPtr pp(elem->getElementsByTagName(L"P"));
	for(long l = 0; l < pp->length; ++l)
		MSHTML::IHTMLElement3Ptr(pp->item(l))->inflateBlock = VARIANT_TRUE;
}

LRESULT CFBEView::OnPaste(WORD, WORD, HWND, BOOL&)
{
	try
	{
		m_mk_srv->BeginUndoUnit(L"Paste");
		++m_enable_paste;
		
		// added by SeNS: process clipboard and change nbsp
		if (OpenClipboard())
		{
			// process text
			if ( IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_UNICODETEXT))
			{
				if (_Settings.GetNBSPChar().Compare(L"\u00A0") != 0)
				{
					HANDLE hData = GetClipboardData( CF_UNICODETEXT );
					TCHAR *buffer = (TCHAR*)GlobalLock( hData );
					CString fromClipboard(buffer);
					GlobalUnlock( hData );

					fromClipboard.Replace( L"\u00A0", _Settings.GetNBSPChar());

					HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, (fromClipboard.GetLength()+1)*sizeof(TCHAR));
					buffer = (TCHAR*)GlobalLock(clipbuffer);
					wcscpy(buffer, fromClipboard);
					GlobalUnlock( clipbuffer );
					SetClipboardData(CF_UNICODETEXT, clipbuffer);
				}
			}
			// process bitmaps from clipboard
			else if ( IsClipboardFormatAvailable(CF_BITMAP))
			{
				HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
				TCHAR szPathName[MAX_PATH] = { 0 };
				TCHAR szFileName[MAX_PATH] = { 0 };
				if (::GetTempPath(sizeof(szPathName)/sizeof(TCHAR), szPathName))
					if (::GetTempFileName(szPathName, L"img", ::GetTickCount(), szFileName))
					{
						int quality = _Settings.GetJpegQuality();

						CString fileName(szFileName);
						CImage image; 
						image.Attach(hBitmap); 

						if (_Settings.GetImageType() == 0)
						{
							fileName.Replace(L".tmp", L".png");
							image.Save(fileName, Gdiplus::ImageFormatPNG);
						}
						else
						{
							fileName.Replace(L".tmp", L".jpg");
							// set encoder quality
							Gdiplus::EncoderParameters encoderParameters[1];
							encoderParameters[0].Count = 1;
							encoderParameters[0].Parameter[0].Guid = Gdiplus::EncoderQuality;
							encoderParameters[0].Parameter[0].NumberOfValues = 1;
							encoderParameters[0].Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
							encoderParameters[0].Parameter[0].Value = &quality;
							image.Save(fileName, Gdiplus::ImageFormatJPEG, &encoderParameters[0]);
						}

						AddImage(fileName, true);
						::DeleteFile(fileName);
					}
			}
			CloseClipboard();
		}

		IOleCommandTargetPtr(m_browser)->Exec(&CGID_MSHTML, IDM_PASTE, 0, NULL, NULL);
		--m_enable_paste;
		if(m_normalize)
			Normalize(Document()->body);
		m_mk_srv->EndUndoUnit();
	}
	catch(_com_error& err)
	{
		U::ReportError(err);
	}

	return 0;
}

// searching
bool CFBEView::DoSearch(bool fMore)
{
	if(m_fo.pattern.IsEmpty())
	{
		if(m_is_start)
			m_is_start->raw_select();
		return true;
	}

	NormalizeSearchPatternNbsp(m_fo.pattern);

	return m_fo.fRegexp ? DoSearchRegexp(fMore) : DoSearchStd(fMore);
}

// Removes HTML tags
void RemoveTags(CString &src)
{
	int openTag = 0, closeTag=0;
	while (openTag != -1)
	{
		openTag = src.Find(L"<", 0);
		closeTag = src.Find(L">", openTag+1);
		if (openTag != -1 && closeTag > openTag)
			src.Delete(openTag, closeTag-openTag+1);
	}
}

// Returns text offset including inline images (treated as a 3 chars each)
int CFBEView::TextOffset(MSHTML::IHTMLTxtRange *rng, AU::ReMatch rm, CString txt, CString htmlTxt)
{
	CString text(txt);
	CString match = rm->Value;
	// special fix for "Words" dialog
	match = match.TrimRight(10);
	match = match.TrimRight(13);
	int num = 0, pos = 1;
	if (text.IsEmpty()) text.SetString(rng->text);
	while (num < text.GetLength())
	{
		num = text.Find (match, num);
		if ((num == rm->FirstIndex) || (num == -1)) break;
		num += 1;
		pos++;
	}
	CString html(htmlTxt);
	if (html.IsEmpty()) html.SetString(rng->htmlText);

	// change <IMG to "afro-american" O<IMG LOL
	html.Replace (L"<IMG", L"O<IMG");
	RemoveTags(html);

	num = 0;
	for (int i=0; i<pos; i++)
	{
		num = html.Find (match, num);
		num += 1;
	}
	// find number of inline images occurences
	html = html.Left(num);
	pos = num = 0;
	while (num < html.GetLength())
	{
		num = html.Find(L"O", num);
		if (num == -1) break;
		num += 1;
		pos++;
	}
	return pos*3;
}

static void MoveRangeToRegexMatch(MSHTML::IHTMLTxtRange* range, AU::ReMatch match, int offset)
{
	range->collapse(VARIANT_TRUE);
	range->move(L"character", match->FirstIndex + offset);
	if(range->moveStart(L"character", 1) == 1)
		range->move(L"character", -1);
	range->moveEnd(L"character", match->Length);
}

void CFBEView::SelMatch(MSHTML::IHTMLTxtRange *tr,AU::ReMatch rm) 
{
	// SeNS: fix for issue #147
	int numImages = TextOffset (tr, rm);

	MoveRangeToRegexMatch(tr, rm, numImages);
	// set focus to editor if selection empty
	if (!rm->Length)
		SetFocus();
	tr->select();
	m_fo.ClearMatch();
	m_fo.match = new AU::IMatch2(*rm);
	m_fo.hasMatch = true;
}

bool CFBEView::DoSearchRegexp(bool fMore)
{
	try
	{
		m_fo.ClearMatch();

		// well, try to compile it first
		ScopedSearchRegExp re;
		InitSearchRegExp(re.get(), m_fo.flags, m_fo.pattern);

		// locate starting paragraph
		MSHTML::IHTMLTxtRangePtr sel(Document()->selection->createRange());
		if(!fMore && (bool)m_is_start)
			sel = m_is_start->duplicate();
		if(!(bool)sel)
			return false;

		MSHTML::IHTMLElementPtr sc(SelectionStructCon());
		long s_idx = 0;
		long s_off1 = 0;
		long s_off2 = 0;
		if((bool)sc)
		{
			s_idx = sc->sourceIndex;
			if(IsParagraphElement(sc))
			{
				s_off2 = sel->text.length();
				MSHTML::IHTMLTxtRangePtr pr(sel->duplicate());
				pr->moveToElementText(sc);
				pr->setEndPoint(L"EndToStart", sel);
				s_off1 = pr->text.length();
				s_off2 += s_off1;
			}
		}

		// walk the all collection now, looking for the next P
		MSHTML::IHTMLElementCollectionPtr all(Document()->all);
		long all_len = all->length;
		long incr = m_fo.flags & FRF_REVERSE ? -1 : 1;
		bool fWrapped = false;

		// * search in starting element
		if(IsParagraphElement(sc))
		{
			sel->moveToElementText(sc);
			CString selText = sel->text;
			AU::ReMatches rm(ExecuteSearchRegExp(re.get(), sel));

			// changed by SeNS: fix for issue #62
			if(rm->Count > 0 && !(selText.IsEmpty() && fMore))
			{
				if(incr > 0)
				{
					for(long l = m_startMatch; l < rm->Count; ++l)
					{
						AU::ReMatch crm(rm->Item[l]);
						if(crm->FirstIndex >= s_off2)
						{
							m_startMatch = l + 1;
							SelMatch(sel, crm);
							return true;
						}
					}
				}
				else
				{
					for(long l = m_endMatch; l >= 0; --l)
					{
						AU::ReMatch crm(rm->Item[l]);
						if(crm->FirstIndex < s_off1)
						{
							SelMatch(sel, crm);
							m_endMatch = l - 1;
							return true;
						}
					}
				}
			}
		}

		// search all others
		for(long cur = s_idx + incr; ; cur += incr)
		{
			// adjust out of bounds indices
			if(cur < 0)
			{
				cur = all_len - 1;
				fWrapped = true;
			}
			else if(cur >= all_len)
			{
				cur = 0;
				fWrapped = true;
			}

			// check for wraparound
			if(cur == s_idx)
				break;

			MSHTML::IHTMLElementPtr elem(all->item(cur));
			if(!IsParagraphElement(elem))
				continue;

			sel->moveToElementText(elem);
			AU::ReMatches rm(ExecuteSearchRegExp(re.get(), sel));
			if(rm->Count <= 0)
				continue;
			if(incr > 0)
			{
				SelMatch(sel, rm->Item[0]);
				m_startMatch = 1;
			}
			else
			{
				SelMatch(sel, rm->Item[rm->Count - 1]);
				m_endMatch = rm->Count - 2;
			}
			NotifyWrappedSearch(fWrapped);
			return true;
		}

		// search again in starting element
		if(IsParagraphElement(sc))
		{
			sel->moveToElementText(sc);
			AU::ReMatches rm(ExecuteSearchRegExp(re.get(), sel));
			if(rm->Count > 0)
			{
				if(incr > 0)
				{
					for(long l = 0; l < rm->Count; ++l)
					{
						AU::ReMatch crm(rm->Item[l]);
						if(crm->FirstIndex < s_off1)
						{
							SelMatch(sel, crm);
							NotifyWrappedSearch(fWrapped);
							return true;
						}
					}
				}
				else
				{
					for(long l = rm->Count - 1; l >= 0; --l)
					{
						AU::ReMatch crm(rm->Item[l]);
						if(crm->FirstIndex >= s_off2)
						{
							SelMatch(sel, crm);
							NotifyWrappedSearch(fWrapped);
							return true;
						}
					}
				}
			}
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return false;
}

bool CFBEView::DoSearchStd(bool fMore)
{
	try
	{
		m_fo.ClearMatch();

		// fetch selection
		MSHTML::IHTMLTxtRangePtr sel(Document()->selection->createRange());
		if(!fMore && (bool)m_is_start)
			sel = m_is_start->duplicate();
		if(!(bool)sel)
			return false;
		
		MSHTML::IHTMLTxtRangePtr org(sel->duplicate());
		// check if it is collapsed
		if(sel->compareEndPoints(L"StartToEnd", sel) != 0)
		{
			// collapse and advance
			if(m_fo.flags & FRF_REVERSE)
				sel->collapse(VARIANT_TRUE);
			else
				sel->collapse(VARIANT_FALSE);
		}

		// search for text
		if(sel->findText((const wchar_t*)m_fo.pattern, 1073741824, m_fo.flags) == VARIANT_TRUE)
		{
			// ok, found
			sel->select();
			return true;
		}

		// not found, try searching from start to sel
		sel = MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange();
		sel->collapse(m_fo.flags & 1 ? VARIANT_FALSE : VARIANT_TRUE);
		if(sel->findText((const wchar_t*)m_fo.pattern, 1073741824, m_fo.flags) == VARIANT_TRUE
			&& org->compareEndPoints("StartToStart", sel)*(m_fo.flags & 1 ? -1 : 1) > 0)
		{
			// found
			sel->select();
			MessageBeep(MB_ICONASTERISK);
			return true;
		}
	}
	catch (_com_error&)
	{
		//U::ReportError(err);
	}

	return false;
}

static CString GetSM(AU::ReSubMatches sm, int idx)
{
	if(!sm)
		return CString();

	if(idx < 0 || idx >= sm->Count)
		return CString();

	_variant_t vt(sm->Item[idx]);

	if(V_VT(&vt) == VT_BSTR)
		return V_BSTR(&vt);

	return CString();
}

struct RR
{
	enum
	{
		STRONG = 1,
		EMPHASIS = 2,
		UPPER = 4,
		LOWER = 8,
		TITLE = 16
	};

	int flags;
	int start;
	int len;
};

typedef CSimpleValArray<RR> RRList;

static void ApplyCaseMap(TCHAR* text, int start, int len, DWORD flags)
{
	if (!text || len <= 0)
		return;

	if (flags == LCMAP_UPPERCASE)
	{
		CharUpperBuff(text + start, len);
		return;
	}

	if (flags == LCMAP_LOWERCASE)
		CharLowerBuff(text + start, len);
}


static CString GetReplStr(const CString& rstr, AU::ReMatch rm, RRList& rl)
{
	CString rep;
	rep.GetBuffer(rstr.GetLength());
	rep.ReleaseBuffer(0);

	AU::ReSubMatches rs(rm->SubMatches);

	RR cr;
	memset(&cr, 0, sizeof(cr));
	int flags=0;

	CString rv;
	bool emptyParam = false;

	for(int i = 0; i < rstr.GetLength(); ++i)
	{
		if ((rstr[i] == L'$' && i < rstr.GetLength() - 1) ||
			(rstr[i] == L'\\' && i < rstr.GetLength() - 1))
		{
			switch(rstr[++i])
			{
				//case L'&': // whole match
				case L'0': // whole match
					rv=(const wchar_t *)rm->Value;
					break;
				case L'+': // last submatch
					rv = GetSM(rs, rs->Count - 1);
					break;
				case L'1':
				case L'2':
				case L'3':
				case L'4':
				case L'5':
				case L'6':
				case L'7':
				case L'8':
				case L'9':
					rv = GetSM(rs, rstr[i] - L'0' - 1);
					if(rv.IsEmpty()) 
						emptyParam = true;
					break;
				case L'T': // title case
					flags |= RR::TITLE;
					continue;
				case L'U': // uppercase
					flags |= RR::UPPER;
					continue;
				case L'L': // lowercase
					flags |= RR::LOWER;
					continue;
				case L'S': // strong
					flags |= RR::STRONG;
					continue;
				case L'E': // emphasis
					flags |= RR::EMPHASIS;
					continue;
				case L'Q': // turn off flags
					flags = 0;
					continue;
				default: // ignore
					continue;
			}
		}

		if(cr.flags != flags && cr.flags && cr.start < rep.GetLength())
		{
			cr.len = rep.GetLength() - cr.start;
			rl.Add(cr);
			cr.flags = 0;
		}

		if(flags)
		{
			cr.flags = flags;
			cr.start = rep.GetLength();
		}

		// SeNS: fix for issue #142
		if (!emptyParam)
		{
			if(!rv.IsEmpty())
			{
				rep += rv;
				rv.Empty();
			}
			else rep += rstr[i];
		}
		else emptyParam = false;
	}

		if(cr.flags && cr.start < rep.GetLength())
		{
			cr.len = rep.GetLength() - cr.start;
			rl.Add(cr);
		}

		// process case conversions here
		int tl = rep.GetLength();
		TCHAR* cp = rep.GetBuffer(tl);
		for(int j = 0; j < rl.GetSize();)
		{
			RR rr = rl[j];
			if(rr.flags & RR::UPPER)
				ApplyCaseMap(cp, rr.start, rr.len, LCMAP_UPPERCASE);
			else if(rr.flags & RR::LOWER)
				ApplyCaseMap(cp, rr.start, rr.len, LCMAP_LOWERCASE);
			else if(rr.flags & RR::TITLE && rr.len > 0)
			{
				ApplyCaseMap(cp, rr.start, 1, LCMAP_UPPERCASE);
				ApplyCaseMap(cp, rr.start + 1, rr.len - 1, LCMAP_LOWERCASE);
			}
	
			if((rr.flags &~ (RR::UPPER | RR::LOWER | RR::TITLE)) == 0)
				rl.RemoveAt(j);
			else
				++j;
		}

		rep.ReleaseBuffer(tl);

	return rep;
}

static CString PrepareRegexReplacementText(const CString& replacementTemplate, AU::ReMatch match, RRList& formatting)
{
	CString replacement(GetReplStr(replacementTemplate, match, formatting));
	NormalizeReplacementNbsp(replacement);
	return replacement;
}

static void ApplyReplacementFormatting(MSHTML::IHTMLTxtRangePtr sel, const CString& repl, const RRList& rl)
{
	for(int i = 0; i < rl.GetSize(); ++i)
	{
		RR rr = rl[i];
		MSHTML::IHTMLTxtRangePtr range = sel->duplicate();
		range->move(L"character", rr.start - repl.GetLength());
		range->moveEnd(L"character", rr.len);
		if(rr.flags & RR::STRONG)
			range->execCommand(L"Bold", VARIANT_FALSE);
		if(rr.flags & RR::EMPHASIS)
			range->execCommand(L"Italic", VARIANT_FALSE);
	}
}

void  CFBEView::DoReplace() {
  try {
    MSHTML::IHTMLTxtRangePtr  sel(Document()->selection->createRange());
    if (!(bool)sel)
      return;
    int			      adv=0;

	m_mk_srv->BeginUndoUnit(L"replace");

    if (m_fo.hasMatch && m_fo.match) { // use regexp match copy
      RRList	rl;
      CString rep(PrepareRegexReplacementText(m_fo.replacement, m_fo.match, rl));

      sel->text=(const wchar_t *)rep;
      ApplyReplacementFormatting(sel, rep, rl);
      adv=rep.GetLength();
	  m_fo.ClearMatch();
    } else { // plain text
      sel->text=(const wchar_t *)m_fo.replacement;
      adv=m_fo.replacement.GetLength();
    }
    sel->moveStart(L"character",-adv);
    sel->select();
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }
  m_mk_srv->EndUndoUnit();
}

int CFBEView::GlobalReplace(MSHTML::IHTMLElementPtr elem, CString cntTag)
{
	if(m_fo.pattern.IsEmpty())
		return 0;

	try
	{
		MSHTML::IHTMLTxtRangePtr sel(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
		if(elem)
			sel->moveToElementText(elem);
		if(!(bool)sel)
			return 0;

		ScopedSearchRegExp re;
		NormalizeSearchPatternNbsp(m_fo.pattern);
		InitSearchRegExp(re.get(), m_fo.flags, m_fo.pattern);

		m_mk_srv->BeginUndoUnit(L"replace");

		sel->collapse(VARIANT_TRUE);

		int nRepl = 0;

		if(m_fo.fRegexp)
		{
			MSHTML::IHTMLElementCollectionPtr all;
			if(elem)
				all = MSHTML::IHTMLElement2Ptr(elem)->getElementsByTagName(cntTag.AllocSysString());
			else
				all = MSHTML::IHTMLDocument3Ptr(Document())->getElementsByTagName(cntTag.AllocSysString());
			_bstr_t charstr(L"character");
			RRList rl;
			CString repl;

			for(long l = 0;l < all->length; ++l)
			{
				MSHTML::IHTMLElementPtr elem(all->item(l));
				sel->moveToElementText(elem);;
				AU::ReMatches rm(ExecuteSearchRegExp(re.get(), sel));
				if(rm->Count <= 0)
					continue;

				// SeNS: fix for issue #147
				MSHTML::IHTMLTxtRangePtr rng = sel->duplicate();
				CString text = rng->text;
				CString html = rng->htmlText;

				// Replace
				sel->collapse(VARIANT_TRUE);
				long last = 0;
				for(long i = 0; i < rm->Count; ++i)
				{
					AU::ReMatch cur(rm->Item[i]);
					long delta = cur->FirstIndex - last;

					// SeNS
					delta += TextOffset (rng, cur, text, html);

					if(delta)
					{
						sel->move(charstr, delta);
						last += delta;
					}
					if(sel->moveStart(charstr, 1) == 1)
						sel->move(charstr, -1);
					delta = cur->Length;
					last += cur->Length;
					sel->moveEnd(charstr, delta);
					rl.RemoveAll();
					repl = PrepareRegexReplacementText(m_fo.replacement, cur, rl);

					sel->text = (const wchar_t*)repl;
					ApplyReplacementFormatting(sel, repl, rl);
					++nRepl;
				}
			}
		}
		else
		{
			DWORD flags = m_fo.flags & ~FRF_REVERSE;
			_bstr_t pattern((const wchar_t*)m_fo.pattern);
			_bstr_t repl((const wchar_t*)m_fo.replacement);
				while(sel->findText(pattern, 1073741824, flags) == VARIANT_TRUE)
				{
					sel->text = repl;
					++nRepl;
				}
		}

		m_mk_srv->EndUndoUnit();
		return nRepl;
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return 0;
}

int CFBEView::ToolWordsGlobalReplace(	MSHTML::IHTMLElementPtr fbw_body,
										int* pIndex,
										int* globIndex,
										bool find,
										CString cntTag)
{
	if(m_fo.pattern.IsEmpty())
		return 0;

	int nRepl = 0;

	try
	{
		ScopedSearchRegExp re;
		NormalizeSearchPatternNbsp(m_fo.pattern);
		InitSearchRegExp(re.get(), m_fo.flags, m_fo.pattern);
		re->Global = m_fo.flags & FRF_WHOLE ? VARIANT_TRUE : VARIANT_FALSE;
		re->Multiline = VARIANT_TRUE;
		MSHTML::IHTMLElementCollectionPtr paras = MSHTML::IHTMLElement2Ptr(fbw_body)->getElementsByTagName(cntTag.AllocSysString());
		if(!paras->length)
			return 0;

		int iNextElem = pIndex != NULL ? *pIndex : 0;
		CSimpleArray<CFBEView::pElAdjacent> pAdjElems;

		while(iNextElem < paras->length)
		{
			pAdjElems.RemoveAll();

			MSHTML::IHTMLElementPtr currElem(paras->item(iNextElem));
			CString innerText = currElem->innerText;
			pAdjElems.Add(pElAdjacent(currElem));

			if(pIndex != NULL)
				*pIndex = iNextElem;

			MSHTML::IHTMLDOMNodePtr currNode(currElem);
			if(MSHTML::IHTMLElementPtr siblElem = currNode->nextSibling)
			{
				int jNextElem = iNextElem + 1;
				for(int i = jNextElem; i < paras->length; ++i)
				{
					MSHTML::IHTMLElementPtr nextElem = paras->item(i);
					if(siblElem == nextElem)
					{
						pAdjElems.Add(pElAdjacent(siblElem));
						innerText += L"\n";
						innerText += siblElem->innerText.GetBSTR();
						iNextElem++;
						siblElem = MSHTML::IHTMLDOMNodePtr(nextElem)->nextSibling;
					}
					else
					{
						break;
					}
				}
			}
			innerText += L"\n";

			if(innerText.IsEmpty())
			{
				iNextElem++;
				continue;
			}

			// Replace
			AU::ReMatches rm(ExecuteSearchRegExp(re.get(), innerText));
			if(rm->Count <= 0)
			{
				iNextElem++;
				continue;
			}

			for(long i = 0; i < rm->Count; ++i)
			{
				AU::ReMatch cur(rm->Item[i]);

				long matchIdx = cur->FirstIndex;
				long matchLen = cur->Length - 1;

				long pAdjLen = 0;
				bool begin = false, end = false;
				int first = 0, last = 0;

				for(int b = 0; b < pAdjElems.GetSize(); ++b)
				{
					int pElemLen = pAdjElems[b].innerText.length() + 1;

					if(!pElemLen)
						continue;

					pAdjLen += pElemLen;

					if(matchIdx < pAdjLen && !begin)
					{
						begin = true;
						first = b;
					}

					if(matchIdx + (matchLen - 1) < pAdjLen)
					{
						end = true;
						last = b;
						break;
					}
				}

				for(int skip = 0; skip < first; ++skip)
					matchIdx -= (pAdjElems[skip].innerText.length() + 1);

				CString newCont;
				for(int index = first; index <= last; ++index)
					newCont += static_cast<const wchar_t*>(pAdjElems[index].innerText);

				MSHTML::IHTMLTxtRangePtr found(Document()->selection->createRange());
				found->moveToElementText(pAdjElems[first].elem);

				// SeNS: fix for issue #148
				int foundOffset = matchIdx + TextOffset(found, cur);
				MoveRangeToRegexMatch(found, cur, foundOffset - cur->FirstIndex);
				found->select();

				if(find)
				{
					if(i == rm->Count - 1)
					{
						(*globIndex) = -1;
						(*pIndex) += (pAdjElems.GetSize());
					}
					else
						(*globIndex)++;

					if(*globIndex > i)
					{
						(*globIndex)--;
						continue;
					}

					return 0;
				}

				found->text = m_fo.replacement.AllocSysString();

				newCont.Delete(matchIdx, matchLen - (last - first));
				newCont.Insert(matchIdx, m_fo.replacement);

				pAdjElems[first].innerText = newCont.AllocSysString();
				//pAdjElems[first].elem->innerText = pAdjElems[first].innerText;

				for(int c = first + 1; c <= last; ++c)
				{
				//	MSHTML::IHTMLDOMNodePtr(pAdjElems[c].elem)->removeNode(VARIANT_TRUE);
					iNextElem--;
				}

				for(int c = first + 1; c < last; ++c)
					pAdjElems.RemoveAt(c);

				if(nRepl >= m_fo.replNum)
					goto stop;

				CString again;
				for(int index = 0; index < pAdjElems.GetSize(); ++index)
				{
					again += static_cast<const wchar_t*>(pAdjElems[index].innerText);
					again += L"\n";
				}
				rm = ExecuteSearchRegExp(re.get(), again);
				i--;

				nRepl++;
			}

			iNextElem++;
		}

stop:
		if(find)
		{
			Document()->selection->empty();
			return -1;
		}
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return nRepl;
}

class CViewReplaceDlg : public CReplaceDlgBase {
public:
  CViewReplaceDlg(CFBEView *view) : CReplaceDlgBase(view) { }

  virtual void DoFind() {
    if (!m_view->DoSearch())
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, m_view->m_fo.pattern);	
	}
    else {
      SaveString();
      SaveHistory();
      m_selvalid=true;
      MakeClose();
    }
  }
  virtual void DoReplace() {
    if (m_selvalid) { // replace
      m_view->DoReplace();
      m_selvalid=false;
    }
	m_view->m_startMatch = m_view->m_endMatch = 0;
    DoFind();
  }
  virtual void DoReplaceAll() {
    int nRepl=m_view->GlobalReplace();
    if (nRepl>0) {
      SaveString();
      SaveHistory();
      U::MessageBox(MB_OK, IDS_REPL_ALL_CAPT, IDS_REPL_DONE_MSG, nRepl);
      MakeClose();
      m_selvalid=false;
    } else
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, m_view->m_fo.pattern);
	}
  }
};

LRESULT CFBEView::OnFind(WORD, WORD, HWND, BOOL&)
{
	m_fo.pattern = (const wchar_t*)Selection();
	if(!m_find_dlg)
		m_find_dlg = new CViewFindDlg(this);

	if(!m_find_dlg->IsValid())
		m_find_dlg->ShowDialog(*this); // show modeless
	else
		m_find_dlg->SetFocus();
	return 0;
}

LRESULT CFBEView::OnReplace(WORD, WORD, HWND, BOOL&)
{
	m_fo.pattern = (const wchar_t *)Selection();
	if(!m_replace_dlg)
		m_replace_dlg = new CViewReplaceDlg(this);

	if(!m_replace_dlg->IsValid())
		m_replace_dlg->ShowDialog(*this);
	else
		m_replace_dlg->SetFocus();
	return 0;
}

LRESULT  CFBEView::OnFindNext(WORD, WORD, HWND, BOOL&) {
  if (!DoSearch())
  {
	U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, m_fo.pattern);
  }
  return 0;
}

// binary objects
_variant_t	CFBEView::GetBinary(const wchar_t *id) {
  try {
    CComDispatchDriver    dd(Script());
    _variant_t    ret;
    _variant_t    arg(id);
    if (SUCCEEDED(dd.Invoke1(L"GetBinary",&arg,&ret)))
      return ret;
  }
  catch (_com_error&) { }
  return _variant_t();
}

// change notifications
void	CFBEView::EditorChanged(int id) {
  switch (id) {
  case FWD_SINK:
    break;
  case BACK_SINK:
    break;
  case RANGE_SINK:
	m_startMatch = m_endMatch = 0;
    if (!m_ignore_changes)
      ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_ED_CHANGED),(LPARAM)m_hWnd);
    break;
  }
}

// DWebBrowserEvents2
void  CFBEView::OnDocumentComplete(IDispatch *pDisp,VARIANT *vtUrl) {
  CComPtr<IUnknown> eventBrowser;
  CComPtr<IUnknown> topLevelBrowser;
  if (pDisp) pDisp->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&eventBrowser));
  if (m_browser) m_browser->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&topLevelBrowser));
  if (!eventBrowser || !topLevelBrowser || eventBrowser != topLevelBrowser)
  {
    StartupTrace::Event(L"webbrowser", L"WB141", L"DocumentComplete ignored for frame");
    return;
  }
  CString url = (vtUrl && V_VT(vtUrl) == VT_BSTR) ? StartupTrace::RedactPath(V_BSTR(vtUrl)) : CString(L"-");
  CString readyState(L"(unknown)");
  try
  {
    MSHTML::IHTMLDocument2Ptr document = m_browser ? m_browser->Document : NULL;
    if (document) readyState = (const wchar_t*)_bstr_t(document->readyState);
  }
  catch (_com_error&) { }
  const ULONGLONG elapsed = m_navigation_started ? ::GetTickCount64() - m_navigation_started : 0;
  CString details; details.Format(L"url=%s; ready-state=%s; navigate-elapsed=%llu", (LPCWSTR)url, (LPCWSTR)readyState, elapsed);
  StartupTrace::Event(L"webbrowser", L"WB140", details);
  m_last_browser_event=L"DocumentComplete";
  m_complete=true;
}

bool CFBEView::Init()
{
  StartupTrace::Event(L"webbrowser", L"WB160", L"CFBEView::Init begin");
  if (!m_browser)
  {
    StartupTrace::Error(L"webbrowser", L"WB200", L"IWebBrowser2 unavailable");
    return false;
  }

  CComPtr<IDispatch> documentDispatch;
  HRESULT hr = m_browser->get_Document(&documentDispatch);
  StartupTrace::HResult(L"webbrowser", L"WB200", hr, L"IWebBrowser2::get_Document");
  if (FAILED(hr) || !documentDispatch) return false;

  CComPtr<MSHTML::IHTMLDocument2> document;
  hr = documentDispatch->QueryInterface(&document);
  StartupTrace::HResult(L"webbrowser", L"WB201", hr, L"QueryInterface(IHTMLDocument2)");
  if (FAILED(hr) || !document) return false;
  m_hdoc = document.p;

  // MSHTML otherwise turns text resembling a UNC path (for example, \\word)
  // into a file:// hyperlink when the editor loses focus.  Links in FB2 must
  // only be created by an explicit editor command.
  try
  {
    if (document->execCommand(L"AutoUrlDetect", VARIANT_FALSE, _variant_t(VARIANT_FALSE)) != VARIANT_TRUE)
      StartupTrace::Warning(L"webbrowser", L"WB205", L"AutoUrlDetect was not disabled");
  }
  catch (const _com_error& error)
  {
    StartupTrace::HResult(L"webbrowser", L"WB205", error.Error(), L"Disable AutoUrlDetect");
  }

  CComPtr<MSHTML::IMarkupServices2> markupServices;
  hr = document->QueryInterface(&markupServices);
  StartupTrace::HResult(L"webbrowser", L"WB210", hr, L"QueryInterface(IMarkupServices2)");
  if (FAILED(hr) || !markupServices) return false;
  m_mk_srv = markupServices.p;

  CComPtr<MSHTML::IMarkupContainer2> markupContainer;
  hr = document->QueryInterface(&markupContainer);
  StartupTrace::HResult(L"webbrowser", L"WB220", hr, L"QueryInterface(IMarkupContainer2)");
  if (FAILED(hr) || !markupContainer) return false;
  m_mkc = markupContainer.p;

  CComPtr<MSHTML::IHTMLElement> body;
  hr = document->get_body(&body);
  StartupTrace::HResult(L"webbrowser", L"WB225", hr, L"IHTMLDocument2::get_body");
  if (FAILED(hr) || !body) return false;

  DocumentEvents::DispEventUnadvise(document, &DIID_HTMLDocumentEvents2);
  hr = DocumentEvents::DispEventAdvise(document, &DIID_HTMLDocumentEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB230", hr, L"DocumentEvents::DispEventAdvise");
  if (FAILED(hr)) return false;

  TextEvents::DispEventUnadvise(body, &DIID_HTMLTextContainerEvents2);
  hr = TextEvents::DispEventAdvise(body, &DIID_HTMLTextContainerEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB240", hr, L"TextEvents::DispEventAdvise");
  if (FAILED(hr)) return false;

  hr = m_mkc->RegisterForDirtyRange((RangeSink*)this, &m_dirtyRangeCookie);
  StartupTrace::HResult(L"webbrowser", L"WB250", hr, L"RegisterForDirtyRange");
  if (FAILED(hr)) return false;

  IDispatchPtr helper = CreateHelper();
  if (!helper)
  {
    StartupTrace::Error(L"webbrowser", L"WB260", L"CreateHelper returned null");
    return false;
  }
  StartupTrace::Event(L"webbrowser", L"WB260", L"CreateHelper completed");
	if (IsSecondSetExternalFaultEnabled())
	{
		hr = E_FAIL;
		StartupTrace::HResult(L"fault", L"FI011", hr, L"second SetExternalDispatch injected failure");
	}
	else
		hr = SetExternalDispatch(helper);
  StartupTrace::HResult(L"webbrowser", L"WB270", hr, L"SetExternalDispatch #2");
  if (FAILED(hr)) return false;

  MSHTML::IHTMLElement2Ptr body2(body.p);
  MSHTML::IHTMLDOMNodePtr bodyNode(body.p);
  if (!body2 || !bodyNode)
  {
    StartupTrace::Error(L"webbrowser", L"WB275", L"body does not expose required interfaces");
    return false;
  }
  StartupTrace::Event(L"webbrowser", L"WB280", L"FixupParagraphs");
  FixupParagraphs(body2);
  if (m_normalize)
  {
    StartupTrace::Event(L"webbrowser", L"WB290", L"Normalize");
    Normalize(bodyNode);
  }

  if (!m_normalize) {
    MSHTML::IHTMLElementCollectionPtr all(document->all);
    MSHTML::IHTMLInputElementPtr ii(all->item(L"diID"));
    if ((bool)ii && ii->value.length()==0) {
      UUID uuid;
      unsigned char *str;
      if (UuidCreate(&uuid)==RPC_S_OK && UuidToStringA(&uuid,&str)==RPC_S_OK) {
        CString us(str);
        RpcStringFreeA(&str);
        us.MakeUpper();
        ii->value=(const wchar_t *)us;
      }
    }
    ii=all->item(L"diVersion");
    if ((bool)ii && ii->value.length()==0) ii->value=L"1.0";
    ii=all->item(L"diDate");
    MSHTML::IHTMLInputElementPtr jj(all->item(L"diDateVal"));
    if ((bool)ii && (bool)jj && ii->value.length()==0 && jj->value.length()==0) {
      time_t tt;
      time(&tt);
      char buffer[128];
      strftime(buffer,sizeof(buffer),"%Y-%m-%d",localtime(&tt));
      ii->value=buffer;
      jj->value=buffer;
    }
    ii=all->item(L"diProgs");
    if ((bool)ii && ii->value.length()==0) ii->value=L"FB Tools";
  }

  hr = m_browser->put_RegisterAsDropTarget(VARIANT_FALSE);
  StartupTrace::HResult(L"webbrowser", L"WB295", hr, L"put_RegisterAsDropTarget");
  if (FAILED(hr)) return false;
  m_initialized=true;
  StartupTrace::Event(L"webbrowser", L"WB299", L"CFBEView::Init completed");
  return true;
}
void CFBEView::OnNavigateError(IDispatch* pDisp, VARIANT* vtUrl, VARIANT* vtFrame, VARIANT* vtStatusCode, VARIANT_BOOL* fCancel)
{
  CComPtr<IUnknown> eventBrowser;
  CComPtr<IUnknown> topLevelBrowser;
  if (pDisp) pDisp->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&eventBrowser));
  if (m_browser) m_browser->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&topLevelBrowser));
  const bool topLevel = eventBrowser && topLevelBrowser && eventBrowser == topLevelBrowser;
  CString url = (vtUrl && V_VT(vtUrl) == VT_BSTR) ? StartupTrace::RedactPath(V_BSTR(vtUrl)) : CString(L"-");
  CString frame = (vtFrame && V_VT(vtFrame) == VT_BSTR) ? StartupTrace::SanitizeLogText(V_BSTR(vtFrame), 64) : CString(L"-");
  long status = 0;
  if (vtStatusCode && (V_VT(vtStatusCode) == VT_I4 || V_VT(vtStatusCode) == VT_INT)) status = V_I4(vtStatusCode);
  const int cancelled = fCancel && *fCancel == VARIANT_TRUE ? 1 : 0;
  const ULONGLONG elapsed = m_navigation_started ? ::GetTickCount64() - m_navigation_started : 0;
  CString details;
  details.Format(L"url=%s; frame=%s; status=%ld; top-level=%d; cancel=%d; navigate-elapsed=%llu", (LPCWSTR)url, (LPCWSTR)frame, status, topLevel ? 1 : 0, cancelled, elapsed);
  if (!topLevel)
  {
    StartupTrace::Event(L"webbrowser", L"WB136", details);
    return;
  }
  m_navigation_failed = true;
  m_navigation_status = status;
  m_last_browser_event = L"NavigateError";
  StartupTrace::Warning(L"webbrowser", L"WB135", details);
}
void  CFBEView::OnBeforeNavigate(IDispatch *pDisp,VARIANT *vtUrl,VARIANT *vtFlags,
				 VARIANT *vtTargetFrame,VARIANT *vtPostData,
				 VARIANT *vtHeaders,VARIANT_BOOL *fCancel)
{
  m_last_browser_event=L"BeforeNavigate";
  if (!m_initialized)
    return;

  if (vtUrl && V_VT(vtUrl)==VT_BSTR) {
    m_nav_url=V_BSTR(vtUrl);

    if (m_nav_url.Left(13)==_T("fbw-internal:"))
      return;

	// changed by SeNS: possible fix for issue #87
	// tested on Windows Vista Ultimate
    ::PostMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_NAVIGATE),(LPARAM)m_hWnd);
  }

  // disable navigating away
  *fCancel=VARIANT_TRUE;
}

// HTMLDocumentEvents
void  CFBEView::OnSelChange(IDispatch *evt) {
  if (!m_ignore_changes)
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
  if (m_cur_sel)
    m_cur_sel.Release();
}

VARIANT_BOOL  CFBEView::OnContextMenu(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	oe->cancelBubble = VARIANT_TRUE;
	oe->returnValue = VARIANT_FALSE;
	if(!m_normalize)
	{
		MSHTML::IHTMLElementPtr elem(oe->srcElement);
		if(!(bool)elem)
			return VARIANT_TRUE;
		if(U::scmp(elem->tagName,L"INPUT") && U::scmp(elem->tagName, L"TEXTAREA"))
			return VARIANT_TRUE;
	}

	// display custom context menu here
	CMenu menu;
	CString itemName;

	menu.CreatePopupMenu();
	itemName = FbeLoadCString(IDS_HOTKEY_EDIT_UNDO);
	menu.AppendMenu(MF_STRING, ID_EDIT_UNDO, itemName);
	menu.AppendMenu(MF_SEPARATOR);

	itemName = FbeLoadCString(IDS_CTXMENU_CUT);
	menu.AppendMenu(MF_STRING, ID_EDIT_CUT, itemName);

	itemName = FbeLoadCString(IDS_CTXMENU_COPY);
	menu.AppendMenu(MF_STRING, ID_EDIT_COPY, itemName);

	itemName = FbeLoadCString(IDS_CTXMENU_PASTE);
	menu.AppendMenu(MF_STRING, ID_EDIT_PASTE, itemName);

	// The table commands must be available where the user edits a cell, not
	// only in the main menu.  The source element may be an inline child, so
	// walk up to its TD/TH ancestor.
	MSHTML::IHTMLElementPtr contextCell(FindTableCell(MSHTML::IHTMLElementPtr(oe->srcElement)));
	if (m_normalize && contextCell)
	{
		// Right-click does not reliably move the MSHTML selection.  Retain the
		// clicked cell so the command modifies the cell the menu was opened on.
		if (m_table_selection_cells.empty())
			m_cur_sel = contextCell;
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_ROW_ABOVE, GetLocalizedMainMenuText(ID_TABLE_INSERT_ROW_ABOVE, L"Insert row above"));
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_ROW_BELOW, GetLocalizedMainMenuText(ID_TABLE_INSERT_ROW_BELOW, L"Insert row below"));
		menu.AppendMenu(MF_STRING, ID_TABLE_DELETE_ROW, GetLocalizedMainMenuText(ID_TABLE_DELETE_ROW, L"Delete row"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_COLUMN_LEFT, GetLocalizedMainMenuText(ID_TABLE_INSERT_COLUMN_LEFT, L"Insert column left"));
		menu.AppendMenu(MF_STRING, ID_TABLE_INSERT_COLUMN_RIGHT, GetLocalizedMainMenuText(ID_TABLE_INSERT_COLUMN_RIGHT, L"Insert column right"));
		menu.AppendMenu(MF_STRING, ID_TABLE_DELETE_COLUMN, GetLocalizedMainMenuText(ID_TABLE_DELETE_COLUMN, L"Delete column"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, ID_TABLE_MAKE_HEADER_CELLS, GetLocalizedMainMenuText(ID_TABLE_MAKE_HEADER_CELLS, L"Make header cells"));
		menu.AppendMenu(MF_STRING, ID_TABLE_MAKE_NORMAL_CELLS, GetLocalizedMainMenuText(ID_TABLE_MAKE_NORMAL_CELLS, L"Make normal cells"));
	}

	if(m_normalize)
	{
		menu.AppendMenu(MF_SEPARATOR);
		MSHTML::IHTMLElementPtr cur(SelectionContainer());
		MSHTML::IHTMLElementPtr initial(cur);
		int cmd = ID_SEL_BASE;
		itemName = FbeLoadCString(IDS_CTXMENU_SELECT);

		while((bool)cur && U::scmp(cur->tagName,L"BODY") && U::scmp(cur->id, L"fbw_body"))
		{
			menu.AppendMenu(MF_STRING, cmd, itemName + L" " + GetPath(cur));
			cur = cur->parentElement;
			++cmd;
		}
		if(U::scmp(initial->className, L"image") == 0)
		{
			MSHTML::IHTMLImgElementPtr image = MSHTML::IHTMLDOMNodePtr(initial)->firstChild;
			CString src = image->src.GetBSTR();
			src.Delete(src.Find(L"fbw-internal:"), 13);
			if(src != L"#undefined")
			{
				menu.AppendMenu(MF_SEPARATOR);
				itemName = FbeLoadCString(IDS_CTXMENU_IMG_SAVEAS);
				menu.AppendMenu(MF_STRING, ID_SAVEIMG_AS, itemName);
			}
		}
	}

	AU::TRACKPARAMS tp;
	tp.hMenu = menu;
	tp.uFlags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON;
	tp.x = oe->screenX;
	tp.y = oe->screenY;
	::SendMessage(m_frame, AU::WM_TRACKPOPUPMENU, 0, (LPARAM)&tp);

	return VARIANT_TRUE;
}

LRESULT CFBEView::OnSelectElement(WORD, WORD wID, HWND, BOOL&) {
  int	steps=wID-ID_SEL_BASE;
  try {
    MSHTML::IHTMLElementPtr	  cur(SelectionContainer());

    while ((bool)cur && steps-->0)
      cur=cur->parentElement;

    MSHTML::IHTMLTxtRangePtr	  r(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());

    r->moveToElementText(cur);

    ++m_ignore_changes;
    r->select();
    --m_ignore_changes;

    m_cur_sel=cur;
    ::SendMessage(m_frame,WM_COMMAND,MAKELONG(0,IDN_SEL_CHANGE),(LPARAM)m_hWnd);
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }

  return 0;
}

VARIANT_BOOL  CFBEView::OnClick(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	MSHTML::IHTMLElementPtr elem(oe->srcElement);	

  	m_startMatch = m_endMatch = 0;

	if(!(bool)elem)
		return VARIANT_FALSE;

	MSHTML::IHTMLElementPtr parent_element = elem->parentElement;

	if(!(bool)parent_element)
		return VARIANT_FALSE;

	bstr_t pc = parent_element->className;

	if(!U::scmp(pc, L"image"))
	{
		// make image selected
		IHTMLControlRangePtr r(((MSHTML::IHTMLElement2Ptr)(Document()->body))->createControlRange());
		HRESULT hr = r->add((IHTMLControlElementPtr)elem->parentElement);
		hr = r->select();
		//::SendMessage(m_frame, WM_COMMAND, MAKELONG(IDC_HREF, IDN_WANTFOCUS), (LPARAM)m_hWnd);

		return VARIANT_TRUE;
	}

	if (U::scmp(elem->tagName,L"A"))
	{
		return VARIANT_FALSE;
	}
	/*else
	{
	  ::SendMessage(m_frame, WM_COMMAND, MAKELONG(IDC_HREF, IDN_WANTFOCUS), (LPARAM)m_hWnd);
	  return VARIANT_FALSE;
	}*/

	if(oe->altKey!=VARIANT_TRUE || oe->shiftKey==VARIANT_TRUE || oe->ctrlKey==VARIANT_TRUE)
		return VARIANT_FALSE;

	CString sref(AU::GetAttrCS(elem, L"href"));
	if(sref.IsEmpty() || sref[0] != L'#')
		return VARIANT_FALSE;

	sref.Delete(0);

	MSHTML::IHTMLElementPtr targ(Document()->all->item((const wchar_t*)sref));

	if(!(bool)targ)
		return VARIANT_FALSE;

	GoTo(targ);
	
	oe->cancelBubble = VARIANT_TRUE;
	oe->returnValue = VARIANT_FALSE;

	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnMouseDown(IDispatch* evt)
{
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	if (!eventObject || eventObject->button != 1) return VARIANT_FALSE;
	m_table_selection_dragging = false;
	if (m_table_selection_anchor) m_table_selection_anchor.Release();
	UpdateTableCellHighlights(m_table_selection_cells, std::vector<MSHTML::IHTMLElementPtr>());
	MSHTML::IHTMLElementPtr source(eventObject->srcElement);
	MSHTML::IHTMLElementPtr cell(FindTableCell(source));
	if (!cell) return VARIANT_FALSE;
	m_table_selection_anchor = cell;
	m_table_selection_dragging = true;
	return VARIANT_FALSE;
}

VARIANT_BOOL CFBEView::OnMouseMove(IDispatch* evt)
{
	if (!m_table_selection_dragging || !m_table_selection_anchor) return VARIANT_FALSE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	if (!cell || cell == m_table_selection_anchor || !GetTableCellRectangle(m_table_selection_anchor, cell, cells) || !SelectTableCellRange(Document(), m_table_selection_anchor, cell)) return VARIANT_FALSE;
	UpdateTableCellHighlights(m_table_selection_cells, cells);
	eventObject->cancelBubble = VARIANT_TRUE;
	eventObject->returnValue = VARIANT_FALSE;
	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnMouseUp(IDispatch* evt)
{
	if (!m_table_selection_dragging) return VARIANT_FALSE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	if (cell && cell != m_table_selection_anchor && GetTableCellRectangle(m_table_selection_anchor, cell, cells) && SelectTableCellRange(Document(), m_table_selection_anchor, cell)) UpdateTableCellHighlights(m_table_selection_cells, cells);
	m_table_selection_dragging = false;
	m_table_selection_anchor.Release();
	return VARIANT_FALSE;
}

bool CFBEView::MoveTableCell(bool reverse)
{
	try
	{
		if (!HasDoc()) return false;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		MSHTML::IHTMLElementPtr cell(FindTableCell(selection ? selection->parentElement() : MSHTML::IHTMLElementPtr()));
		MSHTML::IHTMLElementPtr row(FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FindTableElement(row));
		if (!cell || !row || !table) return false;

		std::vector<MSHTML::IHTMLElementPtr> cells;
		GetTableCells(table, cells);
		size_t index = 0;
		while (index < cells.size() && cells[index] != cell) ++index;
		if (index == cells.size()) return false;

		if (!reverse && index + 1 == cells.size())
		{
			BeginUndoUnit(L"insert table row below");
			MSHTML::IHTMLElement2Ptr(row)->insertAdjacentElement(L"afterEnd", CreateTableRowLike(Document(), row));
			EndUndoUnit();
			::SendMessage(m_frame, WM_COMMAND, MAKELONG(0, IDN_SEL_CHANGE), reinterpret_cast<LPARAM>(m_hWnd));
			::SendMessage(m_frame, WM_COMMAND, MAKELONG(0, IDN_TREE_RESTORE), 0);
			GetTableCells(table, cells);
		}

		size_t targetIndex = index;
		if (reverse) {
			if (targetIndex > 0) --targetIndex;
		} else if (targetIndex + 1 < cells.size()) {
			++targetIndex;
		}
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
		range->moveToElementText(cells[targetIndex]);
		range->collapse(VARIANT_TRUE);
		range->select();
		return true;
	}
	catch (_com_error&) { return false; }
}

VARIANT_BOOL  CFBEView::OnKeyDown(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	if (oe && oe->keyCode == VK_TAB && MoveTableCell(oe->shiftKey == VARIANT_TRUE))
	{
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_FALSE;
	}
	if (oe && (oe->keyCode == VK_LEFT || oe->keyCode == VK_UP || oe->keyCode == VK_PRIOR || oe->keyCode == VK_HOME))
		m_startMatch = m_endMatch = 0;
	return VARIANT_TRUE;
}

VARIANT_BOOL  CFBEView::OnRealPaste(IDispatch* evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	oe->cancelBubble = VARIANT_TRUE;
	if(!m_enable_paste)
	{
		// Blocks first OnRealPaste to stop double-insertion
		SendMessage(WM_COMMAND, MAKELONG(ID_EDIT_PASTE, 0), 0);
		oe->returnValue = VARIANT_FALSE;
	}
	else
	{
		oe->returnValue = VARIANT_TRUE;
	}

	return VARIANT_TRUE;
}

bool  CFBEView::IsFormChanged() {
  if (!m_form_changed && (bool)m_cur_input)
    m_form_changed=m_form_changed || m_cur_input->value != m_cur_val;
  return m_form_changed;
}

bool  CFBEView::IsFormCP() {
  if (!m_form_cp && (bool)m_cur_input)
    m_form_cp=m_form_cp || m_cur_input->value != m_cur_val;
  return m_form_cp;
}

void  CFBEView::ResetFormChanged() {
  m_form_changed=false;
  if (m_cur_input)
    m_cur_val=m_cur_input->value;
}

void  CFBEView::ResetFormCP() {
  m_form_cp=false;
  if (m_cur_input)
    m_cur_val=m_cur_input->value;
}

void  CFBEView::OnFocusIn(IDispatch *evt) {
  // check previous value
  if (m_cur_input) {
    bool cv=m_cur_input->value != m_cur_val;
    m_form_changed=m_form_changed || cv;
    m_form_cp=m_form_cp || cv;
    m_cur_input.Release();
  }

  MSHTML::IHTMLEventObjPtr  oe(evt);
  if (!(bool)oe)
    return;

  MSHTML::IHTMLElementPtr   te(oe->srcElement);
  if (!(bool)te || U::scmp(te->tagName,L"INPUT"))
    return;

  m_cur_input=te;
  if (!(bool)m_cur_input)
    return;

  if (U::scmp(m_cur_input->type,L"text")) {
    m_cur_input.Release();
    return;
  }

  m_cur_val=m_cur_input->value;
}

// find/replace support for scintilla
bool CFBEView::SciFindNext(HWND src,bool fFwdOnly,bool fBarf) {
  if (m_fo.pattern.IsEmpty())
    return true;

  int	    flags=0;
  if (m_fo.flags & FRF_WHOLE)
    flags|=SCFIND_WHOLEWORD;
  if (m_fo.flags & FRF_CASE)
    flags|=SCFIND_MATCHCASE;
  if (m_fo.fRegexp)
    flags|=SCFIND_REGEXP|SCFIND_CXX11REGEX;
  int rev=m_fo.flags & FRF_REVERSE && !fFwdOnly;

  NormalizeSearchPatternNbsp(m_fo.pattern);

  DWORD   len=::WideCharToMultiByte(CP_UTF8,0, m_fo.pattern,m_fo.pattern.GetLength(), NULL,0,NULL,NULL);
  std::vector<char> tmp(len+1);
  if (!tmp.empty()) 
  {
    ::WideCharToMultiByte(CP_UTF8,0, m_fo.pattern,m_fo.pattern.GetLength(), tmp.data(),len,NULL,NULL);
    tmp[len]='\0';
    int p1=::SendMessage(src,SCI_GETSELECTIONSTART,0,0);
    int p2=::SendMessage(src,SCI_GETSELECTIONEND,0,0);
	if (p2>p1 && !rev) p1=p2;
//   if (p1!=p2 && !rev) ++p1;
    if (rev) --p1;
    if (p1<0) p1=0;
    p2=rev ? 0 : ::SendMessage(src,SCI_GETLENGTH,0,0);
    int p3=p2==0 ? ::SendMessage(src,SCI_GETLENGTH,0,0) : 0;
    ::SendMessage(src,SCI_SETTARGETSTART,p1,0);
    ::SendMessage(src,SCI_SETTARGETEND,p2,0);
    ::SendMessage(src,SCI_SETSEARCHFLAGS,flags,0);
    // this sometimes hangs in reverse search :)
    int ret=::SendMessage(src,SCI_SEARCHINTARGET,len,(LPARAM)tmp.data());
    if (ret==-1) 
	{ // try wrap
		if (p1!=p3) 
		{
			::SendMessage(src,SCI_SETTARGETSTART,p3,0);
			::SendMessage(src,SCI_SETTARGETEND,p1,0);
			::SendMessage(src,SCI_SETSEARCHFLAGS,flags,0);
			ret=::SendMessage(src,SCI_SEARCHINTARGET,len,(LPARAM)tmp.data());
		}
		if (ret==-1) 
		{
			if (fBarf)
			{
				U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, m_fo.pattern);
			}
			return false;
		}
		::MessageBeep(MB_ICONASTERISK);
    }
    p1=::SendMessage(src,SCI_GETTARGETSTART,0,0);
    p2=::SendMessage(src,SCI_GETTARGETEND,0,0);
    ::SendMessage(src,SCI_SETSELECTIONSTART,p1,0);
    ::SendMessage(src,SCI_SETSELECTIONEND,p2,0);
    ::SendMessage(src,SCI_SCROLLCARET,0,0);
    return true;
  } else
  {
    wchar_t msg[MAX_LOAD_STRING + 1];
	wchar_t cpt[MAX_LOAD_STRING + 1];
	FbeLoadString(_Module.GetResourceInstance(), IDS_OUT_OF_MEM_MSG, msg, MAX_LOAD_STRING);
	FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
    ::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONERROR);
  }

  return false;
}

_bstr_t CFBEView::Selection()
{
	try
	{
		MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
		if(!(bool)rng)
			return _bstr_t();

		MSHTML::IHTMLTxtRangePtr dup(rng->duplicate());
		dup->collapse(VARIANT_TRUE);

		MSHTML::IHTMLElementPtr elem(dup->parentElement());
		while ((bool)elem && U::scmp(elem->tagName, L"P") && U::scmp(elem->tagName, L"DIV"))
			elem = elem->parentElement;

		if(elem)
		{
			dup->moveToElementText(elem);
			if(rng->compareEndPoints(L"EndToEnd", dup) > 0)
				rng->setEndPoint(L"EndToEnd", dup);
		}

		return rng->text;
	}
	catch (_com_error& err)
	{
		U::ReportError(err);
	}

	return _bstr_t();
}

// Modification by Pilgrim
static bool IsTable(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"table")==0;
}

static bool IsTR(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"tr")==0;
}

static bool IsTH(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"th")==0;
}

static bool IsTD(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"td")==0;
}

bool CFBEView::GoToFootnote(bool fCheck)
{
	// * create selection range
	MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
	if (!(bool)rng)
		return false;

	MSHTML::IHTMLTxtRangePtr next_rng = rng->duplicate();
	MSHTML::IHTMLTxtRangePtr prev_rng = rng->duplicate();
	next_rng->moveEnd(L"character", +1);
	prev_rng->moveStart(L"character", -1);

	CString	sref(AU::GetAttrCS(SelectionAnchor(),L"href"));
	if (sref.IsEmpty())
		sref = AU::GetAttrCS(SelectionAnchor(next_rng->parentElement()),L"href");
	if (sref.IsEmpty())
		sref = AU::GetAttrCS(SelectionAnchor(prev_rng->parentElement()),L"href");

	if (sref.Find(L"file") == 0)
		sref = sref.Mid(sref.ReverseFind (L'#'),1024);
	if (sref.IsEmpty() || sref[0]!=_T('#'))
		return false;

	// * ok, all checks passed
	if (fCheck)
		return true;

	sref.Delete(0);

	MSHTML::IHTMLElementPtr     targ(Document()->all->item((const wchar_t *)sref));

	if (!(bool)targ)
		return false;

	MSHTML::IHTMLDOMNodePtr childNode;
	MSHTML::IHTMLDOMNodePtr node(targ);
	if (!(bool)node)
		return false;

	// added by SeNS: move caret to the foornote text
	if (!U::scmp(node->nodeName,L"DIV") && !U::scmp(targ->className,L"section"))
	{
		if (node->firstChild) 
		{
			childNode = node->firstChild;
			while (childNode && !U::scmp(childNode->nodeName,L"DIV") && 
				  (!U::scmp(MSHTML::IHTMLElementPtr(childNode)->className,L"image") || 
				   !U::scmp(MSHTML::IHTMLElementPtr(childNode)->className,L"title"))) 
				childNode=childNode->nextSibling;
		}
	}
	if (!childNode) childNode=node;
	if (childNode)
	{
		GoTo(MSHTML::IHTMLElementPtr(childNode));
		targ->scrollIntoView(true);
	}

	return true; 
}
bool CFBEView::GoToReference(bool fCheck)
{
	// * create selection range
	MSHTML::IHTMLTxtRangePtr	rng(Document()->selection->createRange());
	if (!(bool)rng)
		return false;

	// * get its parent element
	MSHTML::IHTMLElementPtr	pe(GetHP(rng->parentElement()));
	if (!(bool)pe)
		return false;

	if (rng->compareEndPoints(L"StartToEnd",rng)!=0)
		return false;

	while((bool)pe && (U::scmp(pe->tagName,L"DIV")!=0 || U::scmp(pe->className,L"section")!=0)) 
		pe=pe->parentElement; // Find parent division
	if(!(bool)pe) 
		return false;

	MSHTML::IHTMLElementPtr body=pe->parentElement;
	
	while((bool)body && (U::scmp(body->tagName,L"DIV")!=0 || U::scmp(body->className,L"body")!=0)) 
		body=body->parentElement; // Find body

	if(!(bool)body) 
		return false;

	CString id = MSHTML::IHTMLElementPtr(rng->parentElement())->id;
	CString	sfbname(AU::GetAttrCS(body,L"fbname"));	
	if(id.IsEmpty() && (sfbname.IsEmpty() || !(sfbname.CompareNoCase(L"notes")==0 || sfbname.CompareNoCase(L"comments")==0)))
		return false;
	id = L"#"+id;
	
	// * ok, all checks passed
	if (fCheck)
		return true;

	MSHTML::IHTMLElement2Ptr			elem(Document()->body);
	MSHTML::IHTMLElementCollectionPtr	coll(elem->getElementsByTagName(L"A"));
	if (!(bool)coll || coll->length==0) 
	{
		wchar_t cpt[MAX_LOAD_STRING + 1];
		wchar_t msg[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
		FbeLoadString(_Module.GetResourceInstance(), IDS_GOTO_REF_FAIL_MSG, msg, MAX_LOAD_STRING);		
		::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONINFORMATION);
		return false;
	}

	for (long l=0;l<coll->length;++l) {
		MSHTML::IHTMLElementPtr a(coll->item(l));
		if (!(bool)a)
			continue;

		CString href(AU::GetAttrCS((MSHTML::IHTMLElementPtr)coll->item(l),L"href"));

		// changed by SeNS
		if (href.Find(L"file") == 0)
			href = href.Mid(href.ReverseFind (L'#'),1024);
		else if(href.Find(_T("://"),0) !=-1)
			continue;

		CString snote = L"#"+pe->id;

		if(href==snote || href==id)
		{
			GoTo(a);
			MSHTML::IHTMLTxtRangePtr r(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
			r->moveToElementText(a);
			r->collapse(VARIANT_TRUE);
			// move selection to position after reference
			CString sa = a->innerText;
			r->move(L"character", sa.GetLength());
			r->select();
			// scroll to the center of view
			MSHTML::IHTMLRectPtr rect = MSHTML::IHTMLElement2Ptr(a)->getBoundingClientRect();
			MSHTML::IHTMLWindow2Ptr window(MSHTML::IHTMLDocument2Ptr(Document())->parentWindow);
			if (rect && window)
			{
				if (rect->bottom-rect->top <= _Settings.GetViewHeight())
					window->scrollBy(0,(rect->top+rect->bottom-_Settings.GetViewHeight())/2);
				else
					window->scrollBy(0,rect->top);
			}
			break;
		}
	}

	return false;
}

LRESULT CFBEView::OnEditInsertTable(WORD wNotifyCode, WORD wID, HWND hWndCtl)
{
	CTableDlg dlg;
	if(dlg.DoModal()==IDOK) {
		int nRows = dlg.m_nRows;
		int nColumns = dlg.m_nColumns;
		bool bTitle = dlg.m_bTitle;
		InsertTable(false,bTitle,nRows,nColumns);
	}
	return 0;
}

static void NotifyTableStructureChanged(HWND frame, HWND view)
{
	::SendMessage(frame, WM_COMMAND, MAKELONG(0, IDN_SEL_CHANGE), reinterpret_cast<LPARAM>(view));
	::SendMessage(frame, WM_COMMAND, MAKELONG(0, IDN_TREE_RESTORE), 0);
}

LRESULT CFBEView::OnTableInsertRowAbove(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FindTableElement(row));
		LogicalTableGrid grid;
		if (!cell || !row || !BuildLogicalTableGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		BeginUndoUnit(L"insert table row above");
		std::vector<bool> expanded(grid.cells.size(), false);
		MSHTML::IHTMLElementPtr newRow(Document()->createElement(L"TR")); newRow->className = L"tr";
		for (long column = 0; column < grid.columns; ++column) {
			const long above = grid.At(rowIndex - 1, column), below = grid.At(rowIndex, column);
			if (above >= 0 && above == below && !expanded[above]) { SetTableSpan(grid.cells[above].element, L"fbrowspan", L"rowspan", grid.cells[above].rowspan + 1); expanded[above] = true; }
			else if (!(above >= 0 && above == below)) MSHTML::IHTMLElement2Ptr(newRow)->insertAdjacentElement(L"beforeEnd", CreateTableCell(Document(), TableCellTagAt(grid, rowIndex, column, cell->tagName)));
		}
		MSHTML::IHTMLElement2Ptr(row)->insertAdjacentElement(L"beforeBegin", newRow);
		EndUndoUnit();
		NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertRowBelow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FindTableElement(row));
		LogicalTableGrid grid;
		if (!cell || !row || !BuildLogicalTableGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		const long boundary = rowIndex + 1;
		BeginUndoUnit(L"insert table row below");
		std::vector<bool> expanded(grid.cells.size(), false);
		MSHTML::IHTMLElementPtr newRow(Document()->createElement(L"TR")); newRow->className = L"tr";
		for (long column = 0; column < grid.columns; ++column) {
			const long above = grid.At(boundary - 1, column), below = grid.At(boundary, column);
			if (above >= 0 && above == below && !expanded[above]) { SetTableSpan(grid.cells[above].element, L"fbrowspan", L"rowspan", grid.cells[above].rowspan + 1); expanded[above] = true; }
			else if (!(above >= 0 && above == below)) MSHTML::IHTMLElement2Ptr(newRow)->insertAdjacentElement(L"beforeEnd", CreateTableCell(Document(), TableCellTagAt(grid, boundary - 1, column, cell->tagName)));
		}
		MSHTML::IHTMLElement2Ptr(row)->insertAdjacentElement(L"afterEnd", newRow);
		EndUndoUnit();
		NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableDeleteRow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr row(FindTableRow(SelectionStructTableCon()));
		LogicalTableGrid grid;
		if (!row || !row->parentElement || !BuildLogicalTableGrid(FindTableElement(row), grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		BeginUndoUnit(L"delete table row");
		for (size_t index = 0; index < grid.cells.size(); ++index) {
			LogicalTableCell& current = grid.cells[index];
			if (current.sourceRow < rowIndex && current.sourceRow + current.rowspan > rowIndex)
				SetTableSpan(current.element, L"fbrowspan", L"rowspan", current.rowspan - 1);
			else if (current.sourceRow == rowIndex && current.rowspan > 1 && rowIndex + 1 < static_cast<long>(grid.rows.size())) {
				SetTableSpan(current.element, L"fbrowspan", L"rowspan", current.rowspan - 1);
				long before = -1;
				for (size_t other = 0; other < grid.cells.size(); ++other)
					if (grid.cells[other].sourceRow == rowIndex + 1 && grid.cells[other].startColumn >= current.startColumn &&
						(before < 0 || grid.cells[other].startColumn < grid.cells[before].startColumn)) before = static_cast<long>(other);
				if (before >= 0) MSHTML::IHTMLElement2Ptr(grid.cells[before].element)->insertAdjacentElement(L"beforeBegin", current.element);
				else MSHTML::IHTMLElement2Ptr(grid.rows[rowIndex + 1])->insertAdjacentElement(L"beforeEnd", current.element);
			}
		}
		MSHTML::IHTMLDOMNodePtr(row->parentElement)->removeChild(MSHTML::IHTMLDOMNodePtr(row));
		EndUndoUnit();
		NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

static bool InsertTableColumn(CFBEView* view, bool before)
{
	MSHTML::IHTMLElementPtr selectedCell(view->SelectionStructTableCon());
	MSHTML::IHTMLElementPtr selectedRow(FindTableRow(selectedCell));
	MSHTML::IHTMLElementPtr table(FindTableElement(selectedRow));
	LogicalTableGrid grid;
	if (!selectedCell || !selectedRow || !BuildLogicalTableGrid(table, grid)) return false;
	const long selectedIndex = FindLogicalCell(grid, selectedCell);
	if (selectedIndex < 0) return false;
	const long column = grid.cells[selectedIndex].startColumn + (before ? 0 : grid.cells[selectedIndex].colspan);
	view->BeginUndoUnit(before ? L"insert table column left" : L"insert table column right");
	std::vector<bool> expanded(grid.cells.size(), false);
	for (long rowIndex = 0; rowIndex < static_cast<long>(grid.rows.size()); ++rowIndex) {
		const long left = grid.At(rowIndex, column - 1), right = grid.At(rowIndex, column);
		if (left >= 0 && left == right && !expanded[left]) { SetTableSpan(grid.cells[left].element, L"fbcolspan", L"colspan", grid.cells[left].colspan + 1); expanded[left] = true; }
		else InsertCellAtLogicalColumn(view->Document(), grid, rowIndex, column, TableCellTagAt(grid, rowIndex, before ? column : column - 1, selectedCell->tagName));
	}
	view->EndUndoUnit();
	return true;
}

LRESULT CFBEView::OnTableInsertColumnLeft(WORD, WORD, HWND, BOOL&)
{
	try { if (InsertTableColumn(this, true)) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertColumnRight(WORD, WORD, HWND, BOOL&)
{
	try { if (InsertTableColumn(this, false)) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

static bool DeleteTableLogicalColumn(CFBEView* view, const LogicalTableGrid& grid, long column)
{
	if (!view || column < 0 || column >= grid.columns) return false;
	view->BeginUndoUnit(L"delete table column");
	std::vector<bool> handled(grid.cells.size(), false);
	for (long rowIndex = 0; rowIndex < static_cast<long>(grid.rows.size()); ++rowIndex) {
		const long owner = grid.At(rowIndex, column);
		if (owner < 0 || handled[owner]) continue;
		handled[owner] = true;
		if (grid.cells[owner].colspan > 1) SetTableSpan(grid.cells[owner].element, L"fbcolspan", L"colspan", grid.cells[owner].colspan - 1);
		else MSHTML::IHTMLDOMNodePtr(grid.cells[owner].element->parentElement)->removeChild(MSHTML::IHTMLDOMNodePtr(grid.cells[owner].element));
	}
	view->EndUndoUnit();
	return true;
}

bool CFBEView::DeleteTableLogicalColumnForTest(long column)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		LogicalTableGrid grid;
		if (!BuildLogicalTableGrid(table, grid) || !DeleteTableLogicalColumn(this, grid, column)) return false;
		NotifyTableStructureChanged(m_frame, m_hWnd);
		return true;
	}
	catch (_com_error&) { return false; }
}

LRESULT CFBEView::OnTableDeleteColumn(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr selectedCell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr selectedRow(FindTableRow(selectedCell));
		MSHTML::IHTMLElementPtr table(FindTableElement(selectedRow));
		LogicalTableGrid grid;
		if (!selectedCell || !selectedRow || !BuildLogicalTableGrid(table, grid)) return 0;
		const long selectedIndex = FindLogicalCell(grid, selectedCell);
		if (selectedIndex < 0) return 0;
		if (DeleteTableLogicalColumn(this, grid, grid.cells[selectedIndex].startColumn)) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableToggleHeaderCell(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		if (!cell || !FindTableElement(cell)) return 0;
		const wchar_t* targetName = U::scmp(cell->tagName, L"TH") == 0 ? L"TD" : L"TH";
		MSHTML::IHTMLElementPtr replacement(CreateTableCell(Document(), targetName));
		replacement->innerHTML = cell->innerHTML;
		CopyTableCellReplacementAttributes(cell, replacement);
		MSHTML::IHTMLDOMNodePtr(cell->parentElement)->replaceChild(MSHTML::IHTMLDOMNodePtr(replacement), MSHTML::IHTMLDOMNodePtr(cell));
		std::vector<TableCellReplacement> replacements; TableCellReplacement pair = { replacement, cell }; replacements.push_back(pair);
		const HRESULT undoResult = AddTableCellToggleUndoUnit(Document(), replacements);
		if (FAILED(undoResult)) {
			MSHTML::IHTMLDOMNodePtr(replacement->parentElement)->replaceChild(MSHTML::IHTMLDOMNodePtr(cell), MSHTML::IHTMLDOMNodePtr(replacement));
			_com_issue_error(undoResult);
		}
		NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnEditInsImage(WORD, WORD cmdID, HWND, BOOL&)
{
	// added by SeNS
	bool bInline = (cmdID != ID_EDIT_INS_IMAGE);
	
	if(_Settings.GetInsImageAsking())
	{
		CAddImageDlg imgDialog;
		imgDialog.DoModal(*this);
	}

	if(!_Settings.GetIsInsClearImage())
	{
		CString imageFilter = ImageImportFileFilter();
		CFileDialogEx dlg(
			TRUE,
			NULL,
			NULL,
			OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
			imageFilter
			);

		wchar_t dlgTitle[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDS_ADD_IMAGE_FILEDLG, dlgTitle, MAX_LOAD_STRING);
		dlg.m_ofn.lpstrTitle = dlgTitle;
		dlg.m_ofn.nFilterIndex = 1;
		dlg.CenterOnOwner();

		if(dlg.DoModal(*this) == IDOK)
		{
			AddImage(dlg.m_szFileName, bInline);
		}
	}
	else
	{
		// added by SeNS
		try {
			if (bInline)
			{
				MSHTML::IHTMLDOMNodePtr node(Call(L"InsInlineImage"));
			}
			else
			{
				MSHTML::IHTMLDOMNodePtr node(Call(L"InsImage"));
				if (node)
					BubbleUp(node,L"DIV");
			}
		}
		catch (_com_error&) { }
	}

	return 0;
}

bool  CFBEView::InsertTable(bool fCheck, bool bTitle, int nrows, int ncolumns) {
	try {
		// * create selection range
		MSHTML::IHTMLTxtRangePtr	rng(Document()->selection->createRange());
		if (!(bool)rng)
			return false;

		// * get its parent element
		MSHTML::IHTMLElementPtr	pe(GetHP(rng->parentElement()));
		if (!(bool)pe)
			return false;

		// * get parents for start and end ranges and ensure they are the same as pe
		MSHTML::IHTMLTxtRangePtr	tr(rng->duplicate());
		tr->collapse(VARIANT_TRUE);
		if (GetHP(tr->parentElement())!=pe)
			return false;
#if 0
		tr=rng->duplicate();
		tr->collapse(VARIANT_FALSE);
		if (GetHP(tr->parentElement())!=pe)
			return false;
#endif

		// * check if it possible to insert a table there
		_bstr_t   cls(pe->className);
		if (U::scmp(cls,L"section") && U::scmp(cls,L"epigraph") &&
			U::scmp(cls,L"annotation") && U::scmp(cls,L"history") && U::scmp(cls,L"cite"))
			return false;

		// * ok, all checks passed
		if (fCheck)
			return true;

		// at this point we are ready to create a table
		// Structural editor operations use direct DOM insertion inside a markup
		// undo unit.  Find the current paragraph as an insertion anchor.
		MSHTML::IHTMLElementPtr anchor(rng->parentElement());
		while (anchor && U::scmp(anchor->tagName, L"P") != 0)
			anchor = anchor->parentElement;
		if (!anchor || anchor->parentElement != pe)
			return false;

		// * create an undo unit
		m_mk_srv->BeginUndoUnit(L"insert table");

		MSHTML::IHTMLElementPtr te(Document()->createElement(L"TABLE"));
		te->className = L"table";
		// MSHTML only materializes rows/cells added through the DOM when they
		// are placed in a table section.  HTML parsing creates TBODY for us,
		// but direct DOM insertion must do it explicitly.
		MSHTML::IHTMLElementPtr tbody(Document()->createElement(L"TBODY"));
		MSHTML::IHTMLElement2Ptr(te)->insertAdjacentElement(L"beforeEnd", tbody);
		nrows = max(1, nrows);
		ncolumns = max(1, ncolumns);
		const int totalRows = nrows + (bTitle ? 1 : 0);
		MSHTML::IHTMLElementPtr firstCell;
		for (int row = 0; row < totalRows; ++row)
		{
			MSHTML::IHTMLElementPtr tre(Document()->createElement(L"TR"));
			tre->className = L"tr";
			const wchar_t* cellType = bTitle && row == 0 ? L"TH" : L"TD";
			for (int column = 0; column < ncolumns; ++column) {
				MSHTML::IHTMLElementPtr cell(CreateTableCell(Document(), cellType));
				if (!firstCell) firstCell = cell;
				MSHTML::IHTMLElement2Ptr(tre)->insertAdjacentElement(L"beforeEnd", cell);
			}
			MSHTML::IHTMLElement2Ptr(tbody)->insertAdjacentElement(L"beforeEnd", tre);
		}

		// Unlike pasteHTML/InsertHTML, direct insertion is supported by this
		// MSHTML host and is captured by the surrounding undo unit.
		MSHTML::IHTMLElement2Ptr(anchor)->insertAdjacentElement(L"afterEnd", te);

		// * ensure we have good html
		RelocateParagraphs(MSHTML::IHTMLDOMNodePtr(pe));
		FixupParagraphs(pe);

		// * close undo unit
		m_mk_srv->EndUndoUnit();
		if (firstCell) {
			MSHTML::IHTMLTxtRangePtr cellRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
			cellRange->moveToElementText(firstCell);
			cellRange->collapse(VARIANT_TRUE);
			cellRange->select();
		}
		// Refresh command state after the modal table dialog. Without this the
		// Undo button can remain disabled even though MSHTML has an undo unit.
		::SendMessage(m_frame, WM_COMMAND, MAKELONG(0, IDN_SEL_CHANGE), reinterpret_cast<LPARAM>(m_hWnd));
		return true;
	}
	catch (_com_error& e) {
		U::ReportError(e);
	}
	return false;
}

long CFBEView::InsertCode()
{	
	if(bCall(L"IsCode", SelectionStructCode()))
	{
		HRESULT hr;
		BeginUndoUnit(L"insert code");
		hr = m_mk_srv->RemoveElement(SelectionStructCode());
		EndUndoUnit();
		return hr == S_OK ? 0 : -1;
	}
	else
	{
		bool undoStarted = false;
		try
		{
			BeginUndoUnit(L"insert code");
			undoStarted = true;

			int offset = -1;
			MSHTML::IHTMLTxtRangePtr rng(Document()->selection->createRange());
			if (!(bool)rng)
			{
				undoStarted = false;
				EndUndoUnit();
				return -1;
			}

			CString rngHTML((wchar_t*)rng->htmlText);

			// empty selection case - select current word
			if(rngHTML.IsEmpty())
			{
				// select word
				rng->moveStart(L"word",-1);
				CString txt = rng->text;
				offset = txt.GetLength();
				rng->expand(L"word");
				rngHTML.SetString(rng->htmlText);
			}

			if (!rngHTML.IsEmpty() && iswspace(rngHTML[rngHTML.GetLength()-1])) 
			{
				rng->moveEnd(L"character",-1);
				rngHTML.SetString(rng->htmlText);
				if (offset > rngHTML.GetLength()) offset--;
			}

			// save selection
			MSHTML::IMarkupPointerPtr selBegin, selEnd;
			m_mk_srv->CreateMarkupPointer(&selBegin);
			m_mk_srv->CreateMarkupPointer(&selEnd);
			m_mk_srv->MovePointersToRange(rng, selBegin, selEnd);

			if(rngHTML.Find(L"<P") != -1)
			{
				MSHTML::IHTMLElementPtr spanElem = Document()->createElement(L"<SPAN class=code>");
				MSHTML::IHTMLElementPtr selElem = rng->parentElement();
				
				MSHTML::IHTMLTxtRangePtr rngStart = rng->duplicate();
				MSHTML::IHTMLTxtRangePtr rngEnd = rng->duplicate();
				rngStart->collapse(VARIANT_TRUE);
				rngEnd->collapse(VARIANT_FALSE);

				MSHTML::IHTMLElementPtr elBegin = rngStart->parentElement(), elEnd = rngEnd->parentElement();
				while(U::scmp(elBegin->tagName, L"P")) elBegin = elBegin->parentElement;
				while(U::scmp(elEnd->tagName, L"P")) elEnd = elEnd->parentElement;

				MSHTML::IHTMLDOMNodePtr bNode = elBegin, eNode = elEnd;
				int last = 0;
				while(bNode)
				{
					CString elBeginHTML = elBegin->innerHTML;

					if(U::scmp(elBegin->tagName, L"P") == 0 && elBeginHTML.Find(L"<SPAN") < 0)
					{
						spanElem->innerHTML = elBegin->innerHTML;
						if(!(elBeginHTML.Find(L"<SPAN class=code>") == 0 && 
							elBeginHTML.Find(L"</SPAN>") == elBeginHTML.GetLength() - 7))
						{
							elBegin->innerHTML = spanElem->outerHTML;
						}
					}
					// remove code tag
					else
					{
						elBeginHTML.Replace (L"<SPAN class=code>", L" ");
						elBeginHTML.Replace (L"</SPAN>", L" ");
						elBegin->innerHTML = elBeginHTML.AllocSysString();
					}

					if(bNode == eNode) 
						break;

					bNode = bNode->nextSibling;
					elBegin = bNode;
				}
				// expand selection to the last paragraph
				rng->moveToElementText(elBegin);
				m_mk_srv->MovePointersToRange(rng, NULL, selEnd); 
			}
			else if(rngHTML.Find(L"<SPAN class=code>") != -1 && rngHTML.Find(L"</SPAN>") != -1)
			{
					rngHTML.Replace (L"<SPAN class=code>", L" ");
					rngHTML.Replace (L"</SPAN>", L" ");
					rng->pasteHTML(rngHTML.AllocSysString());
			}
			else
			{
				if (!rngHTML.IsEmpty() && iswspace(rngHTML[0]))
				{
					rng->moveStart(L"character",1);
					rngHTML.SetString(rng->htmlText);
				}
				if (!rngHTML.IsEmpty() && iswspace(rngHTML[rngHTML.GetLength()-1]))
				{
					rng->moveEnd(L"character",-1);
					rngHTML.SetString(rng->htmlText);
				}			
				rngHTML = L"<SPAN class=code>" + rngHTML + L"</SPAN>";
				rng->pasteHTML(rngHTML.AllocSysString());
			}

			// restore selection
			if (offset >= 0)
			{
				rng->move(L"word", -1);
				rng->move(L"character", offset);
				rng->select();
			}
			else
			{
				m_mk_srv->MoveRangeToPointers(selBegin, selEnd, rng);
				rng->select();
			}

			undoStarted = false;
			EndUndoUnit();
		}
		catch (_com_error& e)
		{
			if (undoStarted)
				EndUndoUnit();
			U::ReportError(e);
			return -1;
		}

		return 0;
	}
}

int CFBEView::GetRangePos(const MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr &element, int &pos)
{
	MSHTML::IHTMLTxtRangePtr	tr(range->duplicate());
	tr->collapse(VARIANT_TRUE);

	// * get its parent element
	element = tr->parentElement();

	MSHTML::IHTMLTxtRangePtr btr(range->duplicate());
	btr->moveToElementText(element);
	btr->collapse(VARIANT_TRUE);

	pos = 0;

	MSHTML::IHTMLDOMNodePtr node(element);
	if(!(bool)node)
	{
		return 0;
	}

	int count = 0;
	int cuttedchars = 0;

	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode)
		{
			int skip = CountNodeChars(node);
			cuttedchars += skip;
            btr->move(L"character", skip);
		}
		else
		{
			// ��������� �� ���������� �� �� ������� �������
			int skip = count + textNode->length;			
			btr->move(L"character", skip);

			if(btr->compareEndPoints(L"StartToStart", tr) != -1)
			{
				btr->move(L"character", -skip);
				break;
			}
			pos += skip;
		}
		
		node = node->nextSibling;
	}

	// ����� ��������. 
	// ���� ������ ����� ����� ����� ����, �� tr ����������� ������ �� brt � ��� ���� ������� �� ������ ����� ���
	int k = btr->compareEndPoints(L"StartToStart", tr);
	if(k == -1)
	{
		int res = btr->move(L"character", 1);		
		if (res != 1)
		{
			return 0;
		}
		if(btr->compareEndPoints(L"StartToStart", tr) != 1)
		{
			++pos;
		}
	}

	while(btr->compareEndPoints(L"StartToStart", tr) == -1)
	{
		++pos;	
		int res = btr->move(L"character", 1);		
		if (res != 1)
		{
			return 0;
		}
	}

	return pos;
}

bool CFBEView::GetSelectionInfo(MSHTML::IHTMLElementPtr *begin, MSHTML::IHTMLElementPtr *end, int* begin_char, int* end_char, MSHTML::IHTMLTxtRangePtr range)
{
	*begin_char = 0;
	*end_char = 0;

	int b = 0;
	int e = 0;

	bool one_elment = false;
	// * create selection range
	MSHTML::IHTMLTxtRangePtr	rng;	
	if(!(bool)range)
	{
		IDispatchPtr disp(Document()->selection->createRange());
		rng = disp;
		if (!(bool)rng)
		{
			// ���� �� ���������� ������� textrange, ������� ������� control range
			MSHTML::IHTMLControlRangePtr  coll(disp);
			if (!(bool)coll)
			{
				return false;
			}	
			*begin = coll->item(0);
			*end = coll->item(coll->length - 1);
			return true;
		}
	}
	else
		rng = range;

	bstr_t text = rng->text;

	MSHTML::IHTMLTxtRangePtr	tr(rng->duplicate());
	tr->collapse(VARIANT_TRUE);

	// * get its parent element
	*begin = tr->parentElement();
	if (!(bool)(*begin))
		return false;

	// ���� ������� ������������ ������;
	this->GetRangePos(tr, *begin, b);

	tr = rng->duplicate();
	tr->collapse(VARIANT_FALSE);
	*end = tr->parentElement();
	if (*end == *begin)
	{
		one_elment = true;
	}

	this->GetRangePos(tr, *end, e);

	MSHTML::IHTMLDOMNodePtr nodeb(*begin);
	MSHTML::IHTMLDOMNodePtr nodee(*end);
	if(!(bool)nodeb || !(bool)nodee)
	{
		return false;
	}

	/*b = this->GetRelationalCharPos(nodeb, b);
	e = this->GetRelationalCharPos(nodee, e);*/
	
	*begin_char = b;
	*end_char = e;

	return true;
}

MSHTML::IHTMLTxtRangePtr CFBEView::SetSelection(MSHTML::IHTMLElementPtr begin, MSHTML::IHTMLElementPtr end, int begin_pos, int end_pos)
{
	if(!(bool)begin)
	{
		return 0;
	}
	if(!(bool)end)
	{
		end = begin;
		end_pos = begin_pos;
	}

	begin_pos = this->GetRealCharPos(begin, begin_pos);
	end_pos = this->GetRealCharPos(end, end_pos);

	MSHTML::IHTMLTxtRangePtr rng(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
	if(!(bool)rng)
	{
		return 0;
	}

	// ������������� ������ ���������� ������
	MSHTML::IHTMLTxtRangePtr rng_begin(rng->duplicate());
	rng_begin->moveToElementText(begin);
	rng_begin->collapse(VARIANT_TRUE);
	rng_begin->moveStart(L"character", begin_pos);

	if(begin == end)
	{
		rng_begin->moveEnd(L"character", end_pos - begin_pos);
		HRESULT hr = rng_begin->select();		
		
		return rng_begin;
	}

	MSHTML::IHTMLTxtRangePtr rng_end(rng->duplicate());
	rng_end->moveToElementText(end);
	rng_end->moveStart(L"character", end_pos);

	// ���������� ������
	rng_begin->setEndPoint(L"EndToStart", rng_end);

	rng_begin->select();
	

	return rng_begin;
}

int CFBEView::GetRelationalCharPos(MSHTML::IHTMLDOMNodePtr node, int pos)
{
	if(!(bool)node)
	{
		return 0;
	}

	int relpos = 0;
	int cuttedchars = 0;

	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode)
		{
			cuttedchars += CountNodeChars(node);			
		}
		else
		{
			if(relpos + cuttedchars + textNode->length >= pos)
			{
				return pos - cuttedchars;
			}
			relpos += textNode->length;
		}
		node = node->nextSibling;
	}

	return 0;
}

int CFBEView::GetRealCharPos(MSHTML::IHTMLDOMNodePtr node, int pos)
{
	if(!(bool)node)
	{
		return 0;
	}

	int realpos = 0;
	int cuttedchars = 0;

	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode)
		{
			cuttedchars += CountNodeChars(node);			
		}
		else
		{
			if((realpos + textNode->length) >= pos)
			{
				return pos + cuttedchars;
			}
			realpos += textNode->length;
		}
		node = node->nextSibling;
	}

	return 0;
}

int CFBEView::CountNodeChars(MSHTML::IHTMLDOMNodePtr node)
{
	if(!(bool)node)
	{
		return 0;
	}

	int count = 0;

	node = node->firstChild;
	while(node)
	{
		MSHTML::IHTMLDOMTextNodePtr textNode(node);
		if(!(bool)textNode)
		{
			count += CountNodeChars(node);
		}		
		else
		{
			count += textNode->length;
		}
		node = node->nextSibling;
	}

	return count;
}

bool CFBEView::CloseFindDialog(CFindDlgBase* dlg)
{
	if(!dlg || !dlg->IsValid())
		return false;

	dlg->DestroyWindow();
	return true;
}

bool CFBEView::CloseFindDialog(CReplaceDlgBase* dlg)
{
	if(!dlg || !dlg->IsValid())
		return false;

	dlg->DestroyWindow();
	return true;
}

bool CFBEView::ExpandTxtRangeToParagraphs(MSHTML::IHTMLTxtRangePtr& rng,
											MSHTML::IHTMLElementPtr& begin,
											MSHTML::IHTMLElementPtr& end) const
{
	MSHTML::IHTMLTxtRangePtr tr1 = rng->duplicate(); 
	tr1->collapse(VARIANT_TRUE);

	MSHTML::IHTMLElementPtr te = GetHP(tr1->parentElement());

	if(!(bool)te)
		return false;

	MSHTML::IHTMLTxtRangePtr tr2 = rng->duplicate(); 
	tr2->collapse(VARIANT_FALSE);

	begin = tr1->parentElement(); 
	while((bool)begin && U::scmp(begin->tagName, L"P"))
		begin = begin->parentElement;

	if(!(bool)begin)
		return false;

	end = tr2->parentElement();
	while((bool)end && U::scmp(end->tagName, L"P"))
		end = end->parentElement;

	if(!(bool)end)
		return false;

	if(begin == end)
		rng->moveToElementText(begin);
	else
	{
		MSHTML::IMarkupPointerPtr pBegin, pEnd;
		m_mk_srv->CreateMarkupPointer(&pBegin);
		m_mk_srv->CreateMarkupPointer(&pEnd);
		pBegin->MoveAdjacentToElement(begin, MSHTML::ELEM_ADJ_AfterBegin);
		pEnd->MoveAdjacentToElement(end, MSHTML::ELEM_ADJ_BeforeEnd);
		m_mk_srv->MoveRangeToPointers(pBegin, pEnd, rng);
	}

	return true;
}

LRESULT CFBEView::OnCode(WORD wCode, WORD wID, HWND hWnd, BOOL& bHandled)
{
	return InsertCode();
}

bool CFBEView::SelectionHasTags(wchar_t* elem)
{
	try
	{
		MSHTML::IHTMLTxtRangePtr range = Document()->selection->createRange();
		if(range)
		{
			CString html = range->htmlText;
			if(html.Find(CString(L"<") + elem) != -1)
				return true;
		}
	}
	catch(_com_error& err)
	{
		U::ReportError(err);
		return false;
	}

	return false;
}

BSTR CFBEView::PrepareDefaultId(const CString& filename){

    CString _filename = U::Transliterate(filename);
	// prepare a default id
	int cp = _filename.ReverseFind(_T('\\'));
	if (cp < 0)
		cp = 0;
	else
		++cp;
	CString   newid;
	TCHAR	    *ncp=newid.GetBuffer(_filename.GetLength()-cp);
	int	    newlen=0;
	while (cp<_filename.GetLength()) {
		TCHAR   c=_filename[cp];
		if ((c>=_T('0') && c<=_T('9')) ||
			(c>=_T('A') && c<=_T('Z')) ||
			(c>=_T('a') && c<=_T('z')) ||
			c==_T('_') || c==_T('-') || c==_T('.'))
			ncp[newlen++]=c;
		++cp;
	}
	newid.ReleaseBuffer(newlen);
	if (!newid.IsEmpty() && !(
		(newid[0]>=_T('A') && newid[0]<=_T('Z')) ||
		(newid[0]>=_T('a') && newid[0]<=_T('z')) ||
		newid[0]==_T('_')))
		newid.Insert(0,_T('_'));
	return newid.AllocSysString();
}

static bool GetSelectedTableCells(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& currentCell, std::vector<MSHTML::IHTMLElementPtr>& result);
static bool ReplaceTableCells(MSHTML::IHTMLDocument2Ptr document, const std::vector<MSHTML::IHTMLElementPtr>& cells, const wchar_t* targetName);

void CFBEView::ResetTableGridBuildCountForTest()
{
	if (IsTableGridInstrumentationEnabled()) g_tableGridBuildCount = 0;
}

long CFBEView::TableGridBuildCountForTest()
{
	return IsTableGridInstrumentationEnabled() ? g_tableGridBuildCount : -1;
}

bool CFBEView::SelectTableLogicalRangeForTest(long firstRow, long firstColumn, long lastRow, long lastColumn)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		LogicalTableGrid grid;
		if (!body || !BuildLogicalTableGrid(table, grid)) return false;
		const long first = grid.At(firstRow, firstColumn), last = grid.At(lastRow, lastColumn);
		if (first < 0 || last < 0) return false;
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		range->moveToElementText(grid.cells[first].element);
		if (first == last) range->collapse(VARIANT_TRUE);
		else {
			end->moveToElementText(grid.cells[last].element);
			range->setEndPoint(L"EndToEnd", end);
		}
		range->select();
		return true;
	}
	catch (_com_error&) { return false; }
}

static unsigned long HashNormalizedTableHtml(const CString& html)
{
	unsigned long hash = 2166136261u;
	for (int index = 0; index < html.GetLength(); ++index) {
		hash ^= static_cast<unsigned long>(html[index]);
		hash *= 16777619u;
	}
	return hash;
}

CStringA CFBEView::TableStructuralSnapshot()
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		LogicalTableGrid grid;
		if (!BuildLogicalTableGrid(table, grid)) return CStringA("invalid");
		CStringA snapshot; snapshot.Format("rows=%ld;columns=%ld;", static_cast<long>(grid.rows.size()), grid.columns);
		for (size_t index = 0; index < grid.cells.size(); ++index) {
			const LogicalTableCell& cell = grid.cells[index];
			CString id(AU::GetAttrCS(cell.element, L"id")), style, fbstyle(AU::GetAttrCS(cell.element, L"fbstyle"));
			MSHTML::IHTMLStylePtr runtimeStyle(cell.element ? cell.element->style : MSHTML::IHTMLStylePtr());
			if (runtimeStyle) style = (const wchar_t*)_bstr_t(runtimeStyle->cssText);
			CString colspan(AU::GetAttrCS(cell.element, L"colspan")), fbcolspan(AU::GetAttrCS(cell.element, L"fbcolspan"));
			CString rowspan(AU::GetAttrCS(cell.element, L"rowspan")), fbrowspan(AU::GetAttrCS(cell.element, L"fbrowspan"));
			CString align(AU::GetAttrCS(cell.element, L"align")), fbalign(AU::GetAttrCS(cell.element, L"fbalign"));
			CString valign(AU::GetAttrCS(cell.element, L"valign")), fbvalign(AU::GetAttrCS(cell.element, L"fbvalign"));
			_bstr_t innerHtml(cell.element->innerHTML);
			CStringA entry; entry.Format("c%u:id=%S,tag=%S,row=%ld,column=%ld,logical-colspan=%ld,logical-rowspan=%ld,html=%08lX,style=%S,fbstyle=%S,colspan=%S,fbcolspan=%S,rowspan=%S,fbrowspan=%S,align=%S,fbalign=%S,valign=%S,fbvalign=%S;", static_cast<unsigned>(index),
				(const wchar_t*)id, (const wchar_t*)cell.element->tagName, cell.sourceRow, cell.startColumn, cell.colspan, cell.rowspan,
				HashNormalizedTableHtml(CString((const wchar_t*)innerHtml)), (const wchar_t*)style, (const wchar_t*)fbstyle,
				(const wchar_t*)colspan, (const wchar_t*)fbcolspan, (const wchar_t*)rowspan, (const wchar_t*)fbrowspan,
				(const wchar_t*)align, (const wchar_t*)fbalign, (const wchar_t*)valign, (const wchar_t*)fbvalign);
			snapshot += entry;
		}
		for (long row = 0; row < static_cast<long>(grid.rows.size()); ++row) for (long column = 0; column < grid.columns; ++column) {
			CStringA entry; entry.Format("s%ld,%ld=%ld;", row, column, grid.At(row, column)); snapshot += entry;
		}
		return snapshot;
	}
	catch (_com_error&) { return CStringA("error"); }
}

static bool MakeSelectedTableCells(CFBEView* view, const wchar_t* targetName)
{
	MSHTML::IHTMLElementPtr currentCell(view->SelectionStructTableCon());
	std::vector<MSHTML::IHTMLElementPtr> cells;
	return GetSelectedTableCells(view->Document(), currentCell, cells) && ReplaceTableCells(view->Document(), cells, targetName);
}

LRESULT CFBEView::OnTableMakeHeaderCells(WORD, WORD, HWND, BOOL&)
{
	try { if (MakeSelectedTableCells(this, L"TH")) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableMakeNormalCells(WORD, WORD, HWND, BOOL&)
{
	try { if (MakeSelectedTableCells(this, L"TD")) NotifyTableStructureChanged(m_frame, m_hWnd); }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

static bool GetSelectedTableCells(MSHTML::IHTMLDocument2Ptr document, const MSHTML::IHTMLElementPtr& currentCell, std::vector<MSHTML::IHTMLElementPtr>& result)
{
	result.clear();
	MSHTML::IHTMLElementPtr anchor(FindTableCell(currentCell));
	try {
		if (!anchor) {
			MSHTML::IHTMLTxtRangePtr range(document->selection->createRange());
			if (range) { range->collapse(VARIANT_TRUE); anchor = FindTableCell(range->parentElement()); }
		}
	}
	catch (_com_error&) { }
	MSHTML::IHTMLElementPtr table(FindTableElement(anchor));
	LogicalTableGrid grid;
	if (!anchor || !table || !BuildLogicalTableGrid(table, grid)) return false;
	long first = FindLogicalCell(grid, anchor), last = first;
	try {
		MSHTML::IHTMLTxtRangePtr selection(document->selection->createRange());
		MSHTML::IHTMLTxtRangePtr start(selection ? selection->duplicate() : MSHTML::IHTMLTxtRangePtr()), end(selection ? selection->duplicate() : MSHTML::IHTMLTxtRangePtr());
		if (start && end) {
			start->collapse(VARIANT_TRUE); end->collapse(VARIANT_FALSE);
			MSHTML::IHTMLElementPtr firstCell(FindTableCell(start->parentElement())), lastCell(FindTableCell(end->parentElement()));
			const long selectedFirst = FindLogicalCell(grid, firstCell), selectedLast = FindLogicalCell(grid, lastCell);
			if (selectedFirst >= 0 && selectedLast >= 0) { first = selectedFirst; last = selectedLast; }
		}
	}
	catch (_com_error&) { }
	if (first < 0 || last < 0) return false;
	const LogicalTableCell& firstCell = grid.cells[first]; const LogicalTableCell& lastCell = grid.cells[last];
	const long rowStart = min(firstCell.sourceRow, lastCell.sourceRow), rowEnd = max(firstCell.sourceRow + firstCell.rowspan - 1, lastCell.sourceRow + lastCell.rowspan - 1);
	const long colStart = min(firstCell.startColumn, lastCell.startColumn), colEnd = max(firstCell.startColumn + firstCell.colspan - 1, lastCell.startColumn + lastCell.colspan - 1);
	std::vector<bool> selected(grid.cells.size(), false);
	for (long row = rowStart; row <= rowEnd; ++row) for (long column = colStart; column <= colEnd; ++column) {
		const long owner = grid.At(row, column); if (owner >= 0) selected[owner] = true;
	}
	for (size_t index = 0; index < grid.cells.size(); ++index) if (selected[index]) result.push_back(grid.cells[index].element);
	return !result.empty();
}

static bool ReplaceTableCells(MSHTML::IHTMLDocument2Ptr document, const std::vector<MSHTML::IHTMLElementPtr>& cells, const wchar_t* targetName)
{
	std::vector<TableCellReplacement> replacements;
	for (size_t index = 0; index < cells.size(); ++index) {
		if (!cells[index] || U::scmp(cells[index]->tagName, targetName) == 0) continue;
		MSHTML::IHTMLElementPtr replacement(CreateTableCell(document, targetName)); replacement->innerHTML = cells[index]->innerHTML;
		CopyTableCellReplacementAttributes(cells[index], replacement);
		MSHTML::IHTMLDOMNodePtr(cells[index]->parentElement)->replaceChild(MSHTML::IHTMLDOMNodePtr(replacement), MSHTML::IHTMLDOMNodePtr(cells[index]));
		TableCellReplacement pair = { replacement, cells[index] }; replacements.push_back(pair);
	}
	if (replacements.empty()) return false;
	const HRESULT undoResult = AddTableCellToggleUndoUnit(document, replacements);
	if (SUCCEEDED(undoResult)) return true;
	for (size_t index = replacements.size(); index > 0; --index) {
		TableCellReplacement& pair = replacements[index - 1];
		if (pair.active && pair.active->parentElement) MSHTML::IHTMLDOMNodePtr(pair.active->parentElement)->replaceChild(MSHTML::IHTMLDOMNodePtr(pair.inactive), MSHTML::IHTMLDOMNodePtr(pair.active));
	}
	_com_issue_error(undoResult);
	return false;
}

HRESULT CFBEView::AddImportedBinary(const BYTE* bytes, size_t size, const CString& logicalFileName,
	const CString& mimeType, _variant_t* checkedId)
{
	if (!bytes || !size || size > ULONG_MAX)
		return E_INVALIDARG;
	_variant_t args[4];
	SAFEARRAY* data = SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(size));
	if (!data)
		return E_OUTOFMEMORY;
	void* raw = NULL;
	HRESULT hr = SafeArrayAccessData(data, &raw);
	if (FAILED(hr)) {
		SafeArrayDestroy(data);
		return hr;
	}
	memcpy(raw, bytes, size);
	SafeArrayUnaccessData(data);
	V_ARRAY(&args[0]) = data;
	V_VT(&args[0]) = VT_ARRAY | VT_UI1;
	V_BSTR(&args[1]) = mimeType.AllocSysString();
	V_VT(&args[1]) = VT_BSTR;
	V_BSTR(&args[2]) = PrepareDefaultId(logicalFileName);
	V_VT(&args[2]) = VT_BSTR;
	// apiAddBinary's first argument is a real filesystem path, not the FB2
	// binary name.  Converted imports only have the latter, so force dimension
	// discovery from the bytes rather than accidentally resolving a same-named
	// file in the process working directory.
	V_BSTR(&args[3]) = ::SysAllocString(L"");
	V_VT(&args[3]) = VT_BSTR;
	CComDispatchDriver body(Script());
	_variant_t localId;
	hr = body.InvokeN(L"apiAddBinary", args, 4, &localId);
	if (FAILED(hr))
		return hr;
	// apiAddBinary incrementally adds dimensions for the new image.  Refresh
	// only the lists; OnBinaryChange rebuilds every image and is reserved for
	// edits to existing binary properties.
	hr = body.Invoke0(L"FillCoverList");
	if (SUCCEEDED(hr) && checkedId)
		*checkedId = localId;
	return hr;
}

// images
void CFBEView::AddImage(const CString& filename, bool bInline)
{
	ImageImportOptions options;
	options.outputFormat = static_cast<ImageOutputFormat>(_Settings.GetImageImportFormat());
	options.jpegQuality = static_cast<int>(_Settings.GetImageImportJpegQuality());
	options.keepSupportedImages = _Settings.GetImageImportKeepSupported();
	ImageImportResult imported;
	CString error;
	HRESULT hr = ImportImageForFb2(filename, options, imported, error);
	if (hr == E_ABORT) {
		if (::MessageBox(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.image_import.flatten_question", L"This image has transparency. Convert it to JPEG on a white background?"), FbeLoadRuntimeStringByKey(L"fbe.image_import.batch_title", L"Image import"), MB_YESNO | MB_ICONWARNING) != IDYES) return;
		options.flattenTransparentJpeg = true;
		hr = ImportImageForFb2(filename, options, imported, error);
	}
	if (FAILED(hr)) { if (!error.IsEmpty()) ::MessageBox(m_hWnd, error, L"FictionBook Editor", MB_OK | MB_ICONERROR); else U::ReportError(hr); return; }
	try
	{
		CComDispatchDriver body(Script());
		_variant_t checkedId;
		hr = AddImportedBinary(imported.data.data(), imported.data.size(), imported.logicalFileName, imported.mimeType, &checkedId);
		if (FAILED(hr)) { U::ReportError(hr); return; }

		_variant_t check(false);
		if (bInline)
			hr = body.Invoke2(L"InsInlineImage", &check, &checkedId);
		else
			hr = body.Invoke2(L"InsImage", &check, &checkedId);
		if (FAILED(hr))
			U::ReportError(hr);

		MSHTML::IHTMLDOMNodePtr node(NULL);
		if(node)
			BubbleUp(node, L"DIV");
	}
	catch (_com_error&) { }
}
