// FBEView.cpp : implementation of the CFBEView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "structure/BodyStructuralEditor.h"
#include "ReplacementPreflight.h"
#include "LinkNavigation.h"
#include "navigation/LinkDomNavigation.h"
#include "navigation/LinkNavigationState.h"
#include "navigation/ReferenceNavigation.h"
#include "ImageImport.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

#include "FBEView.h"
#include "SearchReplace.h"
#include "search\\SearchViewportPosition.h"
#include "search\\SearchViewportResults.h"
#include "Scintilla.h"
#include "ElementDescMnr.h"
#include "StartupTrace.h"
#include "RuntimeLocalization.h"
#include "table/TableGrid.h"
#include "table/TableStructuralEditor.h"
#include "view/VisualDomNormalizer.h"
#include "dom/MarkupUndoUnitScope.h"
#include "image/ImageDocumentInserter.h"
#include <vector>

using FbeTable::Grid;

class CSearchHighlightOverlay;
static void DestroySearchHighlightOverlay(CSearchHighlightOverlay* overlay);
extern const IID DIID_FBEHTMLElementEvents2 = __uuidof(MSHTML::HTMLElementEvents2);

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
static void NotifyTableStructureChanged(HWND frame, HWND view);


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

static UINT GetSearchWindowDpi(HWND window)
{
	typedef UINT (WINAPI* GetDpiForWindowProc)(HWND);
	HMODULE user32 = ::GetModuleHandle(L"user32.dll");
	GetDpiForWindowProc getDpiForWindow = user32
		? reinterpret_cast<GetDpiForWindowProc>(::GetProcAddress(user32, "GetDpiForWindow")) : NULL;
	if(getDpiForWindow) return getDpiForWindow(window);
	HDC dc = ::GetDC(window);
	const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSY)) : 96;
	if(dc) ::ReleaseDC(window, dc);
	return dpi ? dpi : 96;
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
static bool SelectTableCellRange(const MSHTML::IHTMLDocument2Ptr& document,
	const MSHTML::IHTMLElementPtr& firstCell, const MSHTML::IHTMLElementPtr& lastCell)
{
	try
	{
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		if (!body || !firstCell || !lastCell || FbeTable::FindTableElement(firstCell) != FbeTable::FindTableElement(lastCell)) return false;
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

LRESULT CFBEView::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /* unused: bHandled */)
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
	DestroySearchHighlightOverlay(m_search_highlight_overlay);
	m_search_highlight_overlay = NULL;
	if(HasDoc())
	{
		// Init can fail after acquiring the document but before all event sinks and
		// markup services are available. Teardown must be best-effort in that case.
		DocumentEvents::DispEventUnadvise(Document(), &DIID_HTMLDocumentEvents2);
		if (m_scroll_event_element)
			ScrollEvents::DispEventUnadvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
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
	::SendMessage(m_frame, AU::WM_DETACH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

LRESULT CFBEView::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
	// The highlight overlay is a separate non-activating owner popup so it is
	// not a layered child window (unsupported by Windows 7).  Resize events are
	// enough to reposition it without a permanent UI polling timer.
	RefreshSearchHighlights();
	return 0;
}

// Search highlighting is deliberately a native overlay.  Styling ranges through
// MSHTML would mutate the FB2 DOM, create undo entries and mark the document
// dirty merely for showing Find All results.
class CSearchHighlightOverlay : public CWindowImpl<CSearchHighlightOverlay>
{
public:
	DECLARE_WND_CLASS(L"FbeSearchHighlightOverlay")

	BEGIN_MSG_MAP(CSearchHighlightOverlay)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
	END_MSG_MAP()

	void Update(HWND parent, const std::vector<RECT>& documentRects, long scrollLeft,
		long scrollTop, std::size_t selected)
	{
		m_documentRects = documentRects;
		m_scrollLeft = scrollLeft;
		m_scrollTop = scrollTop;
		m_selected = selected;
		RECT client = {};
		::GetClientRect(parent, &client);
		POINT origin = { client.left, client.top };
		::ClientToScreen(parent, &origin);
		if (!m_hWnd)
		{
			// Layered *child* windows are unsupported on Windows 7.  An owned
			// popup is supported there and also remains above the hosted browser.
			Create(parent, client, NULL, WS_POPUP | WS_DISABLED,
				WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
			if (m_hWnd)
				::SetLayeredWindowAttributes(m_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		}
		if (m_hWnd)
		{
			// Keep the owned popup immediately above the hosted editor, but below
			// modeless Find/Results windows. HWND_TOP would paint hit rectangles over
			// those controls when an explicit Find All opens the Results pane.
			SetWindowPos(parent, origin.x, origin.y, client.right, client.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW);
			Invalidate();
		}
	}

	void UpdateScroll(long scrollLeft, long scrollTop)
	{
		if (m_scrollLeft == scrollLeft && m_scrollTop == scrollTop) return;
		m_scrollLeft = scrollLeft;
		m_scrollTop = scrollTop;
		if (m_hWnd) Invalidate();
	}

	void Clear()
	{
		m_documentRects.clear();
		if (m_hWnd) ShowWindow(SW_HIDE);
	}

	LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&)
	{
		CPaintDC dc(m_hWnd);
		RECT client = {};
		GetClientRect(&client);
		dc.FillSolidRect(&client, RGB(0, 0, 0)); // colour-keyed transparent
		HBRUSH selectedBrush = ::CreateSolidBrush(RGB(255, 128, 0));
		HBRUSH normalBrush = ::CreateSolidBrush(RGB(255, 215, 0));
		for (std::size_t index = 0; index < m_documentRects.size(); ++index)
		{
			RECT rect = m_documentRects[index];
			::OffsetRect(&rect, -m_scrollLeft, -m_scrollTop);
			if (rect.right <= rect.left) rect.right = rect.left + 1;
			if (rect.bottom <= rect.top) rect.bottom = rect.top + 1;
			::FrameRect(dc, &rect, index == m_selected ? selectedBrush : normalBrush);
		}
		::DeleteObject(normalBrush);
		::DeleteObject(selectedBrush);
		return 0;
	}

private:
	std::vector<RECT> m_documentRects;
	long m_scrollLeft = 0;
	long m_scrollTop = 0;
	std::size_t m_selected = static_cast<std::size_t>(-1);
};

static void DestroySearchHighlightOverlay(CSearchHighlightOverlay* overlay)
{
	delete overlay;
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

// Used by remaining view-owned commands.  SplitContainer resolves parents in
// BodyStructuralEditor so that its DOM algorithm has a single home.
static MSHTML::IHTMLElementPtr GetHP(MSHTML::IHTMLElementPtr hp)
{
	while((bool)hp && U::scmp(hp->tagName,L"DIV"))
		hp = hp->parentElement;
	return hp;
}

bool CFBEView::SplitContainer(bool fCheck)
{
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.SplitContainer(fCheck);
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

static bool IsStanza(MSHTML::IHTMLDOMNode *node) {
	MSHTML::IHTMLElementPtr   elem(node);
	return U::scmp(elem->className,L"stanza")==0;
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
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.InsertPoem(fCheck);
} // CFBEView::InsertPoem

bool CFBEView::InsertCite(bool fCheck)
{
	FbeStructure::BodyStructuralEditor editor(Document(), m_mk_srv);
	return editor.InsertCite(fCheck);
} // CFBEView::InsertCite

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
	// A command-bar click can make createRange() unavailable before the command
	// is routed.  The mouse handler records this exact cell, so prefer it before
	// querying that transient MSHTML selection state.
	if (m_table_selection_anchor) return m_table_selection_anchor;
	MSHTML::IHTMLTxtRangePtr range(Document()->selection->createRange());
	if (range) {
		range->collapse(VARIANT_TRUE);
		MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(range->parentElement()));
		if (cell) return cell;
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
	FbeVisualDom::NormalizeStructure(Document(), el);
    // fixup links
    FixupLinks(el);

    m_mk_srv->EndUndoUnit();
  }
  catch (_com_error& e) {
    U::ReportError(e);
  }
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
	m_last_search_error.Empty();
	if(m_fo.pattern.IsEmpty())
	{
		if(m_is_start)
			m_is_start->raw_select();
		return true;
	}

	NormalizeSearchPatternNbsp(m_fo.pattern);

	return m_fo.fRegexp ? DoSearchRegexp(fMore) : DoSearchStd(fMore);
}

bool CFBEView::DoSearchFromScopeStart()
{
	m_last_search_error.Empty();
	if (m_fo.pattern.IsEmpty())
		return true;
	NormalizeSearchPatternNbsp(m_fo.pattern);
	return DoSearchNative(true, m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal, true);
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
	PositionFoundRange(tr);
	m_fo.ClearMatch();
	m_fo.match = new AU::IMatch2(*rm);
	m_fo.hasMatch = true;
}

CString CFBEView::SearchResultStatus()
{
	const std::uint64_t version = SearchDocumentGeneration();
	const AU::Search::SearchResults& results = m_document_search.GetResults();
	if (!results.IsValidFor(version) ||
		results.GetSelected() == NULL)
		return CString();
	CString status;
	status.Format(L"%u / %u",
		static_cast<unsigned>(results.GetSelectedIndex() + 1),
		static_cast<unsigned>(results.GetCount()));
	return status;
}

CString CFBEView::FindAllResultStatus()
{
	const std::uint64_t version = SearchDocumentGeneration();
	const AU::Search::SearchResults& results = m_document_search.GetResults();
	if (!results.IsValidFor(version))
		return CString();
	CString status;
	status.Format(FbeLoadRuntimeStringByKey(L"fbe.search.results.found", L"%u found"),
		static_cast<unsigned>(results.GetCount()));
	return status;
}

std::size_t CFBEView::FindResultCount() const
{
	return m_document_search.GetResults().GetCount();
}

void CFBEView::SetFindResultsCompletionStatus(const CString& status)
{
	m_find_results_completion_status = status;
	::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

CString CFBEView::FindResultPreview(std::size_t index) const
{
	std::wstring preview;
	return m_document_search.GetResultPreview(index, &preview)
		? CString(preview.c_str()) : CString();
}

bool CFBEView::FindResultPreviewMatch(std::size_t index, std::size_t* start, std::size_t* length) const
{
	return start != NULL && length != NULL &&
		m_document_search.GetResultPreview(index, NULL, start, length);
}

bool CFBEView::AreFindResultsCurrent()
{
	return m_document_search.GetResults().IsValidFor(SearchDocumentGeneration());
}

std::uint64_t CFBEView::FindResultsRevision() const
{
	return m_document_search.GetResults().GetRevision();
}

bool CFBEView::SelectFindResult(std::size_t index)
{
	try
	{
		if (!Document() || !AreFindResultsCurrent())
			return false;
		if (m_document_search.SelectResult(Document(), SearchDocumentGeneration(), index) == NULL)
			return false;
		PositionFoundRange(MSHTML::IHTMLTxtRangePtr(Document()->selection->createRange()));
		RefreshSearchHighlights();
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

void CFBEView::ClearSearchHighlights()
{
	if (m_search_highlight_overlay)
		m_search_highlight_overlay->Clear();
}

void CFBEView::AdvanceSearchDocumentGeneration(bool refreshFindResultsPane)
{
	// Do this for every real dirty-range notification, including editor-owned
	// operations performed under m_ignore_changes. That flag suppresses the
	// application's dirty UI notification; it must never preserve stale search
	// offsets, replacement previews or highlight geometry.
	m_search_document_generation.Advance();
	m_document_search.Invalidate();
	m_has_find_scope_range = false;
	m_has_last_zero_length_hit = false;
	m_has_replace_preview = false;
	m_find_results_completion_status.Empty();
	ClearSearchHighlights();
	if (refreshFindResultsPane)
		::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

LRESULT CFBEView::OnFinalizeReplaceAllCompletion(UINT, WPARAM, LPARAM, BOOL&)
{
	if (!m_replace_all_completion_pending)
		return 0;

	// This posted turn follows the Replace All mutation and lets MSHTML deliver
	// queued RANGE_SINK notifications while the pending flag protects the pane.
	// Publish one final state, never an intermediate stale one.
	const int replaced = m_replace_all_completion_count;
	AdvanceSearchDocumentGeneration(false);
	CString completion;
	completion.Format(FbeLoadRuntimeStringByKey(L"fbe.replace.preview.completed", L"Replaced: %d"), replaced);
	m_find_results_completion_status = completion;
	m_replace_all_completion_pending = false;
	m_replace_all_completion_count = 0;
	::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
	return 0;
}

void CFBEView::UpdateSearchHighlightsForScroll()
{
	// The scroll event is post-scroll. Recompute a bounded viewport subset so
	// entering new content creates highlights without touching all Find All rows.
	RefreshSearchHighlights();
}

bool CFBEView::TryGetViewportSearchRange(std::size_t* start, std::size_t* end)
{
	if (start == NULL || end == NULL || !Document())
		return false;
	MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
	MSHTML::IHTMLBodyElementPtr body(Document()->body);
	if (!scrollElement || !body || scrollElement->clientHeight <= 0)
		return false;
	MSHTML::IHTMLTxtRangePtr topRange(body->createTextRange());
	MSHTML::IHTMLTxtRangePtr bottomRange(body->createTextRange());
	if (!topRange || !bottomRange)
		return false;
	const long probeX = (std::max)(1L, scrollElement->clientWidth / 2);
	const long bottomY = (std::max)(1L, scrollElement->clientHeight - 2);
	try
	{
		// Real viewport points avoid the left-edge ambiguity of elementFromPoint.
		topRange->moveToPoint(probeX, 1);
		bottomRange->moveToPoint(probeX, bottomY);
	}
	catch (const _com_error&)
	{
		// Old MSHTML engines occasionally reject moveToPoint over a control. Use
		// the same central probes only as a safe fallback, never x=0.
		MSHTML::IHTMLElementPtr topElement(Document()->elementFromPoint(probeX, 1));
		MSHTML::IHTMLElementPtr bottomElement(Document()->elementFromPoint(probeX, bottomY));
		if (!topElement || !bottomElement)
			return false;
		topRange->moveToElementText(topElement);
		bottomRange->moveToElementText(bottomElement);
	}
	const std::uint64_t generation = SearchDocumentGeneration();
	AU::Search::SearchRange topSearch = {}, bottomSearch = {};
	if (!m_document_search.TryGetSearchRange(generation, topRange, &topSearch) ||
		!m_document_search.TryGetSearchRange(generation, bottomRange, &bottomSearch))
		return false;
	*start = (std::min)(topSearch.Start, bottomSearch.Start);
	*end = (std::max)(topSearch.Start + topSearch.Length, bottomSearch.Start + bottomSearch.Length);
	return true;
}

void CFBEView::RefreshSearchHighlights()
{
	try
	{
		// Geometry extraction crosses the COM boundary and is particularly costly
		// for short/common queries.  Keep the Results pane and navigation complete,
		// but bound the visual overlay so Find All cannot monopolize the UI thread.
		const std::size_t maxOverlayRects = 128;
		if (!Document() || !AreFindResultsCurrent() || FindResultCount() == 0)
		{
			ClearSearchHighlights();
			return;
		}
		MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
		if (!scrollElement)
		{
			ClearSearchHighlights();
			return;
		}
		std::size_t viewportStart = 0, viewportEnd = 0;
		if (!TryGetViewportSearchRange(&viewportStart, &viewportEnd))
		{
			ClearSearchHighlights();
			return;
		}
		const std::uint64_t generation = SearchDocumentGeneration();
		const AU::Search::SearchResults& results = m_document_search.GetResults();
		const AU::Search::SearchViewportSubset subset = AU::Search::SelectViewportResults(results, viewportStart, viewportEnd, maxOverlayRects);
		std::vector<RECT> documentRects;
		std::size_t selectedRect = static_cast<std::size_t>(-1);
		for (std::size_t index = subset.FirstIndex, remaining = subset.Count; remaining > 0; ++index, --remaining)
		{
			MSHTML::IHTMLTxtRangePtr range;
			if (!m_document_search.CreateResultRange(Document(), generation, index, range) || !range)
				continue;
			MSHTML::IHTMLTextRangeMetrics2Ptr metrics(range);
			MSHTML::IHTMLRectPtr rect(metrics ? metrics->getBoundingClientRect() : MSHTML::IHTMLRectPtr());
			if (!rect || rect->right < rect->left || rect->bottom < rect->top)
				continue;
			RECT documentRect = { rect->left + scrollElement->scrollLeft, rect->top + scrollElement->scrollTop,
				rect->right + scrollElement->scrollLeft, rect->bottom + scrollElement->scrollTop };
			documentRects.push_back(documentRect);
			if (index == m_document_search.GetSelectedResultIndex())
				selectedRect = documentRects.size() - 1;
		}
		if (documentRects.empty())
		{
			ClearSearchHighlights();
			return;
		}
		if (!m_search_highlight_overlay)
			m_search_highlight_overlay = new CSearchHighlightOverlay();
		m_search_highlight_overlay->Update(m_hWnd, documentRects, scrollElement->scrollLeft, scrollElement->scrollTop,
			selectedRect);
	}
	catch (const _com_error&)
	{
		ClearSearchHighlights();
	}
}

void CFBEView::ShowFindResults()
{
	::SendMessage(m_frame, AU::WM_SHOW_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
}

void CFBEView::PositionFoundRange(MSHTML::IHTMLTxtRange* range)
{
	try
	{
		if(!range || !Document()) return;
		MSHTML::IHTMLTextRangeMetrics2Ptr rangeMetrics(range);
		MSHTML::IHTMLRectPtr matchRect(rangeMetrics ? rangeMetrics->getBoundingClientRect() : MSHTML::IHTMLRectPtr());
		MSHTML::IHTMLElement2Ptr scrollElement(MSHTML::IHTMLDocument3Ptr(Document())->documentElement);
		if(!matchRect || !scrollElement || scrollElement->clientHeight <= 0) return;

		RECT viewportPixels = {};
		::GetClientRect(m_hWnd, &viewportPixels);
		const long viewportWidth = viewportPixels.right - viewportPixels.left;
		const long viewportHeight = viewportPixels.bottom - viewportPixels.top;
		POINT viewportTopLeft = { viewportPixels.left, viewportPixels.top };
		POINT viewportBottomRight = { viewportPixels.right, viewportPixels.bottom };
		::ClientToScreen(m_hWnd, &viewportTopLeft);
		::ClientToScreen(m_hWnd, &viewportBottomRight);

		std::vector<FBESearchViewport::Rect> obstructions;
		HWND dialogs[] = { m_find_dlg ? m_find_dlg->m_hWnd : NULL, m_replace_dlg ? m_replace_dlg->m_hWnd : NULL };
		for(unsigned index = 0; index < _countof(dialogs); ++index)
		{
			RECT dialogBounds = {};
			if(!dialogs[index] || !::IsWindowVisible(dialogs[index]) || !::GetWindowRect(dialogs[index], &dialogBounds)) continue;
			if(dialogBounds.right <= viewportTopLeft.x || dialogBounds.left >= viewportBottomRight.x || dialogBounds.bottom <= viewportTopLeft.y || dialogBounds.top >= viewportBottomRight.y) continue;
			FBESearchViewport::Rect obstruction = {
				FBESearchViewport::ScaleToViewport(dialogBounds.left - viewportTopLeft.x, viewportWidth, scrollElement->clientWidth),
				FBESearchViewport::ScaleToViewport(dialogBounds.top - viewportTopLeft.y, viewportHeight, scrollElement->clientHeight),
				FBESearchViewport::ScaleToViewport(dialogBounds.right - viewportTopLeft.x, viewportWidth, scrollElement->clientWidth),
				FBESearchViewport::ScaleToViewport(dialogBounds.bottom - viewportTopLeft.y, viewportHeight, scrollElement->clientHeight) };
			obstructions.push_back(obstruction);
		}
		const UINT dpi = GetSearchWindowDpi(m_hWnd);
		FBESearchViewport::Rect match = { matchRect->left, matchRect->top, matchRect->right, matchRect->bottom };
		FBESearchViewport::PlacementInput input = {
			scrollElement->scrollTop, scrollElement->scrollHeight, scrollElement->clientHeight, match,
			FBESearchViewport::PreferredMatchTop(scrollElement->clientHeight, dpi),
			FBESearchViewport::MinimumContextTopForDpi(dpi) / 4,
			obstructions.empty() ? NULL : &obstructions[0], static_cast<unsigned>(obstructions.size()) };
		scrollElement->scrollTop = FBESearchViewport::ScrollTopForMatch(input);
	}
	catch(const _com_error&) { }
}

bool CFBEView::DoSearchRegexp(bool fMore)
{
	// Search Core owns matching for both Find and Replace.  The native hit is
	// adapted to IMatch2 by DoSearchNative only at the replacement-template
	// boundary below; Replace must never restore paragraph-by-paragraph search.
	return DoSearchNative(fMore, AU::Search::SearchMode::Regex);
}

bool CFBEView::DoSearchStd(bool fMore)
{
	// Literal Design-mode Find has completed parity checks against the former
	// MSHTML algorithm.  Keep MSHTML at the adapter boundary only: matching,
	// caret-relative navigation and wrap now all go through Search Core.
	return DoSearchNative(fMore, AU::Search::SearchMode::Literal);
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

// Replacing a range that spans paragraph elements with IHTMLTxtRange::text
// lets MSHTML rewrite block markup. Find may report such a regexp hit, but
// replacement is deliberately refused until a structural replacement engine
// can prove preservation of the surrounding FB2 DOM.
static CString CrossParagraphReplacementError()
{
	return FbeLoadRuntimeStringByKey(L"fbe.replace.cross_paragraph",
		L"Replacing a regular-expression match across paragraphs is not supported because it could change document structure.");
}

void  CFBEView::DoReplace() {
  try {
    MSHTML::IHTMLTxtRangePtr  sel(Document()->selection->createRange());
    if (!(bool)sel)
      return;
	if (CheckReplacementRange(sel, m_fo.fRegexp) == ReplacementPreflightResult::CrossParagraph)
	{
		::MessageBox(m_hWnd, CrossParagraphReplacementError(),
			FbeLoadRuntimeStringByKey(L"fbe.replace.preview.caption", L"Replace All"), MB_OK | MB_ICONEXCLAMATION);
		return;
	}
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

int CFBEView::ReplaceAllSearchCore(CString* errorText)
{
	if (errorText != NULL)
		errorText->Empty();
	const std::uint64_t generation = SearchDocumentGeneration();
	const auto previewIsCurrent = [&]() {
		return m_has_replace_preview &&
		m_replace_preview_generation == generation &&
		m_replace_preview_revision == FindResultsRevision() &&
		m_replace_preview_pattern == m_fo.pattern &&
		m_replace_preview_replacement == m_fo.replacement &&
		m_replace_preview_flags == m_fo.flags &&
		m_replace_preview_scope == m_fo.scope &&
		m_replace_preview_regexp == m_fo.fRegexp &&
		m_replace_preview_unicode_properties == m_fo.unicodeProperties &&
		AreFindResultsCurrent();
	};
	if (!previewIsCurrent())
	{
		CString searchError;
		if (!DoFindAll(true, &searchError))
		{
			if (errorText != NULL)
				*errorText = searchError;
			return -1;
		}
		const std::size_t previewCount = m_document_search.GetResults().GetCount();
		if (previewCount == 0)
			return 0;
		m_replace_preview_pattern = m_fo.pattern;
		m_replace_preview_replacement = m_fo.replacement;
		m_replace_preview_generation = generation;
		m_replace_preview_revision = FindResultsRevision();
		m_replace_preview_flags = m_fo.flags;
		m_replace_preview_scope = m_fo.scope;
		m_replace_preview_regexp = m_fo.fRegexp;
		m_replace_preview_unicode_properties = m_fo.unicodeProperties;
		m_has_replace_preview = true;
	}

	const std::size_t count = m_document_search.GetResults().GetCount();
	if (count == 0)
		return 0;
	CString preview;
	preview.Format(FbeLoadRuntimeStringByKey(
		L"fbe.replace.preview.message", L"Number of replacements: %Iu. Continue?"), count);
	if (::MessageBox(m_hWnd, preview, FbeLoadRuntimeStringByKey(
		L"fbe.replace.preview.caption", L"Replace All"), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return -2;
	// The confirmation is modal, so the preview normally stays unchanged. Still
	// validate its complete identity after the user answers: automation, browser
	// events, or another editor command must never apply stale coordinates.
	if (!previewIsCurrent())
	{
		if (errorText != NULL)
			*errorText = FbeLoadRuntimeStringByKey(L"fbe.replace.preview.changed", L"The document changed before Replace All could be applied.");
		return -1;
	}

	// Validate every source coordinate while the document is unchanged. The
	// reverse pass below then preserves all earlier offsets in this snapshot.
	std::vector<MSHTML::IHTMLTxtRangePtr> ranges(count);
	for (std::size_t index = 0; index < count; ++index)
	{
		if (!m_document_search.CreateResultRange(Document(), generation, index, ranges[index]) || !ranges[index])
		{
			if (errorText != NULL)
				*errorText = FbeLoadRuntimeStringByKey(L"fbe.replace.preview.changed", L"The document changed before Replace All could be applied.");
			return -1;
		}
		if (CheckReplacementRange(ranges[index], m_fo.fRegexp) == ReplacementPreflightResult::CrossParagraph)
		{
			if (errorText != NULL)
				*errorText = CrossParagraphReplacementError();
			return -1;
		}
	}

	int replaced = 0;
	bool mutationApplied = false;
	// MSHTML notifies RANGE_SINK for every individual assignment below.  Those
	// notifications are part of this one controlled operation; invalidate once
	// after EndUndoUnit instead of repeatedly clearing the completion status.
	m_controlled_replace_all_mutation = true;
	m_mk_srv->BeginUndoUnit(L"replace all");
	try
	{
		const AU::Search::SearchTextSnapshot& snapshot = m_document_search.GetSnapshot();
		for (std::size_t index = count; index-- > 0;)
		{
			const AU::Search::SearchResult* result = m_document_search.GetResults().GetAt(index);
			if (result == NULL)
				throw _com_error(E_FAIL);
			CString replacement;
			RRList formatting;
			if (m_fo.fRegexp)
			{
				const AU::Search::SearchHit& hit = result->Hit;
				AU::IMatch2 match(CString(snapshot.Text.data() + hit.Start, static_cast<int>(hit.Length)),
					static_cast<int>(hit.Start));
				for (std::size_t capture = 0; capture < hit.Captures.size(); ++capture)
				{
					const AU::Search::SearchCapture& value = hit.Captures[capture];
					match.AddSubMatch(value.Matched
						? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length))
						: CString());
				}
				replacement = PrepareRegexReplacementText(m_fo.replacement, &match, formatting);
			}
			else
			{
				replacement = m_fo.replacement;
				NormalizeReplacementNbsp(replacement);
			}
			ranges[index]->text = static_cast<LPCWSTR>(replacement);
			mutationApplied = true;
			if (m_fo.fRegexp)
				ApplyReplacementFormatting(ranges[index], replacement, formatting);
			++replaced;
		}
	}
	catch (const _com_error& error)
	{
		m_mk_srv->EndUndoUnit();
		m_controlled_replace_all_mutation = false;
		if (mutationApplied)
			AdvanceSearchDocumentGeneration();
		if (errorText != NULL)
			*errorText = error.ErrorMessage();
		return -1;
	}
	m_mk_srv->EndUndoUnit();
	m_fo.ClearMatch();
	m_has_replace_preview = false;
	if (replaced != 0)
	{
		m_replace_all_completion_count = replaced;
		m_replace_all_completion_pending = true;
		// Do not finalize synchronously: MSHTML may still dispatch RANGE_SINK
		// notifications after this method returns. A one-shot posted message gives
		// them one protected UI turn without a polling timer.
		if (!::PostMessage(m_hWnd, AU::WM_FINALIZE_REPLACE_ALL_COMPLETION, 0, 0))
		{
			BOOL handled = FALSE;
			OnFinalizeReplaceAllCompletion(AU::WM_FINALIZE_REPLACE_ALL_COMPLETION, 0, 0, handled);
		}
	}
	m_controlled_replace_all_mutation = false;
	return replaced;
}

int CFBEView::GlobalReplace(MSHTML::IHTMLElementPtr elem, CString cntTag)
{
	UNREFERENCED_PARAMETER(cntTag); // retained for the public Tools API signature
	if (m_fo.pattern.IsEmpty() || !Document())
		return 0;
	bool undoStarted = false;
	bool mutationApplied = false;
	try
	{
		const std::uint64_t generation = SearchDocumentGeneration();
		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Multiline = query.Mode == AU::Search::SearchMode::Regex;

		// GlobalReplace is also used by Tools commands with an element scope. Map
		// that DOM range once, then let Search Core filter snapshot hits; never
		// fall back to IHTMLTxtRange::findText for literal replacements.
		if (!m_document_search.Rebuild(Document(), generation, query))
			return 0;
		AU::Search::SearchRange scope;
		if (elem)
		{
			MSHTML::IHTMLTxtRangePtr elementRange(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
			if (!elementRange) return 0;
			elementRange->moveToElementText(elem);
			if (!m_document_search.TryGetSearchRange(generation, elementRange, &scope) || scope.Length == 0)
				return 0;
			if (!m_document_search.Rebuild(Document(), generation, query, NULL, &scope))
				return 0;
		}

		const std::size_t count = m_document_search.GetResults().GetCount();
		if (count == 0)
			return 0;
		std::vector<MSHTML::IHTMLTxtRangePtr> ranges(count);
		for (std::size_t index = 0; index < count; ++index)
			if (!m_document_search.CreateResultRange(Document(), generation, index, ranges[index]) || !ranges[index])
				return 0;

		int replaced = 0;
		m_mk_srv->BeginUndoUnit(L"replace");
		undoStarted = true;
		const AU::Search::SearchTextSnapshot& snapshot = m_document_search.GetSnapshot();
		for (std::size_t index = count; index-- > 0;)
		{
			const AU::Search::SearchResult* result = m_document_search.GetResults().GetAt(index);
			if (result == NULL) continue;
			CString replacement;
			RRList formatting;
			if (m_fo.fRegexp)
			{
				const AU::Search::SearchHit& hit = result->Hit;
				AU::IMatch2 match(CString(snapshot.Text.data() + hit.Start, static_cast<int>(hit.Length)), static_cast<int>(hit.Start));
				for (std::size_t capture = 0; capture < hit.Captures.size(); ++capture)
				{
					const AU::Search::SearchCapture& value = hit.Captures[capture];
					match.AddSubMatch(value.Matched ? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length)) : CString());
				}
				replacement = PrepareRegexReplacementText(m_fo.replacement, &match, formatting);
			}
			else
			{
				replacement = m_fo.replacement;
				NormalizeReplacementNbsp(replacement);
			}
			ranges[index]->text = static_cast<LPCWSTR>(replacement);
			mutationApplied = true;
			if (m_fo.fRegexp) ApplyReplacementFormatting(ranges[index], replacement, formatting);
			++replaced;
		}
		m_mk_srv->EndUndoUnit();
		undoStarted = false;
		if (mutationApplied)
			AdvanceSearchDocumentGeneration();
		return replaced;
	}
	catch (_com_error& err)
	{
		if (undoStarted) m_mk_srv->EndUndoUnit();
		if (mutationApplied)
			AdvanceSearchDocumentGeneration();
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
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
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
    CString error;
    int nRepl=m_view->ReplaceAllSearchCore(&error);
    if (nRepl>0) {
      SaveString();
      SaveHistory();
      U::MessageBox(MB_OK, IDS_REPL_ALL_CAPT, IDS_REPL_DONE_MSG, nRepl);
      MakeClose();
      m_selvalid=false;
	} else if (nRepl == -2) {
		return;
	} else if (!error.IsEmpty())
	{
		::MessageBox(m_hWnd, error, FbeLoadRuntimeStringByKey(L"fbe.replace.preview.caption", L"Replace All"), MB_OK | MB_ICONEXCLAMATION);
	} else
	{
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_END_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
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
	// Find and Replace share search criteria, including a stable Selection
	// scope.  A reopened Replace dialog may reuse that scope after Find Next
	// moved MSHTML's visible selection to a hit.
	const bool openingReplace = !m_replace_dlg || !m_replace_dlg->IsValid();
	if (openingReplace)
	{
		m_fo.ClearMatch();
		m_has_last_zero_length_hit = false;
		m_has_replace_preview = false;
	}
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
	if (!m_last_search_error.IsEmpty() && LastSearchErrorIsRegexp())
		::MessageBox(m_hWnd, m_last_search_error, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.caption", L"Find"), MB_OK | MB_ICONEXCLAMATION);
	else if (m_last_search_error.IsEmpty())
		U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_fo.pattern));
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
	// A controlled Replace All owns the invalidation and publishes its status
	// only after the complete Undo unit. Ordinary edits still invalidate here.
	if (!m_controlled_replace_all_mutation && !m_replace_all_completion_pending)
		AdvanceSearchDocumentGeneration();
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
	// Init is called after a full MSHTML/TransformXML rebuild.  Link navigation
	// origins are document-DOM scoped and must never survive that replacement.
	ClearLinkNavigationHistory();
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
	// A full hosted-document reload replaces every source coordinate even when
	// MSHTML happens to retain its internal version number.
	AdvanceSearchDocumentGeneration();

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

	// A document rebuild can invoke Init again while the old element is still
	// alive. Always detach that connection point before replacing its target.
	if (m_scroll_event_element)
	{
		ScrollEvents::DispEventUnadvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
		m_scroll_event_element = NULL;
	}

  DocumentEvents::DispEventUnadvise(document, &DIID_HTMLDocumentEvents2);
  hr = DocumentEvents::DispEventAdvise(document, &DIID_HTMLDocumentEvents2);
  StartupTrace::HResult(L"webbrowser", L"WB230", hr, L"DocumentEvents::DispEventAdvise");
  if (FAILED(hr)) return false;

	CComPtr<MSHTML::IHTMLDocument3> document3;
	document->QueryInterface(&document3);
	if (document3)
		m_scroll_event_element = document3->documentElement;
	else
		m_scroll_event_element = NULL;
	if (!m_scroll_event_element)
		m_scroll_event_element = body.p;
	hr = ScrollEvents::DispEventAdvise(m_scroll_event_element, &DIID_FBEHTMLElementEvents2);
	StartupTrace::HResult(L"webbrowser", L"WB235", hr, L"ScrollEvents::DispEventAdvise");
	if (FAILED(hr))
		m_scroll_event_element = NULL; // Highlight All degrades safely.

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
	FbeVisualDom::FixupParagraphs(body2);
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
void  CFBEView::OnBeforeNavigate(IDispatch */* unused: pDisp */,VARIANT *vtUrl,VARIANT */* unused: vtFlags */,
				 VARIANT */* unused: vtTargetFrame */,VARIANT */* unused: vtPostData */,
				 VARIANT */* unused: vtHeaders */,VARIANT_BOOL *fCancel)
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
void  CFBEView::OnSelChange(IDispatch */* unused: evt */) {
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
	MSHTML::IHTMLElementPtr contextCell(FbeTable::FindTableCell(MSHTML::IHTMLElementPtr(oe->srcElement)));
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

void CFBEView::OnScroll(IDispatch */* unused: evt */)
{
	// MSHTML emits this only after its hosted document has updated scrollLeft /
	// scrollTop, so popup coordinates are sampled from the final position.
	UpdateSearchHighlightsForScroll();
}

bool CFBEView::DoSearchNative(bool fMore, AU::Search::SearchMode mode, bool fromScopeStart)
{
	try
	{
		if (!Document())
			return false;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		if (!fMore && m_is_start)
			selection = m_is_start->duplicate();
		if (!selection)
			return false;

		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = mode;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = (m_fo.flags & FRF_REVERSE)
			? AU::Search::SearchDirection::Backward
			: AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Scope = m_fo.scope;
		// Legacy regexp searched paragraphs independently. The body snapshot
		// joins them with newlines, so PCRE2 multiline preserves ^/$ behaviour.
		query.Multiline = mode == AU::Search::SearchMode::Regex;

		const std::uint64_t generation = SearchDocumentGeneration();
		std::wstring nativeError;
		bool expressionError = false;
		m_last_search_error.Empty();
		m_last_search_error_is_regexp = false;
		if (!CanReuseDocumentSearch(query, generation) && !RebuildDocumentSearch(query, selection, &nativeError, &expressionError))
		{
			if (!nativeError.empty())
				m_last_search_error = nativeError.c_str();
			m_last_search_error_is_regexp = expressionError;
			return false;
		}
		const bool sameZeroLengthCriteria = m_has_last_zero_length_hit &&
			AU::Search::HasSameSearchCriteria(m_last_zero_length_query, query) &&
			m_last_zero_length_query.Direction == query.Direction;
		if (!sameZeroLengthCriteria)
			m_has_last_zero_length_hit = false;
		AU::Search::SearchRange selectedRange;
		const bool skipZeroLengthAtOffset = m_has_last_zero_length_hit &&
			m_last_zero_length_generation == generation &&
			m_document_search.TryGetSearchRange(generation, selection, &selectedRange) &&
			selectedRange.Length == 0 && selectedRange.Start == m_last_zero_length_hit;
		bool wrapped = false;
		const AU::Search::SearchHit* hit = fromScopeStart
			? m_document_search.SelectFromOffset(Document(), generation,
				m_has_find_scope_range ? m_find_scope_range.Start : 0,
				AU::Search::SearchDirection::Forward, &wrapped)
			: m_document_search.SelectFromRange(Document(), generation, selection, query.Direction, &wrapped, skipZeroLengthAtOffset);
		if (hit == NULL)
			return false;
		m_has_last_zero_length_hit = hit->Length == 0;
		m_last_zero_length_hit = hit->Start;
		m_last_zero_length_generation = generation;
		m_last_zero_length_query = query;
		if (mode != AU::Search::SearchMode::Regex)
			m_fo.ClearMatch();
		if (mode == AU::Search::SearchMode::Regex)
		{
			// Replace's formatting/template implementation still consumes IMatch2.
			// Adapt the native SearchHit here, at the editor boundary, retaining
			// positional empty captures so $1/$2 semantics remain intact.
			const AU::Search::SearchTextSnapshot& snapshot = m_document_search.GetSnapshot();
			m_fo.ClearMatch();
			m_fo.match = new AU::IMatch2(
				CString(snapshot.Text.data() + hit->Start, static_cast<int>(hit->Length)),
				static_cast<int>(hit->Start));
			for (std::size_t capture = 0; capture < hit->Captures.size(); ++capture)
			{
				const AU::Search::SearchCapture& value = hit->Captures[capture];
				m_fo.match->AddSubMatch(value.Matched
					? CString(snapshot.Text.data() + value.Start, static_cast<int>(value.Length))
					: CString());
			}
			m_fo.hasMatch = true;
		}
		MSHTML::IHTMLTxtRangePtr found(Document()->selection->createRange());
		PositionFoundRange(found);
		RefreshSearchHighlights();
		NotifyWrappedSearch(wrapped);
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

bool CFBEView::CanReuseDocumentSearch(const AU::Search::SearchQuery& query, std::uint64_t generation) const
{
	// Navigation direction only changes which cached hit is selected.  Rebuild
	// only when the document, matching criteria, or captured scope changed.
	if (!m_document_search.GetSession().IsValidFor(generation) ||
		!m_document_search.GetResults().IsValidFor(generation) ||
		!AU::Search::HasSameSearchCriteria(m_document_search.GetSession().GetQuery(), query))
		return false;
	return query.Scope == AU::Search::SearchScope::WholeDocument ||
		(m_has_find_scope_range && m_find_scope_generation == generation &&
			m_find_scope_kind == query.Scope);
}

bool CFBEView::DoFindAll(bool showResults, CString* errorText)
{
	try
	{
		m_find_results_completion_status.Empty();
		if (errorText != NULL)
			errorText->Empty();
		if (!Document())
			return false;
		AU::Search::SearchQuery query;
		query.Text = static_cast<LPCWSTR>(m_fo.pattern);
		query.Mode = m_fo.fRegexp ? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal;
		query.MatchCase = (m_fo.flags & FRF_CASE) != 0;
		query.WholeWord = (m_fo.flags & FRF_WHOLE) != 0;
		query.Direction = (m_fo.flags & FRF_REVERSE)
			? AU::Search::SearchDirection::Backward
			: AU::Search::SearchDirection::Forward;
		query.UnicodeProperties = m_fo.unicodeProperties;
		query.Scope = m_fo.scope;
		query.Multiline = query.Mode == AU::Search::SearchMode::Regex;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		std::wstring nativeError;
		if (!selection || !RebuildDocumentSearch(query, selection, &nativeError))
		{
			if (errorText != NULL)
				*errorText = nativeError.c_str();
			return false;
		}
		if (showResults)
			ShowFindResults();
		else
			::SendMessage(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE, reinterpret_cast<WPARAM>(this), 0);
		// Typing into Find invokes this path after a short debounce.  Computing a
		// rectangle for every hit is synchronous MSHTML work and can freeze the
		// editor for a common one-character query.  Highlighting remains available
		// through the explicit Find All action.
		if (showResults)
			RefreshSearchHighlights();
		else
			ClearSearchHighlights();
		return true;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

bool CFBEView::HasTextSelection()
{
	try
	{
		if (!Document())
			return false;
		MSHTML::IHTMLTxtRangePtr range(Document()->selection->createRange());
		return range && range->compareEndPoints(L"StartToEnd", range) != 0;
	}
	catch (const _com_error&)
	{
		return false;
	}
}

void CFBEView::ResetSearchScope()
{
	m_has_find_scope_range = false;
	m_find_scope_generation = 0;
}

bool CFBEView::RebuildDocumentSearch(const AU::Search::SearchQuery& query, MSHTML::IHTMLTxtRangePtr selection, std::wstring* errorText, bool* expressionError)
{
	if (expressionError != NULL)
		*expressionError = false;
	const std::uint64_t generation = SearchDocumentGeneration();
	if (!m_document_search.Rebuild(Document(), generation, query, errorText))
	{
		if (expressionError != NULL)
			*expressionError = query.Mode == AU::Search::SearchMode::Regex;
		return false;
	}
	if (query.Scope == AU::Search::SearchScope::WholeDocument)
		return true;

	AU::Search::SearchRange range;
	if (m_has_find_scope_range && m_find_scope_generation == generation && m_find_scope_kind == query.Scope)
	{
		range = m_find_scope_range;
	}
	else
	{
		MSHTML::IHTMLTxtRangePtr scopeSelection(selection);
		if (query.Scope == AU::Search::SearchScope::Selection)
		{
			if (!scopeSelection || scopeSelection->compareEndPoints(L"StartToEnd", scopeSelection) == 0)
			{
				if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.selection_scope_unavailable", L"The selected search scope is no longer available."));
				m_document_search.Invalidate();
				return false;
			}
		}
		else
		{
			MSHTML::IHTMLElementPtr section(SelectionStructSection());
			if (!section)
			{
				if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.current_section_unavailable", L"The current section is not available for search."));
				m_document_search.Invalidate();
				return false;
			}
			scopeSelection = MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange();
			scopeSelection->moveToElementText(section);
		}
		if (!m_document_search.TryGetSearchRange(generation, scopeSelection, &range) || range.Length == 0)
		{
			if (errorText != NULL) *errorText = static_cast<LPCWSTR>(FbeLoadRuntimeStringByKey(L"fbe.search.error.scope_mapping_failed", L"The selected search scope could not be mapped to the document."));
			m_document_search.Invalidate();
			return false;
		}
		m_find_scope_range = range;
		m_find_scope_generation = generation;
		m_find_scope_kind = query.Scope;
		m_has_find_scope_range = true;
	}
	return m_document_search.Rebuild(Document(), generation, query, errorText, &range);
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

static void HideNotePreview(CFBEView& view)
{
	try { CComDispatchDriver(view.Script()).Invoke0(L"HideNotePreview"); }
	catch(const _com_error&) { }
}

void CFBEView::ClearLinkNavigationHistory()
{
	m_link_navigation_state.Reset();
}

bool CFBEView::NavigateInternalLink(MSHTML::IHTMLElementPtr link, const CString& targetId)
{
	MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(Document(), targetId));
	if(!target) return false;
	m_link_navigation_state.targetId = targetId;
	m_link_navigation_state.originOrdinal = FBELinkNavigation::GetLinkTargetOrdinal(Document(), link, targetId);
	HideNotePreview(*this);
	GoTo(target);
	return true;
}

bool CFBEView::ReturnToLinkNavigationOrigin()
{
	if(!m_link_navigation_state.HasOrigin() || !Document()) return false;
	MSHTML::IHTMLElementPtr origin(FBELinkNavigation::FindOriginLink(
		Document(), m_link_navigation_state.targetId, m_link_navigation_state.originOrdinal));
	ClearLinkNavigationHistory();
	if(!origin) return false;
	GoTo(origin);
	return true;
}

VARIANT_BOOL  CFBEView::OnClick(IDispatch *evt)
{
	MSHTML::IHTMLEventObjPtr oe(evt);
	if(!oe) return VARIANT_FALSE;
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

	MSHTML::IHTMLElementPtr link = FBELinkNavigation::FindNearestLinkElement(
		elem, FBELinkNavigation::GetEditableBody(Document()));
	if(!link) return VARIANT_FALSE;
	CString href(AU::GetAttrCS(link, L"href"));
	CString documentUrl;
	try {
		MSHTML::IHTMLDocument4Ptr document4(Document());
		if(document4) documentUrl = static_cast<LPCWSTR>(document4->URLUnencoded);
	}
	catch(const _com_error&) { }
	const FBELinkNavigation::LinkActivation activation = FBELinkNavigation::DecideLinkActivation(
		static_cast<LPCWSTR>(href), static_cast<LPCWSTR>(documentUrl),
		oe->ctrlKey == VARIANT_TRUE, oe->altKey == VARIANT_TRUE, oe->shiftKey == VARIANT_TRUE);
	if(activation == FBELinkNavigation::LinkActivation::Ignore) return VARIANT_FALSE;
	if(activation == FBELinkNavigation::LinkActivation::Internal)
	{
		CString targetId(FBELinkNavigation::GetInternalLinkTargetId(Document(), link));
		// Accepted modifier clicks consume broken internal links too: MSHTML must not try a
		// browser fragment navigation after the target was known to be absent.
		NavigateInternalLink(link, targetId);
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	if(activation == FBELinkNavigation::LinkActivation::ExternalHttp)
	{
		HideNotePreview(*this);
		::ShellExecuteW(m_hWnd, L"open", static_cast<LPCWSTR>(href), NULL, NULL, SW_SHOWNORMAL);
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	if(activation == FBELinkNavigation::LinkActivation::Blocked)
	{
		oe->cancelBubble = VARIANT_TRUE;
		oe->returnValue = VARIANT_FALSE;
		return VARIANT_TRUE;
	}
	return VARIANT_FALSE;
}

VARIANT_BOOL CFBEView::OnMouseDown(IDispatch* evt)
{
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	if (!eventObject || eventObject->button != 1) return VARIANT_TRUE;
	m_table_selection_dragging = false;
	if (m_table_selection_anchor) m_table_selection_anchor.Release();
	UpdateTableCellHighlights(m_table_selection_cells, std::vector<MSHTML::IHTMLElementPtr>());
	MSHTML::IHTMLElementPtr source(eventObject->srcElement);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	// The document event signature returns VT_BOOL: VARIANT_FALSE cancels the
	// native MSHTML gesture.  Outside tables (and inside one cell) preserve the
	// editor's normal text selection, double-click and Shift-click behaviour.
	if (!cell) return VARIANT_TRUE;
	m_table_selection_anchor = cell;
	m_table_selection_dragging = true;
	return VARIANT_TRUE;
}

VARIANT_BOOL CFBEView::OnMouseMove(IDispatch* evt)
{
	if (!m_table_selection_dragging || !m_table_selection_anchor) return VARIANT_TRUE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	if (!eventObject) return VARIANT_TRUE;
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	if (!cell || cell == m_table_selection_anchor || !FbeTable::GetCellRectangle(m_table_selection_anchor, cell, cells) || !SelectTableCellRange(Document(), m_table_selection_anchor, cell)) return VARIANT_TRUE;
	UpdateTableCellHighlights(m_table_selection_cells, cells);
	eventObject->cancelBubble = VARIANT_TRUE;
	eventObject->returnValue = VARIANT_FALSE;
	return VARIANT_FALSE;
}

VARIANT_BOOL CFBEView::OnMouseUp(IDispatch* evt)
{
	if (!m_table_selection_dragging) return VARIANT_TRUE;
	MSHTML::IHTMLEventObjPtr eventObject(evt);
	MSHTML::IHTMLElementPtr source(eventObject ? eventObject->srcElement : NULL);
	MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(source));
	std::vector<MSHTML::IHTMLElementPtr> cells;
	bool tableSelectionHandled = !m_table_selection_cells.empty();
	if (cell && cell != m_table_selection_anchor && FbeTable::GetCellRectangle(m_table_selection_anchor, cell, cells) && SelectTableCellRange(Document(), m_table_selection_anchor, cell))
	{
		UpdateTableCellHighlights(m_table_selection_cells, cells);
		tableSelectionHandled = true;
	}
	m_table_selection_dragging = false;
	if (!tableSelectionHandled || !eventObject) return VARIANT_TRUE;
	eventObject->cancelBubble = VARIANT_TRUE;
	eventObject->returnValue = VARIANT_FALSE;
	return VARIANT_FALSE;
}

bool CFBEView::MoveTableCell(bool reverse)
{
	try
	{
		if (!HasDoc()) return false;
		MSHTML::IHTMLTxtRangePtr selection(Document()->selection->createRange());
		MSHTML::IHTMLElementPtr cell(FbeTable::FindTableCell(selection ? selection->parentElement() : MSHTML::IHTMLElementPtr()));
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		if (!cell || !row || !table) return false;

		std::vector<MSHTML::IHTMLElementPtr> cells;
		FbeTable::GetCells(table, cells);
		size_t index = 0;
		while (index < cells.size() && cells[index] != cell) ++index;
		if (index == cells.size()) return false;

		if (!reverse && index + 1 == cells.size())
		{
			FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row below");
			MSHTML::IHTMLElement2Ptr(row)->insertAdjacentElement(L"afterEnd", FbeTable::CreateRowLike(Document(), row));
			undo.Close();
			NotifyTableStructureChanged(m_frame, m_hWnd);
			FbeTable::GetCells(table, cells);
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
				U::MessageBox(MB_OK|MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_fo.pattern));
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
	const FBEReferenceNavigation::Resolution result =
		FBEReferenceNavigation::FindFootnoteTarget(Document(), !fCheck);
	if (!result.CanNavigate()) return false;
	if (fCheck) return true;
	if (!result.HasTarget()) return false;
	GoTo(result.element);
	result.scrollElement->scrollIntoView(VARIANT_TRUE);
	return true;
}
bool CFBEView::GoToReference(bool fCheck)
{
	const FBEReferenceNavigation::Resolution result =
		FBEReferenceNavigation::FindReferenceTarget(Document(), !fCheck);
	if (!result.CanNavigate()) return false;
	if (fCheck) return true;
	if (result.status == FBEReferenceNavigation::ResolutionStatus::NoReferences)
	{
		wchar_t cpt[MAX_LOAD_STRING + 1];
		wchar_t msg[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(), IDR_MAINFRAME, cpt, MAX_LOAD_STRING);
		FbeLoadString(_Module.GetResourceInstance(), IDS_GOTO_REF_FAIL_MSG, msg, MAX_LOAD_STRING);
		::MessageBox(::GetActiveWindow(), msg, cpt, MB_OK|MB_ICONINFORMATION);
		return false;
	}
	if (!result.HasTarget()) return false;
	GoTo(result.element);
	MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(Document()->body)->createTextRange());
	range->moveToElementText(result.element);
	range->collapse(VARIANT_TRUE);
	CString text = result.element->innerText;
	range->move(L"character", text.GetLength());
	range->select();
	MSHTML::IHTMLRectPtr rect = MSHTML::IHTMLElement2Ptr(result.element)->getBoundingClientRect();
	MSHTML::IHTMLWindow2Ptr window(MSHTML::IHTMLDocument2Ptr(Document())->parentWindow);
	if (rect && window)
	{
		if (rect->bottom - rect->top <= _Settings.GetViewHeight())
			window->scrollBy(0, (rect->top + rect->bottom - _Settings.GetViewHeight()) / 2);
		else
			window->scrollBy(0, rect->top);
	}
	// Preserve the historic action return value; command routing ignores it.
	return false;
}

LRESULT CFBEView::OnEditInsertTable(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */)
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
	::SendMessage(frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_SEL_CHANGE)), reinterpret_cast<LPARAM>(view));
	::SendMessage(frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_TREE_RESTORE)), 0);
}

LRESULT CFBEView::OnTableInsertRowAbove(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		Grid grid;
		if (!cell || !row || !FbeTable::BuildGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row above");
		const bool changed = FbeTable::InsertRow(Document(), grid, rowIndex, false, cell->tagName);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertRowBelow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(cell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(row));
		Grid grid;
		if (!cell || !row || !FbeTable::BuildGrid(table, grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table row below");
		const bool changed = FbeTable::InsertRow(Document(), grid, rowIndex, true, cell->tagName);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableDeleteRow(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr row(FbeTable::FindTableRow(SelectionStructTableCon()));
		Grid grid;
		if (!row || !row->parentElement || !FbeTable::BuildGrid(FbeTable::FindTableElement(row), grid)) return 0;
		long rowIndex = 0; while (rowIndex < static_cast<long>(grid.rows.size()) && grid.rows[rowIndex] != row) ++rowIndex;
		if (rowIndex == static_cast<long>(grid.rows.size())) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table row");
		const bool changed = FbeTable::DeleteRow(grid, rowIndex);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertColumnLeft(WORD, WORD, HWND, BOOL&)
{
	try { MSHTML::IHTMLElementPtr cell(SelectionStructTableCon()), row(FbeTable::FindTableRow(cell)), table(FbeTable::FindTableElement(row)); Grid grid; long index = FbeTable::BuildGrid(table, grid) ? FbeTable::FindCell(grid, cell) : -1; if (index >= 0) { FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table column left"); const bool changed = FbeTable::InsertColumn(Document(), grid, index, true, cell->tagName); undo.Close(); if (changed) NotifyTableStructureChanged(m_frame, m_hWnd); } }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableInsertColumnRight(WORD, WORD, HWND, BOOL&)
{
	try { MSHTML::IHTMLElementPtr cell(SelectionStructTableCon()), row(FbeTable::FindTableRow(cell)), table(FbeTable::FindTableElement(row)); Grid grid; long index = FbeTable::BuildGrid(table, grid) ? FbeTable::FindCell(grid, cell) : -1; if (index >= 0) { FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"insert table column right"); const bool changed = FbeTable::InsertColumn(Document(), grid, index, false, cell->tagName); undo.Close(); if (changed) NotifyTableStructureChanged(m_frame, m_hWnd); } }
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

bool CFBEView::DeleteTableLogicalColumnForTest(long column)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		Grid grid;
		if (!FbeTable::BuildGrid(table, grid)) return false;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table column");
		const bool changed = FbeTable::DeleteColumn(grid, column);
		undo.Close();
		if (!changed) return false;
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
		MSHTML::IHTMLElementPtr selectedRow(FbeTable::FindTableRow(selectedCell));
		MSHTML::IHTMLElementPtr table(FbeTable::FindTableElement(selectedRow));
		Grid grid;
		if (!selectedCell || !selectedRow || !FbeTable::BuildGrid(table, grid)) return 0;
		const long selectedIndex = FbeTable::FindCell(grid, selectedCell);
		if (selectedIndex < 0) return 0;
		FbeDom::MarkupUndoUnitScope undo(m_mk_srv, L"delete table column");
		const bool changed = FbeTable::DeleteColumn(grid, grid.cells[selectedIndex].startColumn);
		undo.Close();
		if (changed) NotifyTableStructureChanged(m_frame, m_hWnd);
	}
	catch (_com_error& error) { U::ReportError(error); }
	return 0;
}

LRESULT CFBEView::OnTableToggleHeaderCell(WORD, WORD, HWND, BOOL&)
{
	try
	{
		MSHTML::IHTMLElementPtr cell(SelectionStructTableCon());
		if (!cell || !FbeTable::FindTableElement(cell)) return 0;
		if (FbeTable::ToggleHeaderCell(Document(), cell)) NotifyTableStructureChanged(m_frame, m_hWnd);
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
		const std::vector<ImageImportFileType> imageTypes = ImageImportFileTypes();
		std::vector<COMDLG_FILTERSPEC> filters;
		filters.reserve(imageTypes.size());
		for (const ImageImportFileType& type : imageTypes)
			filters.push_back({ type.displayName.GetString(), type.wildcard.GetString() });

		wchar_t dlgTitle[MAX_LOAD_STRING + 1];
		const CString localizedTitle = FbeLoadRuntimeStringByKey(L"fbe.image.choose_file", L"Choose image");
		wcsncpy_s(dlgTitle, _countof(dlgTitle), localizedTitle.GetString(), _TRUNCATE);
		ModernFileDialog::Request request;
		request.fileMustExist = true; request.pathMustExist = true; request.title = dlgTitle;
		request.okButtonLabel = FbeLoadRuntimeStringByKey(L"fbe.image.open_button", L"Open").GetString();
		request.filters = filters.data(); request.filterCount = static_cast<UINT>(filters.size()); request.filterIndex = 1;
		const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(m_hWnd, request);
		if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
			StartupTrace::HResult(L"file-dialog", L"FD104", dialogResult.error, L"Insert image dialog");
		if(dialogResult.outcome == ModernFileDialog::Outcome::Accepted)
		{
			AddImage(dialogResult.paths.front().c_str(), bInline);
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
					FbeVisualDom::BubbleUp(node,L"DIV");
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
				MSHTML::IHTMLElementPtr cell(FbeTable::CreateCell(Document(), cellType));
				if (!firstCell) firstCell = cell;
				MSHTML::IHTMLElement2Ptr(tre)->insertAdjacentElement(L"beforeEnd", cell);
			}
			MSHTML::IHTMLElement2Ptr(tbody)->insertAdjacentElement(L"beforeEnd", tre);
		}

		// Unlike pasteHTML/InsertHTML, direct insertion is supported by this
		// MSHTML host and is captured by the surrounding undo unit.
		MSHTML::IHTMLElement2Ptr(anchor)->insertAdjacentElement(L"afterEnd", te);

		// * ensure we have good html
		FbeVisualDom::RelocateParagraphs(MSHTML::IHTMLDOMNodePtr(pe));
		FbeVisualDom::FixupParagraphs(pe);

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
		rng_begin->select();
		
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

	ClearSearchHighlights();
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

bool CFBEView::IsFindDialogOpen() const
{
	return m_find_dlg != NULL && m_find_dlg->IsValid();
}

bool CFBEView::IsReplaceDialogOpen() const
{
	return m_replace_dlg != NULL && m_replace_dlg->IsValid();
}

void CFBEView::SyncSearchOptionsToOpenDialogs(FRBase* source)
{
	if (m_find_dlg != NULL && m_find_dlg->IsValid() && m_find_dlg != source)
		m_find_dlg->SyncSearchOptionsFromView();
	if (m_replace_dlg != NULL && m_replace_dlg->IsValid() && m_replace_dlg != source)
		m_replace_dlg->SyncSearchOptionsFromView();
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

LRESULT CFBEView::OnCode(WORD /* unused: wCode */, WORD /* unused: wID */, HWND /* unused: hWnd */, BOOL& /* unused: bHandled */)
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

void CFBEView::ResetTableGridBuildCountForTest()
{
	FbeTable::GridDiagnostics::ResetBuildCount();
}

long CFBEView::TableGridBuildCountForTest()
{
	return FbeTable::GridDiagnostics::BuildCount();
}

bool CFBEView::SelectTableLogicalRangeForTest(long firstRow, long firstColumn, long lastRow, long lastColumn)
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		Grid grid;
		if (!body || !FbeTable::BuildGrid(table, grid)) return false;
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

CStringA CFBEView::TableStructuralSnapshot()
{
	try {
		MSHTML::IHTMLElementPtr body(Document() ? Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr table(tables && tables->length ? tables->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
		Grid grid;
		return FbeTable::BuildGrid(table, grid) ? FbeTable::BuildStructuralSnapshot(grid) : CStringA("invalid");
	}
	catch (_com_error&) { return CStringA("error"); }
}

static bool MakeSelectedTableCells(CFBEView* view, const wchar_t* targetName)
{
	MSHTML::IHTMLElementPtr currentCell(view->SelectionStructTableCon());
	std::vector<MSHTML::IHTMLElementPtr> cells;
	return FbeTable::GetSelectedCells(view->Document(), currentCell, cells) && FbeTable::ReplaceCells(view->Document(), cells, targetName);
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

HRESULT CFBEView::AddImportedBinary(const BYTE* bytes, size_t size, const CString& logicalFileName,
	const CString& mimeType, _variant_t* checkedId)
{
	FbeImage::ImageInsertionResult result;
	const HRESULT hr = FbeImage::AddImportedBinary(Script(), bytes, size, logicalFileName, mimeType, &result);
	if (SUCCEEDED(hr) && checkedId) *checkedId = result.binaryId;
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
		FbeImage::ImageInsertionResult result;
		hr = FbeImage::InsertImportedImage(Script(), imported.data.data(), imported.data.size(), imported.logicalFileName, imported.mimeType,
			bInline ? FbeImage::ImagePlacement::Inline : FbeImage::ImagePlacement::Block, &result);
		if (FAILED(hr))
			U::ReportError(hr);

		MSHTML::IHTMLDOMNodePtr node(result.insertedElement);
		if(!bInline && node)
			FbeVisualDom::BubbleUp(node, L"DIV");
	}
	catch (_com_error&) { }
}
