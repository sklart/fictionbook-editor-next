// FBEView.h : interface of the CFBEView class
//
/////////////////////////////////////////////////////////////////////////////

#include <atlcrack.h>
#if !defined(AFX_FBEVIEW_H__E0C71279_419D_4273_93E3_57F6A57C7CFE__INCLUDED_)
#define AFX_FBEVIEW_H__E0C71279_419D_4273_93E3_57F6A57C7CFE__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#pragma warning(disable : 4996)
#endif // _MSC_VER >= 1000

#include "resource.h"
#include "RuntimeLocalization.h"
#include "Settings.h"
#include "..\\common\\ModernFileDialog.h"
#include "StartupTrace.h"
#include "BinaryFileSave.h"
#include "BinarySaveNotification.h"
#include "search\\DocumentSearchCoordinator.h"
#include "search\\SearchDocumentGeneration.h"
#include "navigation\\LinkNavigationState.h"
#include "structure\\BodyStructuralEditor.h"

extern CSettings _Settings;


class CTableDlg : public CDialogImpl<CTableDlg>,
	public CWinDataExchange<CTableDlg> {
public:
	enum { IDD = IDD_TABLE };

	CButton	m_chekTitle;
	CEdit m_eRows;
	CUpDownCtrl m_udRows;
	CEdit m_eColumns;
	CUpDownCtrl m_udColumns;

	int m_nRows;
	int m_nColumns;
	bool m_bTitle;

	BEGIN_MSG_MAP(CTableDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		REFLECT_NOTIFICATIONS()//��������� ������� ���������� ��������� �� ��������
	END_MSG_MAP()

	//����� DDX ������
	BEGIN_DDX_MAP(CTableDlg)
		DDX_INT(IDC_EDIT_TABLE_ROWS, m_nRows)
		DDX_INT(IDC_EDIT_TABLE_COLUMNS, m_nColumns)
	END_DDX_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
		FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_TABLE);
		m_nRows = 1;
		m_nColumns = 2;
		m_bTitle = true;

		m_chekTitle	= GetDlgItem(IDC_CHECK_TABLE_TITLE);
		m_eRows		= GetDlgItem(IDC_EDIT_TABLE_ROWS);
		m_udRows	= GetDlgItem(IDC_SPIN_TABLE_ROWS);
		m_eColumns = GetDlgItem(IDC_EDIT_TABLE_COLUMNS);
		m_udColumns = GetDlgItem(IDC_SPIN_TABLE_COLUMNS);

		m_chekTitle.SetCheck(1);
		m_eRows.SetWindowText(_T("1"));
		m_eRows.SetSelAll(TRUE);
		m_eRows.SetFocus();

		m_udRows.SetRange(1, 1000);
		m_udRows.SetPos(1);
		m_eColumns.SetWindowText(_T("2"));
		m_udColumns.SetRange(1, 1000);
		m_udColumns.SetPos(2);

		return 0;
	}
	LRESULT OnOK(WORD, WORD wID, HWND, BOOL&) {
		if (!DoDataExchange(TRUE)) return 0;
		if (m_nRows < 1 || m_nColumns < 1) return 0;
		m_bTitle = false;
		if(m_chekTitle.GetCheck() == BST_CHECKED) {
			m_bTitle = true;
		}
		
		EndDialog(wID);
		return IDOK;
	}

	LRESULT OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
		EndDialog(wID);
		return IDCANCEL;
	}

};

static void CenterChildWindow(CWindow parent, CWindow child)
{
	RECT rcParent, rcChild;
	parent.GetWindowRect(&rcParent);
	child.GetWindowRect(&rcChild);
	int parentW = rcParent.right - rcParent.left;;
	int parentH = rcParent.bottom - rcParent.top;
	int childW = rcChild.right - rcChild.left;
	int childH = rcChild.bottom - rcChild.top;
	child.MoveWindow(rcParent.left + parentW/2 - childW/2, rcParent.top + parentH/2 - childH/2, childW, childH);
}

