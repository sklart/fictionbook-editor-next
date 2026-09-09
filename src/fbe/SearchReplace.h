#ifndef SEARCHREPLACE_H
#define SEARCHREPLACE_H

#include "ModelessDialog.h"
#include "Settings.h"
#include "SettingsTooltips.h"
#include "RuntimeLocalization.h"

extern CSettings _Settings;
extern bool VBErr;

class FRBase: public CWinDataExchange<FRBase>
{
public:
	CRegKey		m_fh,m_rh;

	CFBEView*	m_view;
	int			m_whole;
	int			m_case;
	int			m_regexp;
	int			m_dir;
	int			m_unicode;
	int			m_scope;
	CEdit		m_text;
	CSettingsTooltips m_tooltips;

	FRBase(CFBEView* view) : m_view(view), m_whole(0), m_case(0), m_regexp(0), m_dir(1), m_unicode(0), m_scope(0) { }

  HWND	GetDlgItem(int id) { return X_GetDlgItem(id); }
  virtual HWND X_GetDlgItem(int id) = 0;
  BOOL	SetDlgItemText(int id,const TCHAR *str) { return ::SetWindowText(GetDlgItem(id),str); }

  void SetRuntimeText(int id, LPCWSTR key, LPCWSTR fallback)
  {
    const CString text = FbeLoadRuntimeStringByKey(key, fallback);
    if (!text.IsEmpty() && GetDlgItem(id))
      SetDlgItemText(id, text);
  }

  void SetRuntimeDialogTitle(LPCWSTR key, LPCWSTR fallback)
  {
    const CString text = FbeLoadRuntimeStringByKey(key, fallback);
    HWND probe = GetDlgItem(IDC_TEXT);
    HWND dialog = probe ? ::GetParent(probe) : NULL;
    if (!text.IsEmpty() && dialog)
      ::SetWindowText(dialog, text);
  }

	BEGIN_MSG_MAP(FRBase)
		ALT_MSG_MAP(1)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_TEXT,CBN_EDITCHANGE, OnTextChanged)
	END_MSG_MAP()

  BEGIN_DDX_MAP(FRBase)
    DDX_TEXT(IDC_TEXT, m_view->m_fo.pattern)
    if (GetDlgItem(IDC_REPLACE))
      DDX_TEXT(IDC_REPLACE, m_view->m_fo.replacement);
    DDX_CHECK(IDC_WHOLE, m_whole)
    DDX_CHECK(IDC_MATCHCASE, m_case)
    DDX_CHECK(IDC_REGEXP, m_regexp)
	if (GetDlgItem(IDC_FIND_UNICODE_PROPERTIES))
		DDX_CHECK(IDC_FIND_UNICODE_PROPERTIES, m_unicode)
    if (GetDlgItem(IDC_UP))
      DDX_RADIO(IDC_UP, m_dir);
  END_DDX_MAP()

	void GetData()
	{
		m_text.SetSelNone();

		DoDataExchange(TRUE);

		int flags = 0;
		if(m_case)
			flags |= CFBEView::FRF_CASE;
		if(m_whole)
			flags |= CFBEView::FRF_WHOLE;
		if(m_dir == 0)
			flags |= CFBEView::FRF_REVERSE;

		m_view->m_fo.flags = flags;
		m_view->m_fo.fRegexp = m_regexp != 0;
		m_view->m_fo.unicodeProperties = m_unicode != 0;
		HWND scope = FRBase::GetDlgItem(IDC_FIND_SCOPE);
		if (scope)
		{
			const LRESULT selection = ::SendMessage(scope, CB_GETCURSEL, 0, 0);
			if (selection != CB_ERR)
				m_view->m_fo.scope = static_cast<AU::Search::SearchScope>(::SendMessage(scope, CB_GETITEMDATA, selection, 0));
		}
	}

	void PutData()
	{
		m_case = (m_view->m_fo.flags & CFBEView::FRF_CASE) != 0;
		m_whole = (m_view->m_fo.flags & CFBEView::FRF_WHOLE) != 0;
		m_dir = (m_view->m_fo.flags & CFBEView::FRF_REVERSE) == 0;
		m_regexp = m_view->m_fo.fRegexp;
		m_unicode = m_view->m_fo.unicodeProperties;
		m_scope = static_cast<int>(m_view->m_fo.scope);
		DoDataExchange(FALSE);
	}

	void PopulateFindScopes()
	{
		HWND scope = GetDlgItem(IDC_FIND_SCOPE);
		if (!scope)
			return;
		::SendMessage(scope, CB_RESETCONTENT, 0, 0);
		const CString wholeDocument = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.scope_whole_document", L"Whole document");
		const CString currentSection = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.scope_current_section", L"Current section");
		const struct { LPCWSTR Text; AU::Search::SearchScope Value; } values[] = {
			{ wholeDocument, AU::Search::SearchScope::WholeDocument },
			{ currentSection, AU::Search::SearchScope::CurrentSection }
		};
		for (int index = 0; index != _countof(values); ++index)
		{
			const LRESULT item = ::SendMessage(scope, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(values[index].Text));
			::SendMessage(scope, CB_SETITEMDATA, item, static_cast<LPARAM>(values[index].Value));
		}
		// A completed Selection search owns a stable source range. Find Next moves
		// MSHTML's visual selection to a hit, so keep this entry while that source
		// range remains valid; otherwise refresh it from the live selection.
		if (m_view->HasTextSelection() ||
			(m_scope == static_cast<int>(AU::Search::SearchScope::Selection) && m_view->HasSavedSearchScope()))
		{
			const CString selectionText = FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.scope_selection", L"Selection");
			const LRESULT item = ::SendMessage(scope, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(selectionText)));
			::SendMessage(scope, CB_SETITEMDATA, item, static_cast<LPARAM>(AU::Search::SearchScope::Selection));
		}
		for (LRESULT index = 0, count = ::SendMessage(scope, CB_GETCOUNT, 0, 0); index < count; ++index)
			if (static_cast<int>(::SendMessage(scope, CB_GETITEMDATA, index, 0)) == m_scope)
			{
				::SendMessage(scope, CB_SETCURSEL, index, 0);
				return;
			}
		::SendMessage(scope, CB_SETCURSEL, 0, 0);
	}

  void	LoadHistoryImp(const TCHAR *path,CRegKey& rk,HWND hCB,CString& first) {
    if (!hCB)
      return;

    // open history key
    if (rk.Create(_Settings.GetKey(), path)!=ERROR_SUCCESS)
      return;
    
    // get number of entries
    DWORD nfs;
    if (rk.QueryDWORDValue(_T(""),nfs)!=ERROR_SUCCESS)
      return;
    
    // fetch the entries
    //first.Empty();

    CString   ps,str;

    for (DWORD i=0;i<nfs;++i) 
	{
      ps.Format(_T("%d"), static_cast<int>(i));
      str=U::QuerySV(rk,ps);
      if (!str.IsEmpty()) 
	  {
		::SendMessage(hCB,CB_ADDSTRING,0,(LPARAM)(const TCHAR *)str);
		if (first.IsEmpty())
			first=str;
      }
    }
  }

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL& bHandled)
	{
		bHandled = FALSE;

		LoadHistoryImp(_T("SearchHistory"), m_fh, GetDlgItem(IDC_TEXT), m_view->m_fo.pattern);
		LoadHistoryImp(_T("ReplaceHistory"), m_rh, GetDlgItem(IDC_REPLACE), m_view->m_fo.replacement);

		const bool isReplaceDialog = GetDlgItem(IDC_REPLACE) != NULL;
		SetRuntimeDialogTitle(isReplaceDialog ? L"fbe.dialog.idd_replace.caption" : L"fbe.dialog.idd_find.caption", isReplaceDialog ? L"Replace" : L"Find");
		SetRuntimeText(isReplaceDialog ? IDC_REPLACE_LABEL_TEXT : IDC_FIND_LABEL_TEXT,
			isReplaceDialog ? L"fbe.dialog.idd_replace.find_what" : L"fbe.dialog.idd_find.find_what",
			isReplaceDialog ? L"Find:" : L"Find what:");
		SetRuntimeText(ID_FIND_NEXT, isReplaceDialog ? L"fbe.dialog.idd_replace.find_next" : L"fbe.dialog.idd_find.find_next", L"&Find Next");
		if (!isReplaceDialog)
			SetRuntimeText(IDC_FIND_ALL, L"fbe.dialog.idd_find.find_all", L"Find &All");
		SetRuntimeText(IDC_WHOLE, isReplaceDialog ? L"fbe.dialog.idd_replace.whole_word" : L"fbe.dialog.idd_find.whole_word", L"Match &whole words");
		SetRuntimeText(IDC_MATCHCASE, isReplaceDialog ? L"fbe.dialog.idd_replace.match_case" : L"fbe.dialog.idd_find.match_case", L"Match &case");
		SetRuntimeText(IDC_REGEXP, isReplaceDialog ? L"fbe.dialog.idd_replace.regexp" : L"fbe.dialog.idd_find.regexp", L"Regular &expression");
		if (!isReplaceDialog)
			SetRuntimeText(IDC_FIND_UNICODE_PROPERTIES, L"fbe.dialog.idd_find.unicode_properties", L"Unicode properties");
		SetRuntimeText(isReplaceDialog ? IDC_REPLACE_DIRECTION_GROUP : IDC_FIND_DIRECTION_GROUP,
			isReplaceDialog ? L"fbe.dialog.idd_replace.direction" : L"fbe.dialog.idd_find.direction",
			L"Direction");
		SetRuntimeText(IDC_UP, isReplaceDialog ? L"fbe.dialog.idd_replace.up" : L"fbe.dialog.idd_find.up", L"&Up");
		SetRuntimeText(IDC_DOWN, isReplaceDialog ? L"fbe.dialog.idd_replace.down" : L"fbe.dialog.idd_find.down", L"&Down");
		SetRuntimeText(IDCANCEL, isReplaceDialog ? L"fbe.dialog.idd_replace.cancel" : L"fbe.dialog.idd_find.cancel", L"Cancel");
		if(isReplaceDialog)
		{
			SetRuntimeText(IDC_REPLACE_LABEL_REPLACE, L"fbe.dialog.idd_replace.replace_with", L"Replace:");
			SetRuntimeText(IDC_REPLACE_ONE, L"fbe.dialog.idd_replace.replace_one", L"&Replace");
			SetRuntimeText(IDC_REPLACE_ALL, L"fbe.dialog.idd_replace.replace_all", L"Replace &All");
		}

		// Load options
		DWORD flags = _Settings.GetSearchOptions();
		m_view->m_fo.fRegexp = (flags & CFBEView::FRF_REGEX) != 0;
		m_view->m_fo.unicodeProperties = (flags & CFBEView::FRF_UNICODE_PROPERTIES) != 0;
		m_view->m_fo.flags = flags & ~(CFBEView::FRF_REGEX | CFBEView::FRF_UNICODE_PROPERTIES);

		m_view->m_startMatch = m_view->m_endMatch = 0;

		m_text = GetDlgItem(IDC_TEXT);

		// Set fields
		PutData();
		UpdateUnicodeControl();
		if (!isReplaceDialog)
		{
			SetRuntimeText(IDC_FIND_SCOPE_LABEL, L"fbe.dialog.idd_find.scope", L"Scope:");
			PopulateFindScopes();
			const HWND dialog = ::GetParent(GetDlgItem(IDC_TEXT));
			if (dialog)
			{
				m_tooltips.Initialize(dialog);
				m_tooltips.Add(GetDlgItem(IDC_TEXT), L"fbe.tooltip.find.text", L"Text to find. Results update after a short pause while typing.");
				m_tooltips.Add(GetDlgItem(ID_FIND_NEXT), L"fbe.tooltip.find.next", L"Select the next match in the chosen direction.");
				m_tooltips.Add(GetDlgItem(IDC_FIND_ALL), L"fbe.tooltip.find.all", L"Show every match in the Results window.");
				m_tooltips.Add(GetDlgItem(IDC_WHOLE), L"fbe.tooltip.find.whole_word", L"Match complete words only.");
				m_tooltips.Add(GetDlgItem(IDC_MATCHCASE), L"fbe.tooltip.find.match_case", L"Distinguish uppercase and lowercase letters.");
				m_tooltips.Add(GetDlgItem(IDC_REGEXP), L"fbe.tooltip.find.regexp", L"Interpret the query as a regular expression.");
				m_tooltips.Add(GetDlgItem(IDC_FIND_SCOPE), L"fbe.tooltip.find.scope", L"Choose where to search.");
				m_tooltips.Add(GetDlgItem(IDC_FIND_UNICODE_PROPERTIES), L"fbe.tooltip.find.unicode_properties", L"Use Unicode properties in regular expressions.");
				m_tooltips.Add(GetDlgItem(IDC_FIND_STATUS), L"fbe.tooltip.find.status", L"Search status and complete regular-expression diagnostic.");
				m_tooltips.Add(GetDlgItem(IDC_UP), L"fbe.tooltip.find.up", L"Search toward the beginning of the document.");
				m_tooltips.Add(GetDlgItem(IDC_DOWN), L"fbe.tooltip.find.down", L"Search toward the end of the document.");
			}
		}

		return 0;
	}

	LRESULT OnTextChanged(WORD, WORD /* unused: wID */, HWND, BOOL&)
	{
		CheckInput();
		// Find All is debounced so editing a query never synchronously invokes
		// PCRE2 on every keystroke. Replace keeps its existing explicit flow.
		if (GetDlgItem(IDC_FIND_STATUS))
			::SetTimer(::GetParent(GetDlgItem(IDC_TEXT)), 0x4F01, 150, NULL);
		return 0;
	}

  void	CheckInput() {
    ::EnableWindow(GetDlgItem(IDOK),::GetWindowTextLength(GetDlgItem(IDC_TEXT))>0);
  }

	void SaveStringImp(HWND hCB)
	{
		if(!hCB)
			return;

		CString cur = U::GetWindowText(hCB);

		if(cur.IsEmpty())
			return;

		LRESULT Idx = ::SendMessage(hCB, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), (LPARAM)(const TCHAR*)cur);
		if(Idx == 0)
			return;
		if(Idx != CB_ERR)
			::SendMessage(hCB, CB_DELETESTRING, Idx, 0);

		::SendMessage(hCB, CB_INSERTSTRING, 0,(LPARAM)(const TCHAR*)cur);
		// fix for issue #136
		::SendMessage(hCB, CB_SETCURSEL, 0, 0);
	}

	void SaveString()
	{
		SaveStringImp(GetDlgItem(IDC_TEXT));
		SaveStringImp(GetDlgItem(IDC_REPLACE));
	}

	void SaveHistoryImp(CRegKey& rk,HWND hCB)
	{
		if(!rk && !hCB)
			return;

		LRESULT lCount = ::SendMessage(hCB, CB_GETCOUNT, 0, 0);
		if(lCount > 100)
			lCount = 100;

		CString path;
		for (int i = 0; i < lCount; ++i)
		{
			CString cur(U::GetCBString(hCB, i));
			if(cur.IsEmpty())
				continue;
			path.Format(L"%d", i);
			rk.SetStringValue(path, cur);
		}

		rk.SetDWORDValue(L"", lCount);
	}

	void SaveHistory() 
	{
		SaveSearchOptions();
		SaveHistoryImp(m_fh,GetDlgItem(IDC_TEXT));
		SaveHistoryImp(m_rh,GetDlgItem(IDC_REPLACE));
	}

	void SaveSearchOptions()
	{
		_Settings.SetSearchOptions(m_view->m_fo.flags |
			(m_view->m_fo.fRegexp ? CFBEView::FRF_REGEX : 0) |
			(m_view->m_fo.unicodeProperties ? CFBEView::FRF_UNICODE_PROPERTIES : 0), true);
	}

	void UpdateUnicodeControl()
	{
		HWND unicode = GetDlgItem(IDC_FIND_UNICODE_PROPERTIES);
		if (unicode)
			::EnableWindow(unicode, ::IsDlgButtonChecked(::GetParent(unicode), IDC_REGEXP) == BST_CHECKED);
	}
};