class CAddImageDlg : public CDialogImpl<CAddImageDlg>
{
public:
	enum { IDD = IDD_ADDIMAGE };
	BEGIN_MSG_MAP(CAddImageDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDYES, OnBtnClicked)
		COMMAND_ID_HANDLER(IDCANCEL, OnBtnClicked)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
	{
		FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_ADDIMAGE);
		::SetWindowText(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_addimage.caption", L"Image insertion"));
		::SetDlgItemText(m_hWnd, IDYES, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_addimage.yes", L"Insert"));
		::SetDlgItemText(m_hWnd, IDCANCEL, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_inputbox.cancel", L"Cancel"));
		::CenterChildWindow(GetParent(), m_hWnd);
		CButton btn = GetDlgItem(IDC_ADDIMAGE_ASKAGAIN);
		btn.SetCheck(!_Settings.GetInsImageAsking());
		return 0;
	}

	LRESULT OnBtnClicked(WORD, WORD wID, HWND, BOOL&)
	{
		_Settings.SetIsInsClearImage(wID == IDYES ? true : false);
		_Settings.SetInsImageAsking(!IsDlgButtonChecked(IDC_ADDIMAGE_ASKAGAIN));
		return EndDialog(wID);
	}
};

template<class T, int chgID>
class ATL_NO_VTABLE CHTMLChangeSink: public MSHTML::IHTMLChangeSink
{
protected:
public:
	// IUnknown
	STDMETHOD(QueryInterface)(REFIID iid,void **ppvObject)
	{
		if(iid == IID_IUnknown || iid == IID_IHTMLChangeSink)
		{
			*ppvObject = this;
			return S_OK;
		}

		return E_NOINTERFACE;
	}
	STDMETHOD_(ULONG, AddRef)() { return 1; }
	STDMETHOD_(ULONG, Release)() { return 1; }

	// IHTMLChangeSink
	STDMETHOD(raw_Notify)()
	{
		T* pT = static_cast<T*>(this);
		pT->EditorChanged(chgID);
		return S_OK;
	}
};

typedef CWinTraits<WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0>
		  CFBEViewWinTraits;

enum { FWD_SINK, BACK_SINK, RANGE_SINK };

class CFindDlgBase;
class FRBase;
namespace AU {
enum : UINT {
	WM_SHOW_FIND_RESULTS_PANE = WM_APP + 42,
	WM_HIDE_FIND_RESULTS_PANE = WM_APP + 43,
	WM_REFRESH_FIND_RESULTS_PANE = WM_APP + 44,
	WM_DETACH_FIND_RESULTS_PANE = WM_APP + 45,
	WM_FINALIZE_REPLACE_ALL_COMPLETION = WM_APP + 46
};
}
class CSearchHighlightOverlay;

// mshtml.tlb imports HTMLElementEvents2 without a named DIID constant.
// Give the ATL event sink a stable IID with external linkage.
extern const IID DIID_FBEHTMLElementEvents2;

class CFBEView : public CWindowImpl<CFBEView, CAxWindow, CFBEViewWinTraits>,
		 public IDispEventSimpleImpl<0, CFBEView, &DIID_DWebBrowserEvents2>,
		 public IDispEventSimpleImpl<0, CFBEView, &DIID_HTMLDocumentEvents2>,
		 public IDispEventSimpleImpl<1, CFBEView, &DIID_FBEHTMLElementEvents2>,
		 public IDispEventSimpleImpl<0, CFBEView, &DIID_HTMLTextContainerEvents2>,
		 public CHTMLChangeSink<CFBEView,RANGE_SINK>
{
protected:
  typedef IDispEventSimpleImpl<0, CFBEView, &DIID_DWebBrowserEvents2> BrowserEvents;
  typedef IDispEventSimpleImpl<0, CFBEView, &DIID_HTMLDocumentEvents2> DocumentEvents;
	  typedef IDispEventSimpleImpl<1, CFBEView, &DIID_FBEHTMLElementEvents2> ScrollEvents;
  typedef IDispEventSimpleImpl<0, CFBEView, &DIID_HTMLTextContainerEvents2> TextEvents;
  typedef CHTMLChangeSink<CFBEView,FWD_SINK>	  ForwardSink;
  typedef CHTMLChangeSink<CFBEView,BACK_SINK>	  BackwardSink;
  typedef CHTMLChangeSink<CFBEView,RANGE_SINK>	  RangeSink;

  HWND			    m_frame;
  const CString		   *m_document_filename;
  const bool			   *m_document_namevalid;

public:
  CString m_file_name, m_file_path;
  // changed by SeNS
  DWORD			    m_dirtyRangeCookie;
  MSHTML::IMarkupServices2Ptr  m_mk_srv;

protected:
  SHD::IWebBrowser2Ptr	    m_browser;
  MSHTML::IHTMLDocument2Ptr m_hdoc;
	MSHTML::IHTMLElementPtr m_scroll_event_element;
  MSHTML::IMarkupContainer2Ptr m_mkc;

  int			    m_ignore_changes;
  int			    m_enable_paste;

  bool			    m_normalize:1;
  bool			    m_complete:1;
  bool			    m_initialized:1;

  MSHTML::IHTMLElementPtr   m_cur_sel;
  MSHTML::IHTMLElementPtr   m_table_selection_anchor;
  std::vector<MSHTML::IHTMLElementPtr> m_table_selection_cells;
  bool                      m_table_selection_dragging;
  MSHTML::IHTMLInputTextElementPtr m_cur_input;
  _bstr_t		    m_cur_val;
  bool			    m_form_changed;
  bool			    m_form_cp;

  CString		    m_nav_url;
  CString           m_last_browser_event;
  ULONGLONG         m_navigation_started;
  bool              m_navigation_failed;
	long              m_navigation_status;
	FBELinkNavigation::LinkNavigationState m_link_navigation_state;

  static _ATL_FUNC_INFO DocumentCompleteInfo;
  static _ATL_FUNC_INFO BeforeNavigateInfo;
  static _ATL_FUNC_INFO NavigateErrorInfo;
  static _ATL_FUNC_INFO	EventInfo;
  static _ATL_FUNC_INFO	VoidEventInfo;
  static _ATL_FUNC_INFO VoidInfo;

	enum
	{
		FRF_REVERSE	= 1,
		FRF_WHOLE	= 2,
		FRF_CASE	= 4,
		FRF_REGEX	= 8,
		FRF_UNICODE_PROPERTIES = 16
	};

	struct FindReplaceOptions
	{
		CString		pattern;
		CString		replacement;
		AU::ReMatch	match;
		int			flags; // IHTMLTxtRange::findText() flags
		int			replNum;
		bool		hasMatch;
		bool		fRegexp;
		bool		unicodeProperties;
		AU::Search::SearchScope scope;

		FindReplaceOptions() : match(NULL), flags(0), replNum(0), hasMatch(false), fRegexp(false), unicodeProperties(false), scope(AU::Search::SearchScope::WholeDocument) { }
		~FindReplaceOptions() { ClearMatch(); }

		void ClearMatch()
		{
			delete match;
			match = NULL;
			hasMatch = false;
		}
	};

	FindReplaceOptions m_fo;
	CString m_last_search_error;
	bool m_last_search_error_is_regexp;
	MSHTML::IHTMLTxtRangePtr m_is_start;
	DocumentSearchCoordinator m_document_search;
	// Search offsets are semantic document coordinates. MSHTML's markup version
	// also changes for viewport and selection activity, so it must not validate
	// Search Core caches or Results-pane rows.
	AU::Search::SearchDocumentGeneration m_search_document_generation;
	AU::Search::SearchRange m_find_scope_range;
	std::uint64_t m_find_scope_generation;
	AU::Search::SearchScope m_find_scope_kind;
	bool m_has_find_scope_range;
	std::size_t m_last_zero_length_hit;
	std::uint64_t m_last_zero_length_generation;
	AU::Search::SearchQuery m_last_zero_length_query;
	bool m_has_last_zero_length_hit;
	CString m_replace_preview_pattern;
	CString m_replace_preview_replacement;
	std::uint64_t m_replace_preview_generation;
	std::uint64_t m_replace_preview_revision;
	int m_replace_preview_flags;
	AU::Search::SearchScope m_replace_preview_scope;
	bool m_replace_preview_regexp;
	bool m_replace_preview_unicode_properties;
	bool m_has_replace_preview;
	// MSHTML raises RANGE_SINK synchronously for each range->text assignment.
	// Replace All coalesces those notifications into one semantic invalidation
	// after its Undo unit has closed, so its completion status cannot be
	// overwritten by an intermediate stale refresh.
	bool m_controlled_replace_all_mutation;
	// MSHTML can queue a final range notification after ReplaceAllSearchCore
	// returns. Keep the operation pending until one posted UI turn finalizes the
	// invalidation and publishes the completion status.
	bool m_replace_all_completion_pending;
	int m_replace_all_completion_count;
	// A completed Replace All invalidates snapshot offsets. Keep a short
	// presentation-only result so an open Results pane does not call that
	// successful operation "stale".
	CString m_find_results_completion_status;
	CSearchHighlightOverlay* m_search_highlight_overlay;

	struct pElAdjacent
	{
		MSHTML::IHTMLElementPtr elem;
		_bstr_t innerText;

		pElAdjacent(MSHTML::IHTMLElementPtr pElem) : elem(pElem), innerText(pElem->innerText)
		{
		}
	};

	friend class CFindDlgBase;
	friend class FRBase;
	friend class CViewFindDlg;
	friend class CReplaceDlgBase;
	friend class CViewReplaceDlg;
	friend class CSciFindDlg;
	friend class CSciReplaceDlg;
	friend class FRBase;

	int TextOffset(MSHTML::IHTMLTxtRange *rng, AU::ReMatch rm, CString txt = L"", CString htmlTxt = L"");

	void SelMatch(MSHTML::IHTMLTxtRange* tr, AU::ReMatch rm);
	void PositionFoundRange(MSHTML::IHTMLTxtRange* range);
	bool DoSearchNative(bool fMore, AU::Search::SearchMode mode, bool fromScopeStart = false);
	bool CanReuseDocumentSearch(const AU::Search::SearchQuery& query, std::uint64_t generation) const;
	bool RebuildDocumentSearch(const AU::Search::SearchQuery& query, MSHTML::IHTMLTxtRangePtr selection, std::wstring* errorText = NULL, bool* expressionError = NULL);
	void AdvanceSearchDocumentGeneration(bool refreshFindResultsPane = true);
	std::uint64_t SearchDocumentGeneration() const { return m_search_document_generation.Value(); }
	bool HasSavedSearchScope() const { return m_has_find_scope_range && m_find_scope_generation == SearchDocumentGeneration(); }
	void RefreshSearchHighlights();
	void ClearSearchHighlights();
	void UpdateSearchHighlightsForScroll();
	bool TryGetViewportSearchRange(std::size_t* start, std::size_t* end);
	bool HasTextSelection();
	void ResetSearchScope();
	MSHTML::IHTMLElementPtr SelectionContainerImp();

public:
	CFindDlgBase*			m_find_dlg;
	CReplaceDlgBase*		m_replace_dlg;

	SHD::IWebBrowser2Ptr	Browser()
	{
		return m_browser;
	}

	MSHTML::IHTMLDocument2Ptr Document()
	{
		return m_hdoc;
	}
	MSHTML::IMarkupServices2Ptr MarkupServices() { return m_mk_srv; }

  bool			    HasDoc() { return m_hdoc; }
  IDispatchPtr	    Script(){ return MSHTML::IHTMLDocumentPtr(m_hdoc)->Script; }
  CString		    NavURL() { return m_nav_url; }
  CString           LastBrowserEvent() { return m_last_browser_event; }
  void              BeginNavigationTrace() { m_navigation_started = ::GetTickCount64(); m_last_browser_event = L"Navigate"; m_navigation_failed = false; m_navigation_status = 0; }
  bool              NavigationFailed() const { return m_navigation_failed; }
  long              NavigationStatus() const { return m_navigation_status; }

  bool			    Loaded() { bool cmp=m_complete; m_complete=false; return cmp; }
  bool			    Init();

	long			    GetVersionNumber() { return m_mkc ? m_mkc->GetVersionNumber() : -1; }
	const CString& LastSearchError() const { return m_last_search_error; }
	bool LastSearchErrorIsRegexp() const { return m_last_search_error_is_regexp; }

  void			    BeginUndoUnit(const wchar_t *name) 
  { 
	  m_mk_srv->BeginUndoUnit((wchar_t *)name); 
  }
  void			    EndUndoUnit() 
  { 
	  m_mk_srv->EndUndoUnit();
  }

  DECLARE_WND_SUPERCLASS(NULL, CAxWindow::GetWndClassName())

  CFBEView(HWND frame, bool fNorm) : m_frame(frame), m_document_filename(NULL), m_document_namevalid(NULL), m_dirtyRangeCookie(0), m_ignore_changes(0), m_enable_paste(0),
	 m_normalize(fNorm), m_complete(false), m_initialized(false), m_startMatch(0), m_endMatch(0),
	 m_form_changed(false), m_form_cp(false), m_table_selection_dragging(false), m_last_browser_event(L"none"), m_navigation_started(0), m_navigation_failed(false), m_navigation_status(0), m_find_dlg(0), m_replace_dlg(0), m_last_search_error_is_regexp(false), m_find_scope_generation(0), m_find_scope_kind(AU::Search::SearchScope::WholeDocument), m_has_find_scope_range(false), m_last_zero_length_hit(0), m_last_zero_length_generation(0), m_has_last_zero_length_hit(false), m_replace_preview_generation(0), m_replace_preview_revision(0), m_replace_preview_flags(0), m_replace_preview_scope(AU::Search::SearchScope::WholeDocument), m_replace_preview_regexp(false), m_replace_preview_unicode_properties(false), m_has_replace_preview(false), m_controlled_replace_all_mutation(false), m_replace_all_completion_pending(false), m_replace_all_completion_count(0), m_search_highlight_overlay(NULL), m_file_path(), m_file_name() { }
  ~CFBEView();

  BOOL PreTranslateMessage(MSG* pMsg);

  BEGIN_MSG_MAP(CFBEView)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(AU::WM_FINALIZE_REPLACE_ALL_COMPLETION, OnFinalizeReplaceAllCompletion)

    // editing commands
    COMMAND_ID_HANDLER(ID_EDIT_UNDO, OnUndo)
    COMMAND_ID_HANDLER(ID_EDIT_REDO, OnRedo)
    COMMAND_ID_HANDLER(ID_EDIT_CUT, OnCut)
    COMMAND_ID_HANDLER(ID_EDIT_COPY, OnCopy)
    COMMAND_ID_HANDLER(ID_EDIT_PASTE, OnPaste)
	COMMAND_ID_HANDLER(ID_EDIT_PASTE2, OnPaste)
    COMMAND_ID_HANDLER(ID_EDIT_BOLD, OnBold)
    COMMAND_ID_HANDLER(ID_EDIT_ITALIC, OnItalic)
    COMMAND_ID_HANDLER(ID_EDIT_FIND, OnFind)
    COMMAND_ID_HANDLER(ID_EDIT_FINDNEXT, OnFindNext)
    COMMAND_ID_HANDLER(ID_EDIT_REPLACE, OnReplace)
	COMMAND_ID_HANDLER(ID_EDIT_STRIK, OnStrik)
	COMMAND_ID_HANDLER(ID_EDIT_SUP, OnSup)
	COMMAND_ID_HANDLER(ID_EDIT_SUB, OnSub)
	COMMAND_ID_HANDLER(ID_EDIT_CODE, OnCode)

    COMMAND_ID_HANDLER(ID_STYLE_LINK, OnStyleLink)
    COMMAND_ID_HANDLER(ID_STYLE_NOTE, OnStyleFootnote)
    COMMAND_ID_HANDLER(ID_STYLE_NOLINK, OnStyleNolink)

    COMMAND_ID_HANDLER(ID_STYLE_NORMAL, OnStyleNormal)
    COMMAND_ID_HANDLER(ID_STYLE_TEXTAUTHOR, OnStyleTextAuthor)
    COMMAND_ID_HANDLER(ID_STYLE_SUBTITLE, OnStyleSubtitle)

    COMMAND_ID_HANDLER(ID_EDIT_ADD_TITLE, OnEditAddTitle)
    COMMAND_ID_HANDLER(ID_EDIT_ADD_BODY, OnEditAddBody)
    COMMAND_ID_HANDLER(ID_EDIT_ADD_EPIGRAPH, OnEditAddEpigraph)
    COMMAND_ID_HANDLER(ID_EDIT_ADD_TA, OnEditAddTA)
    COMMAND_ID_HANDLER(ID_EDIT_CLONE, OnEditClone)
    COMMAND_ID_HANDLER(ID_EDIT_ADD_IMAGE, OnEditAddImage)
    COMMAND_ID_HANDLER(ID_EDIT_ADD_ANN,OnEditAddAnn)
	COMMAND_ID_HANDLER(ID_EDIT_INS_IMAGE, OnEditInsImage)
	COMMAND_ID_HANDLER(ID_EDIT_INS_INLINEIMAGE, OnEditInsImage)

    COMMAND_ID_HANDLER(ID_EDIT_SPLIT, OnEditSplit)
    COMMAND_ID_HANDLER(ID_EDIT_MERGE, OnEditMerge)
    COMMAND_ID_HANDLER(ID_EDIT_REMOVE_OUTER_SECTION, OnEditRemoveOuter)

    COMMAND_ID_HANDLER(ID_EDIT_INS_POEM, OnEditInsPoem)
    COMMAND_ID_HANDLER(ID_EDIT_INS_CITE, OnEditInsCite)
	COMMAND_ID_HANDLER_EX(ID_INSERT_TABLE, OnEditInsertTable)
	COMMAND_ID_HANDLER(ID_TABLE_INSERT_ROW_ABOVE, OnTableInsertRowAbove)
	COMMAND_ID_HANDLER(ID_TABLE_INSERT_ROW_BELOW, OnTableInsertRowBelow)
	COMMAND_ID_HANDLER(ID_TABLE_DELETE_ROW, OnTableDeleteRow)
	COMMAND_ID_HANDLER(ID_TABLE_INSERT_COLUMN_LEFT, OnTableInsertColumnLeft)
	COMMAND_ID_HANDLER(ID_TABLE_INSERT_COLUMN_RIGHT, OnTableInsertColumnRight)
	COMMAND_ID_HANDLER(ID_TABLE_DELETE_COLUMN, OnTableDeleteColumn)
	COMMAND_ID_HANDLER(ID_TABLE_TOGGLE_HEADER_CELL, OnTableToggleHeaderCell)
	COMMAND_ID_HANDLER(ID_TABLE_MAKE_HEADER_CELLS, OnTableMakeHeaderCells)
	COMMAND_ID_HANDLER(ID_TABLE_MAKE_NORMAL_CELLS, OnTableMakeNormalCells)

    COMMAND_ID_HANDLER(ID_VIEW_HTML, OnViewHTML)
	COMMAND_ID_HANDLER(ID_SAVEIMG_AS, OnSaveImageAs)
    COMMAND_RANGE_HANDLER(ID_SEL_BASE,ID_SEL_BASE+99, OnSelectElement)
  END_MSG_MAP()

	BEGIN_SINK_MAP(CFBEView)
		SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, OnDocumentComplete, &DocumentCompleteInfo)
		SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, OnBeforeNavigate, &BeforeNavigateInfo)
		SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR, OnNavigateError, &NavigateErrorInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONSELECTIONCHANGE, OnSelChange, &VoidEventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONCONTEXTMENU, OnContextMenu, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONCLICK, OnClick, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONMOUSEDOWN, OnMouseDown, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONMOUSEMOVE, OnMouseMove, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONMOUSEUP, OnMouseUp, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONKEYDOWN, OnKeyDown, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLDocumentEvents2, DISPID_HTMLDOCUMENTEVENTS2_ONFOCUSIN, OnFocusIn, &VoidEventInfo)
		SINK_ENTRY_INFO(1, DIID_FBEHTMLElementEvents2, DISPID_HTMLELEMENTEVENTS2_ONSCROLL, OnScroll, &VoidEventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLTextContainerEvents2, DISPID_HTMLELEMENTEVENTS2_ONPASTE, OnRealPaste, &EventInfo)
		SINK_ENTRY_INFO(0, DIID_HTMLTextContainerEvents2, DISPID_HTMLELEMENTEVENTS2_ONDRAGEND, OnDrop, &VoidEventInfo)
	END_SINK_MAP()

  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnFinalizeReplaceAllCompletion(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnFocus(UINT, WPARAM, LPARAM, BOOL&) 
  {
    // pass to document
    if (HasDoc())
      MSHTML::IHTMLDocument4Ptr(Document())->focus();
    return 0;
  }

	// editing commands
	LRESULT ExecCommand(int cmd);
	void QueryStatus(OLECMD *cmd, int ncmd);
	CString QueryCmdText(int cmd);

	LRESULT OnUndo(WORD, WORD, HWND /* unused: hWnd */, BOOL&)
	{ 
		LRESULT res = ExecCommand(IDM_UNDO);
		// update tree view
		::SendMessage(m_frame, WM_COMMAND, static_cast<WPARAM>(MAKELONG(0, IDN_TREE_RESTORE)), 0);
		return res;
	}
	LRESULT OnRedo(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_REDO); }
	LRESULT OnCut(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_CUT); }
	LRESULT OnCopy(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_COPY); }
	LRESULT OnPaste(WORD, WORD, HWND, BOOL&);
	LRESULT OnBold(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_BOLD); }
	LRESULT OnItalic(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_ITALIC); }
	LRESULT OnStrik(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_STRIKETHROUGH); }
	LRESULT OnSup(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_SUPERSCRIPT); }
	LRESULT OnSub(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_SUBSCRIPT); }
	LRESULT OnCode(WORD, WORD, HWND, BOOL&);

  LRESULT OnFind(WORD, WORD, HWND, BOOL&);
  LRESULT OnFindNext(WORD, WORD, HWND, BOOL&);
  LRESULT OnReplace(WORD, WORD, HWND, BOOL&);
  LRESULT OnStyleLink(WORD, WORD, HWND, BOOL&);
  LRESULT OnStyleFootnote(WORD, WORD, HWND, BOOL&);
  LRESULT OnStyleNolink(WORD, WORD, HWND, BOOL&) { return ExecCommand(IDM_UNLINK); }
  LRESULT OnStyleNormal(WORD, WORD, HWND, BOOL&)
  {
	  BeginUndoUnit(L"normal style");
	  U::ChangeAttribute(SelectionStructCon(), L"class", L"normal");
	  EndUndoUnit();

	  return 0;
  }
  LRESULT OnStyleTextAuthor(WORD, WORD, HWND, BOOL&) { Call(L"StyleTextAuthor",SelectionStructCon()); return 0; }
  LRESULT OnStyleSubtitle(WORD, WORD, HWND, BOOL&) { Call(L"StyleSubtitle",SelectionStructCon()); return 0; }
  LRESULT OnViewHTML(WORD, WORD, HWND, BOOL&) {
    IOleCommandTargetPtr  ct(m_browser);
    if (ct)
      ct->Exec(&CGID_MSHTML, IDM_VIEWSOURCE, 0, NULL, NULL);
    return 0;
  }
	LRESULT OnSelectElement(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditAddTitle(WORD, WORD, HWND, BOOL&)	{ Call(L"AddTitle", SelectionStructCon()); return 0; }
	LRESULT OnEditAddEpigraph(WORD, WORD, HWND, BOOL&) { Call(L"AddEpigraph", SelectionStructCon()); return 0; }
	LRESULT OnEditAddBody(WORD, WORD, HWND, BOOL&) { Call(L"AddBody"); return 0; }
	LRESULT OnEditAddTA(WORD, WORD, HWND, BOOL&) { Call(L"AddTA",SelectionStructCon()); return 0; }
	LRESULT OnEditClone(WORD, WORD, HWND, BOOL&) { Call(L"CloneContainer",SelectionStructCon()); return 0; }
	LRESULT OnEditAddImage(WORD, WORD, HWND, BOOL&) { Call(L"AddImage", SelectionStructCon()); return 0; }
	LRESULT OnEditInsImage(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditInsInlineImage(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditAddAnn(WORD, WORD, HWND, BOOL&) { Call(L"AddAnnotation", SelectionStructCon()); return 0; }
	LRESULT OnEditMerge(WORD, WORD, HWND, BOOL&) { Call(L"MergeContainers", SelectionStructCon()); return 0; }
	LRESULT OnEditSplit(WORD, WORD, HWND, BOOL&) { SplitContainer(false); return 0; }
	LRESULT OnEditInsPoem(WORD, WORD, HWND, BOOL&) { InsertPoem(false); return 0; }
	LRESULT OnEditInsCite(WORD, WORD, HWND, BOOL&) { InsertCite(false); return 0; }
	LRESULT OnEditRemoveOuter(WORD, WORD, HWND, BOOL&) { Call(L"RemoveOuterContainer",SelectionStructCon()); return 0; }
	LRESULT OnSaveImageAs(WORD, WORD, HWND, BOOL&)
	{
		CString src;
		MSHTML::IHTMLImgElementPtr image = MSHTML::IHTMLDOMNodePtr(SelectionContainer())->firstChild;
		src = image->src.GetBSTR();
		src.Delete(src.Find(L"fbw-internal:#"), 14);

		_variant_t data;
		try
		{
			CComDispatchDriver dd(Script());
			_variant_t arg(src);
			if(!SUCCEEDED(dd.Invoke1(L"GetImageData", &arg, &data)) || data.vt == VT_EMPTY)
				return 0;
		}
		catch (_com_error&)
		{
			return 0;
		}

		const COMDLG_FILTERSPEC filters[] = { { L"JPEG files (*.jpg)", L"*.jpg" }, { L"PNG files (*.png)", L"*.png" }, { L"All files (*.*)", L"*.*" } };
		ModernFileDialog::Request request;
		request.save = true; request.pathMustExist = true; request.overwritePrompt = true;
		request.defaultExtension = L"jpg"; request.initialFileName = src.GetString();
		request.initialFolder = m_file_path.GetString(); request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = 1;
		const ModernFileDialog::Result dialogResult = ModernFileDialog::Show(m_hWnd, request);
		if (dialogResult.outcome == ModernFileDialog::Outcome::Failed)
			StartupTrace::HResult(L"file-dialog", L"FD105", dialogResult.error, L"Save image dialog");
		if(dialogResult.outcome == ModernFileDialog::Outcome::Accepted)
		{
			const CString outputPath(dialogResult.paths.front().c_str());
			long lowerBound = 0, upperBound = -1;
			void* bytes = NULL;
			if ((data.vt & VT_ARRAY) && data.parray != NULL &&
				::SafeArrayGetLBound(data.parray, 1, &lowerBound) == S_OK &&
				::SafeArrayGetUBound(data.parray, 1, &upperBound) == S_OK &&
				upperBound >= lowerBound && ::SafeArrayAccessData(data.parray, &bytes) == S_OK)
			{
				const DWORD byteCount = static_cast<DWORD>(upperBound - lowerBound + 1);
				DWORD error = ERROR_SUCCESS;
				if (!BinaryFileSave::WriteAtomically(outputPath, bytes, byteCount,
					BinaryFileSave::ExistingFilePolicy::ReplaceExisting, &error))
				{
					CString message;
					message.Format(L"OnSaveImageAs failed (error %lu)", error);
					StartupTrace::Error(L"binary-save", L"B511", message);
					ShowBinarySaveFailure(m_hWnd, outputPath, error);
				}
				::SafeArrayUnaccessData(data.parray);
			}
			else
				StartupTrace::Error(L"binary-save", L"B512", L"OnSaveImageAs received invalid binary data");
		}

		return 0;
	}

  // Modification by Pilgrim
  LRESULT OnEditInsertTable(WORD wNotifyCode, WORD wID, HWND hWndCtl);
  LRESULT OnTableInsertRowAbove(WORD, WORD, HWND, BOOL&);
  LRESULT OnTableInsertRowBelow(WORD, WORD, HWND, BOOL&);
  LRESULT OnTableDeleteRow(WORD, WORD, HWND, BOOL&);
  LRESULT OnTableInsertColumnLeft(WORD, WORD, HWND, BOOL&);
  LRESULT OnTableInsertColumnRight(WORD, WORD, HWND, BOOL&);
  LRESULT OnTableDeleteColumn(WORD, WORD, HWND, BOOL&);
	LRESULT OnTableToggleHeaderCell(WORD, WORD, HWND, BOOL&);
	LRESULT OnTableMakeHeaderCells(WORD, WORD, HWND, BOOL&);
	LRESULT OnTableMakeNormalCells(WORD, WORD, HWND, BOOL&);

  bool	CheckCommand(WORD wID);
  bool	CheckSetCommand(WORD wID);

	// Searching
	bool	CanFindNext()
	{
		return !m_fo.pattern.IsEmpty();
	}

	void	CancelIncSearch();
	void	StartIncSearch();

	void	StopIncSearch()
	{
		if(m_is_start)
			m_is_start.Release();
	}

	bool	DoIncSearch(const CString& str, bool fMore)
	{
		++m_ignore_changes;
		m_fo.pattern = str;
		bool ret = DoSearch(fMore);
		--m_ignore_changes;
		return ret;
	}

	bool DoSearch(bool fMore=true);
	bool DoSearchFromScopeStart();
	bool DoFindAll(bool showResults=true, CString* errorText=NULL);
	CString SearchResultStatus();
	CString FindAllResultStatus();
	std::size_t FindResultCount() const;
	CString FindResultPreview(std::size_t index) const;
	CString FindResultsQuery() const { return m_fo.pattern; }
	bool FindResultPreviewMatch(std::size_t index, std::size_t* start, std::size_t* length) const;
	CString FindResultsCompletionStatus() const { return m_find_results_completion_status; }
	void SetFindResultsCompletionStatus(const CString& status);
	bool AreFindResultsCurrent();
	std::uint64_t FindResultsRevision() const;
	bool SelectFindResult(std::size_t index);
	void ShowFindResults();
	bool DoSearchStd(bool fMore=true);
	bool DoSearchRegexp(bool fMore=true);
	void DoReplace();
	// Returns the committed replacement count; -2 means that preview was
	// cancelled and -1 means that the native query/mapping failed.
	int ReplaceAllSearchCore(CString* errorText=NULL);
	int GlobalReplace(MSHTML::IHTMLElementPtr elem = NULL, CString cntTag = L"P");
	int ToolWordsGlobalReplace(MSHTML::IHTMLElementPtr fbw_body, int* pIndex = NULL, int* globIndex = NULL, bool find = false, CString cntTag = L"P");

	// Shared DOM adapter for document-level binary insertion and editor commands.
	// ImageImport deliberately stays independent of MSHTML and SAFEARRAYs.
	HRESULT AddImportedBinary(const BYTE* data, size_t size, const CString& logicalFileName,
		const CString& mimeType, _variant_t* checkedId = NULL);
	void AddImage(const CString& filename,  bool bInline = false);

	CString LastSearchPattern()
	{
		return m_fo.pattern;
	}

	int ReplaceAllRe(const CString& re, const CString& str, MSHTML::IHTMLElementPtr elem = NULL, CString cntTag = L"P")
	{
		m_fo.pattern = re;
		m_fo.replacement = str;
		m_fo.fRegexp = true;
		m_fo.flags = 0;
		return GlobalReplace(elem, cntTag);
	}

	int ReplaceToolWordsRe( const CString& re,
							const CString& str,
							MSHTML::IHTMLElementPtr fbw_body,
							bool replace = false,
							CString cntTag = L"P",
							int* pIndex = NULL,
							int* globIndex = NULL,
							int replNum = 0
							)
	{
		m_fo.pattern = re;
		m_fo.replacement = str;
		m_fo.fRegexp = true;
		m_fo.flags = FRF_CASE | FRF_WHOLE;
		m_fo.replNum = replNum;
		return ToolWordsGlobalReplace(fbw_body, pIndex, globIndex, !replace, cntTag);
	}

  // searching in scintilla
  bool SciFindNext(HWND src,bool fFwdOnly,bool fBarf);

  // utilities
  CString		    SelPath();
  void			    GoTo(MSHTML::IHTMLElement *e,bool fScroll=true);
  MSHTML::IHTMLElementPtr SelectionContainer()
  {
    if (m_cur_sel)
      return m_cur_sel;
    return SelectionContainerImp();
  }

  bool GetSelectionInfo(MSHTML::IHTMLElementPtr *begin, MSHTML::IHTMLElementPtr *end, int* begin_char, int* end_char, MSHTML::IHTMLTxtRangePtr range);

  bool SelectionHasTags(wchar_t* elem);
  MSHTML::IHTMLElementPtr   SelectionAnchor();
  MSHTML::IHTMLElementPtr   SelectionAnchor(MSHTML::IHTMLElementPtr cur);
  MSHTML::IHTMLElementPtr   SelectionStructCon();
  MSHTML::IHTMLElementPtr	SelectionStructNearestCon();
  MSHTML::IHTMLElementPtr   SelectionStructCode();
  MSHTML::IHTMLElementPtr   SelectionStructImage();
  MSHTML::IHTMLElementPtr   SelectionStructSection();
  MSHTML::IHTMLElementPtr   SelectionStructTable();
	MSHTML::IHTMLElementPtr   SelectionStructTableCon();
	CStringA TableStructuralSnapshot();
	static void ResetTableGridBuildCountForTest();
	static long TableGridBuildCountForTest();
	bool SelectTableLogicalRangeForTest(long firstRow, long firstColumn, long lastRow, long lastColumn);
	bool DeleteTableLogicalColumnForTest(long column);
  MSHTML::IHTMLElementPtr   SelectionsStyleT();
  MSHTML::IHTMLElementPtr	SelectionsStyleTB(_bstr_t& style);
  MSHTML::IHTMLElementPtr   SelectionsStyle();
  MSHTML::IHTMLElementPtr	SelectionsStyleB(_bstr_t& style);
  MSHTML::IHTMLElementPtr   SelectionsColspan();
  MSHTML::IHTMLElementPtr	SelectionsColspanB(_bstr_t& colspan);
  MSHTML::IHTMLElementPtr   SelectionsRowspan();
  MSHTML::IHTMLElementPtr	SelectionsRowspanB(_bstr_t& rowspan);
  MSHTML::IHTMLElementPtr   SelectionsAlignTR();
  MSHTML::IHTMLElementPtr   SelectionsAlignTRB(_bstr_t& align);
  MSHTML::IHTMLElementPtr   SelectionsAlign();
  MSHTML::IHTMLElementPtr   SelectionsAlignB(_bstr_t& align);
  MSHTML::IHTMLElementPtr   SelectionsVAlign();
  MSHTML::IHTMLElementPtr   SelectionsVAlignB(_bstr_t& valign);

  void			    Normalize(MSHTML::IHTMLDOMNodePtr dom);
  MSHTML::IHTMLDOMNodePtr   GetChangedNode();
  void			    ImgSetURL(IDispatch *elem,const CString& url);

  bool			    SplitContainer(bool fCheck);
  FbeStructure::StructuralOperationResult SplitContainerResult(bool fCheck, FbeStructure::SplitFailurePoint failurePoint = FbeStructure::SplitFailurePoint::None);
//  MSHTML::IHTMLDOMNodePtr	  ChangeAttribute(MSHTML::IHTMLElementPtr elem, const wchar_t* attrib, const wchar_t* value);
  bool				InsertPoem(bool fCheck);
  bool				InsertCite(bool fCheck);
  FbeStructure::StructuralOperationResult InsertPoemResult(bool fCheck, FbeStructure::CitePoemFailurePoint failurePoint = FbeStructure::CitePoemFailurePoint::None);
  FbeStructure::StructuralOperationResult InsertCiteResult(bool fCheck, FbeStructure::CitePoemFailurePoint failurePoint = FbeStructure::CitePoemFailurePoint::None);
  bool				InsertTable(bool fCheck, bool bTitle=true, int nrows=1, int ncolumns=2);
	bool				MoveTableCell(bool reverse);
  long				InsertCode();
  bool				GoToFootnote(bool fCheck);
  bool				GoToReference(bool fCheck);
	bool				ReturnToLinkNavigationOrigin();
	void				ClearLinkNavigationHistory();
	bool				NavigateInternalLink(MSHTML::IHTMLElementPtr link, const CString& targetId);
  MSHTML::IHTMLTxtRangePtr	SetSelection(MSHTML::IHTMLElementPtr begin, MSHTML::IHTMLElementPtr end, int begin_pos, int end_pos);
  int				GetRelationalCharPos(MSHTML::IHTMLDOMNodePtr node, int pos);
  int				GetRealCharPos(MSHTML::IHTMLDOMNodePtr node, int pos);
  int				CountNodeChars(MSHTML::IHTMLDOMNodePtr node);
  int				GetRangePos(const MSHTML::IHTMLTxtRangePtr& range, MSHTML::IHTMLElementPtr &element, int &pos);  

  // script calls
  IDispatchPtr	Call(const wchar_t *name);
  bool		bCall(const wchar_t *name, int nParams, VARIANT* params);
  bool		bCall(const wchar_t *name);
  IDispatchPtr	Call(const wchar_t *name,IDispatch *pDisp);
  bool		bCall(const wchar_t *name,IDispatch *pDisp);

  // binary objects
  _variant_t	GetBinary(const wchar_t *id);

  // change notifications
  void	EditorChanged(int id);

  // external helper
  void                SetDocumentFilePathSource(const CString* filename, const bool* namevalid)
  {
    m_document_filename = filename;
    m_document_namevalid = namevalid;
  }
  IDispatchPtr         CreateHelper();

  // DWebBrowserEvents2
  void __stdcall  OnDocumentComplete(IDispatch *pDisp,VARIANT *vtUrl);
  void __stdcall  OnNavigateError(IDispatch *pDisp, VARIANT *vtUrl, VARIANT *vtFrame, VARIANT *vtStatusCode, VARIANT_BOOL *fCancel);
  void __stdcall  OnBeforeNavigate(IDispatch *pDisp,VARIANT *vtUrl,VARIANT *vtFlags,
				   VARIANT *vtTargetFrame,VARIANT *vtPostData,
				   VARIANT *vtHeaders,VARIANT_BOOL *fCancel);

  // HTMLDocumentEvents2
  void __stdcall	  OnSelChange(IDispatch *evt);
	void __stdcall OnScroll(IDispatch *evt);
  VARIANT_BOOL __stdcall  OnContextMenu(IDispatch *evt);
  VARIANT_BOOL __stdcall  OnClick(IDispatch *evt);
	VARIANT_BOOL __stdcall  OnMouseDown(IDispatch *evt);
	VARIANT_BOOL __stdcall  OnMouseMove(IDispatch *evt);
	VARIANT_BOOL __stdcall  OnMouseUp(IDispatch *evt);
  VARIANT_BOOL __stdcall  OnKeyDown(IDispatch *evt);
  void __stdcall	  OnFocusIn(IDispatch *evt);

	// HTMLTextContainerEvents2
	VARIANT_BOOL __stdcall OnRealPaste(IDispatch *evt);
	void __stdcall OnDrop(IDispatch*)
	{
		if(m_normalize)
			Normalize(Document()->body);
	}

   VARIANT_BOOL __stdcall OnDragDrop(IDispatch*)
   {
	    return VARIANT_FALSE;
   }

  // form changes
  bool	    IsFormChanged();
  void	    ResetFormChanged();
  bool	    IsFormCP();
  void	    ResetFormCP();

  // extract currently selected text
  _bstr_t   Selection();
	bool CloseFindDialog(CFindDlgBase* dlg);
	bool CloseFindDialog(CReplaceDlgBase* dlg);
	bool IsFindDialogOpen() const;
	bool IsReplaceDialogOpen() const;
	void SyncSearchOptionsToOpenDialogs(FRBase* source);

private:
	// added by SeNS
	int m_startMatch, m_endMatch;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FBEVIEW_H__E0C71279_419D_4273_93E3_57F6A57C7CFE__INCLUDED_)