class CFindDlgBase: public CModelessDialogImpl<CFindDlgBase>, public FRBase
{
public:
	enum { IDD = IDD_FIND };

	CFindDlgBase(CFBEView *view) : FRBase(view){ }

	BEGIN_MSG_MAP(CFindDlgBase)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		COMMAND_ID_HANDLER(ID_FIND_NEXT, OnDoFind)
		COMMAND_ID_HANDLER(IDC_FIND_ALL, OnDoFindAll)
		COMMAND_HANDLER(IDC_FIND_SCOPE, CBN_SELCHANGE, OnScopeChanged)
		COMMAND_HANDLER(IDC_FIND_SCOPE, CBN_DROPDOWN, OnScopeDropDown)
		COMMAND_HANDLER(IDC_MATCHCASE, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_HANDLER(IDC_WHOLE, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_HANDLER(IDC_REGEXP, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_HANDLER(IDC_FIND_UNICODE_PROPERTIES, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_HANDLER(IDC_UP, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_HANDLER(IDC_DOWN, BN_CLICKED, OnSearchOptionChanged)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		CHAIN_MSG_MAP_ALT(FRBase, 1)
	END_MSG_MAP()


	LRESULT OnCancel(WORD, WORD /* unused: wID */, HWND, BOOL&)
	{
		::KillTimer(m_hWnd, 0x4F01);
		GetData();
		SaveSearchOptions();
		m_view->CloseFindDialog(this);
		return 0;
	}

	LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&)
	{
		::KillTimer(m_hWnd, 0x4F01);
		GetData();
		SaveSearchOptions();
		m_view->CloseFindDialog(this);
		return 0;
	}

	LRESULT OnDoFind(WORD, WORD, HWND, BOOL&)
	{
		DoFind();
		return 0;
	}

	LRESULT OnDoFindAll(WORD, WORD, HWND, BOOL&)
	{
		GetData();
		VBErr = false;
		CString error;
		if (m_view->DoFindAll(true, &error))
		{
			SaveString();
			SaveHistory();
			FRBase::SetDlgItemText(IDC_FIND_STATUS, m_view->FindAllResultStatus());
		}
		else
		{
			// Find All is explicit but stays modeless: keep the PCRE2 diagnostic in
			// the wide status row instead of leaving a previous result count visible.
			FRBase::SetDlgItemText(IDC_FIND_STATUS, error.IsEmpty()
				? FbeLoadRuntimeStringByKey(L"fbe.search.error.invalid_expression", L"Invalid search expression")
				: error);
		}
		return 0;
	}

	LRESULT OnScopeChanged(WORD, WORD, HWND, BOOL&)
	{
		m_view->ResetSearchScope();
		::SetTimer(m_hWnd, 0x4F01, 150, NULL);
		return 0;
	}

	LRESULT OnScopeDropDown(WORD, WORD, HWND, BOOL&)
	{
		HWND scope = FRBase::GetDlgItem(IDC_FIND_SCOPE);
		const LRESULT selected = scope ? ::SendMessage(scope, CB_GETCURSEL, 0, 0) : CB_ERR;
		if (selected != CB_ERR)
			m_scope = static_cast<int>(::SendMessage(scope, CB_GETITEMDATA, selected, 0));
		PopulateFindScopes();
		return 0;
	}

	LRESULT OnSearchOptionChanged(WORD, WORD, HWND, BOOL&)
	{
		UpdateUnicodeControl();
		::SetTimer(m_hWnd, 0x4F01, 150, NULL);
		return 0;
	}

	LRESULT OnTimer(UINT, WPARAM timerId, LPARAM, BOOL& bHandled)
	{
		bHandled = FALSE;
		if (timerId != 0x4F01)
			return 0;
		::KillTimer(m_hWnd, 0x4F01);
		GetData();
		CString error;
		if (m_view->m_fo.pattern.IsEmpty())
			FRBase::SetDlgItemText(IDC_FIND_STATUS, L"");
		else if (m_view->DoFindAll(false, &error))
			FRBase::SetDlgItemText(IDC_FIND_STATUS, m_view->FindAllResultStatus());
		else
			FRBase::SetDlgItemText(IDC_FIND_STATUS, error.IsEmpty()
				? FbeLoadRuntimeStringByKey(L"fbe.search.error.invalid_expression", L"Invalid search expression")
				: error);
		return 0;
	}

	virtual void DoFind() = 0;
	virtual HWND X_GetDlgItem(int id)
	{
		return CModelessDialogImpl<CFindDlgBase>::GetDlgItem(id);
	}
};

class CReplaceDlgBase: public CModelessDialogImpl<CReplaceDlgBase>,
		       public FRBase
{
public:
  enum { IDD = IDD_REPLACE };
  bool m_selvalid; // true if last search was successful but no replacement was done

  CReplaceDlgBase(CFBEView *view) : FRBase(view), m_selvalid(false) { }

  BEGIN_MSG_MAP(CReplaceDlgBase)
    COMMAND_ID_HANDLER(ID_FIND_NEXT, OnDoFind)
    COMMAND_ID_HANDLER(IDC_REPLACE_ONE, OnDoReplace)
    COMMAND_ID_HANDLER(IDC_REPLACE_ALL, OnDoReplaceAll)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

    COMMAND_HANDLER(IDC_TEXT,CBN_EDITCHANGE, OnTextChanged)
    COMMAND_HANDLER(IDC_REPLACE,CBN_EDITCHANGE, OnReplChanged)

    CHAIN_MSG_MAP_ALT(FRBase, 1)
  END_MSG_MAP()


  LRESULT OnCancel(WORD, WORD /* unused: wID */, HWND, BOOL&) {
	  m_view->CloseFindDialog(this);
    return 0;
  }
  virtual void DoFind() = 0;
  LRESULT OnDoFind(WORD, WORD, HWND, BOOL&) {
    GetData();
    DoFind();
    return 0;
  }
  virtual void DoReplace() = 0;
  LRESULT OnDoReplace(WORD, WORD, HWND, BOOL&) {
    GetData();
    DoReplace();
    return 0;
  }
  virtual void DoReplaceAll() = 0;
  LRESULT OnDoReplaceAll(WORD, WORD, HWND, BOOL&) {
    GetData();
    DoReplaceAll();
    return 0;
  }

  LRESULT OnTextChanged(WORD, WORD /* unused: wID */, HWND, BOOL& bHandled) {
    SendMessage(DM_SETDEFID,IDOK);
    bHandled=FALSE;
    return 0;
  }
  LRESULT OnReplChanged(WORD, WORD /* unused: wID */, HWND, BOOL& bHandled) {
    SendMessage(DM_SETDEFID,IDC_REPLACE_ONE);
    bHandled=FALSE;
    return 0;
  }
  void MakeClose() {
    // change cancel button to "Close"
	CString s;
	s = FbeLoadCString(IDS_MB_CLOSE);
	::SetWindowText(CModelessDialogImpl<CReplaceDlgBase>::GetDlgItem(IDCANCEL),s);
    SendMessage(DM_SETDEFID,IDC_REPLACE_ONE);
  }
  virtual HWND	X_GetDlgItem(int id) { return CModelessDialogImpl<CReplaceDlgBase>::GetDlgItem(id); }
};

class CViewFindDlg: public CFindDlgBase
{
public:
	CViewFindDlg(CFBEView* view) : CFindDlgBase(view) { }

	virtual void DoFind()
	{
		GetData();
		VBErr = false;
		if(!m_view->DoSearch())
		{
			if (!VBErr)
			{
				if (!m_view->LastSearchError().IsEmpty())
					::MessageBox(m_hWnd, m_view->LastSearchError(), FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.caption", L"Find"), MB_OK | MB_ICONEXCLAMATION);
				else
					U::MessageBox(MB_OK | MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
			}
		}
		else
		{
			SaveString();
			SaveHistory();
			FRBase::SetDlgItemText(IDC_FIND_STATUS, m_view->SearchResultStatus());
		}
	}
};

// A modeless companion to Find.  It stores indexes only; the document-facing
// coordinator remains the sole owner of snapshot offsets and generation.
class CFindResultsDlg: public CModelessDialogImpl<CFindResultsDlg>
{
public:
	enum { IDD = IDD_FIND_RESULTS };

	explicit CFindResultsDlg(CFBEView* view) : m_view(view), m_revision(0) { }

	BEGIN_MSG_MAP(CFindResultsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		NOTIFY_HANDLER(IDC_FIND_RESULTS_LIST, LVN_ITEMACTIVATE, OnItemActivate)
		NOTIFY_HANDLER(IDC_FIND_RESULTS_LIST, NM_CUSTOMDRAW, OnListCustomDraw)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	void Refresh()
	{
		if (!IsValid())
			return;
		m_list.DeleteAllItems();
		if (!m_view->AreFindResultsCurrent())
		{
			::SetWindowText(GetDlgItem(IDC_FIND_RESULTS_STATUS), FbeLoadRuntimeStringByKey(
				L"fbe.dialog.idd_find_results.stale", L"Search results are stale. Run Find All again."));
			return;
		}
		m_revision = m_view->FindResultsRevision();
		for (std::size_t index = 0; index < m_view->FindResultCount(); ++index)
		{
			CString number;
			number.Format(L"%Iu", index + 1);
			const int item = m_list.InsertItem(static_cast<int>(index), number);
			m_list.SetItemText(item, 1, m_view->FindResultPreview(index));
		}
		CString status;
		status.Format(FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.count", L"%Iu results"), m_view->FindResultCount());
		::SetWindowText(GetDlgItem(IDC_FIND_RESULTS_STATUS), status);
	}

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
	{
		::SetWindowText(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.caption", L"Find results"));
		::SetWindowText(GetDlgItem(IDCANCEL), FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.close", L"Close"));
		m_list = GetDlgItem(IDC_FIND_RESULTS_LIST);
		m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		m_list.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.number", L"#"), LVCFMT_RIGHT, 38);
		m_list.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find_results.context", L"Context"), LVCFMT_LEFT, 260);
		Refresh();
		// WM_SIZE is not guaranteed after a modeless dialog's initial layout.
		// Size the sole Context column now so it never looks like an unused third
		// column until the user manually resizes the Results window.
		BOOL handled = FALSE;
		OnSize(WM_SIZE, 0, 0, handled);
		return 0;
	}

	LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL&)
	{
		if (!m_list.IsWindow()) return 0;
		RECT client = {};
		GetClientRect(&client);
		const int margin = 8;
		RECT closeRect = {};
		RECT statusRect = {};
		::GetWindowRect(::GetDlgItem(m_hWnd, IDCANCEL), &closeRect);
		::GetWindowRect(::GetDlgItem(m_hWnd, IDC_FIND_RESULTS_STATUS), &statusRect);
		const int closeWidth = (std::max)(60, static_cast<int>(closeRect.right - closeRect.left));
		const int closeHeight = (std::max)(23, static_cast<int>(closeRect.bottom - closeRect.top));
		const int statusHeight = (std::max)(16, static_cast<int>(statusRect.bottom - statusRect.top));
		const int footerHeight = (std::max)(closeHeight, statusHeight);
		int listWidth = static_cast<int>(client.right) - 2 * margin;
		const int footerTop = static_cast<int>(client.bottom) - margin - footerHeight;
		int listHeight = footerTop - 2 * margin;
		int statusWidth = static_cast<int>(client.right) - 3 * margin - closeWidth;
		if (listWidth < 0) listWidth = 0;
		if (listHeight < 0) listHeight = 0;
		if (statusWidth < 0) statusWidth = 0;
		m_list.SetWindowPos(HWND_TOP, margin, margin, listWidth, listHeight, SWP_NOZORDER);
		m_list.SetColumnWidth(1, listWidth > 38 ? listWidth - 38 : 0);
		::SetWindowPos(::GetDlgItem(m_hWnd, IDC_FIND_RESULTS_STATUS), HWND_TOP, margin,
			footerTop + (footerHeight - statusHeight) / 2, statusWidth, statusHeight, SWP_NOZORDER);
		::SetWindowPos(::GetDlgItem(m_hWnd, IDCANCEL), HWND_TOP, client.right - margin - closeWidth, footerTop,
			closeWidth, closeHeight, SWP_NOZORDER);
		return 0;
	}

	LRESULT OnListCustomDraw(int, LPNMHDR header, BOOL&)
	{
		NMLVCUSTOMDRAW* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
		if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
			return CDRF_NOTIFYITEMDRAW;
		if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
			return CDRF_NOTIFYSUBITEMDRAW;
		if (draw->nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM) || draw->iSubItem != 1)
			return CDRF_DODEFAULT;

		std::size_t matchStart = 0, matchLength = 0;
		const int item = static_cast<int>(draw->nmcd.dwItemSpec);
		if (item < 0 || !m_view->FindResultPreviewMatch(static_cast<std::size_t>(item), &matchStart, &matchLength) ||
			matchLength == 0)
			return CDRF_DODEFAULT;
		const CString text = m_view->FindResultPreview(static_cast<std::size_t>(item));
		if (matchStart >= static_cast<std::size_t>(text.GetLength()))
			return CDRF_DODEFAULT;
		matchLength = (std::min)(matchLength, static_cast<std::size_t>(text.GetLength()) - matchStart);

		RECT cell = {};
		if (!m_list.GetSubItemRect(item, 1, LVIR_LABEL, &cell))
			return CDRF_DODEFAULT;
		cell.left += 3;
		HDC dc = draw->nmcd.hdc;
		HFONT oldFont = static_cast<HFONT>(::SelectObject(dc,
			reinterpret_cast<HGDIOBJ>(::SendMessage(m_list, WM_GETFONT, 0, 0))));
		const bool selected = (m_list.GetItemState(item, LVIS_SELECTED) & LVIS_SELECTED) != 0;
		::SetBkMode(dc, TRANSPARENT);
		::SetTextColor(dc, selected ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_WINDOWTEXT));
		::DrawText(dc, text, text.GetLength(), &cell, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

		SIZE prefix = {};
		::GetTextExtentPoint32(dc, text, static_cast<int>(matchStart), &prefix);
		const CString matched = text.Mid(static_cast<int>(matchStart), static_cast<int>(matchLength));
		SIZE matchedSize = {};
		::GetTextExtentPoint32(dc, matched, matched.GetLength(), &matchedSize);
		RECT highlight = cell;
		highlight.left += prefix.cx;
		highlight.right = highlight.left + matchedSize.cx;
		if (highlight.left < cell.right && highlight.right > cell.left)
		{
			HBRUSH brush = ::CreateSolidBrush(selected ? RGB(46, 112, 184) : RGB(255, 235, 120));
			::FillRect(dc, &highlight, brush);
			::DeleteObject(brush);
			::SetTextColor(dc, selected ? RGB(255, 255, 255) : RGB(100, 45, 0));
			::DrawText(dc, matched, matched.GetLength(), &highlight, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP);
		}
		::SelectObject(dc, oldFont);
		return CDRF_SKIPDEFAULT;
	}

	LRESULT OnItemActivate(int, LPNMHDR header, BOOL&)
	{
		const NMLISTVIEW* item = reinterpret_cast<const NMLISTVIEW*>(header);
		if (item->iItem < 0)
			return 0;
		if (m_revision != m_view->FindResultsRevision() ||
			!m_view->SelectFindResult(static_cast<std::size_t>(item->iItem)))
			Refresh();
		return 0;
	}

	LRESULT OnCancel(WORD, WORD, HWND, BOOL&)
	{
		m_view->CloseFindResultsDialog(this);
		return 0;
	}

	LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&)
	{
		m_view->CloseFindResultsDialog(this);
		return 0;
	}

private:
	CFBEView* m_view;
	CListViewCtrl m_list;
	std::uint64_t m_revision;
};

#endif
