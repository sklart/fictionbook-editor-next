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
				m_tooltips.Add(GetDlgItem(IDC_FIND_ALL), L"fbe.tooltip.find.all", L"Show every match in the Results pane.");
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
			SetFindStatus(m_view->FindAllResultStatus());
		}
		else
		{
			// Find All is explicit but stays modeless: keep the PCRE2 diagnostic in
			// the wide status row instead of leaving a previous result count visible.
			SetFindStatus(error.IsEmpty()
				? FbeLoadRuntimeStringByKey(L"fbe.search.error.mapping_failed", L"Search could not be mapped to the document.")
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
			SetFindStatus(L"");
		else if (m_view->DoFindAll(false, &error))
			SetFindStatus(m_view->FindAllResultStatus());
		else
			SetFindStatus(error.IsEmpty()
				? FbeLoadRuntimeStringByKey(L"fbe.search.error.mapping_failed", L"Search could not be mapped to the document.")
				: error);
		return 0;
	}

	virtual void DoFind() = 0;
	void SetFindStatus(const CString& status)
	{
		FRBase::SetDlgItemText(IDC_FIND_STATUS, status);
		const CString tooltip = status.IsEmpty()
			? FbeLoadRuntimeStringByKey(L"fbe.tooltip.find.status", L"Search status and complete regular-expression diagnostic.")
			: status;
		m_tooltips.UpdateText(FRBase::GetDlgItem(IDC_FIND_STATUS), tooltip);
	}
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
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
    COMMAND_ID_HANDLER(ID_FIND_NEXT, OnDoFind)
    COMMAND_ID_HANDLER(IDC_REPLACE_ONE, OnDoReplace)
    COMMAND_ID_HANDLER(IDC_REPLACE_ALL, OnDoReplaceAll)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

    COMMAND_HANDLER(IDC_TEXT,CBN_EDITCHANGE, OnTextChanged)
    COMMAND_HANDLER(IDC_REPLACE,CBN_EDITCHANGE, OnReplChanged)

    CHAIN_MSG_MAP_ALT(FRBase, 1)
  END_MSG_MAP()


  LRESULT OnCancel(WORD, WORD /* unused: wID */, HWND, BOOL&) {
	  GetData();
	  SaveSearchOptions();
	  m_view->CloseFindDialog(this);
    return 0;
  }
	LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&) {
		GetData();
		SaveSearchOptions();
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
				if (!m_view->LastSearchError().IsEmpty() && m_view->LastSearchErrorIsRegexp())
					::MessageBox(m_hWnd, m_view->LastSearchError(), FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.caption", L"Find"), MB_OK | MB_ICONEXCLAMATION);
				else if (m_view->LastSearchError().IsEmpty())
					U::MessageBox(MB_OK | MB_ICONEXCLAMATION, IDR_MAINFRAME, IDS_SEARCH_FAIL_MSG, static_cast<LPCWSTR>(m_view->m_fo.pattern));
				else
					SetFindStatus(m_view->LastSearchError());
			}
		}
		else
		{
			SaveString();
			SaveHistory();
			SetFindStatus(m_view->SearchResultStatus());
		}
	}
};

#endif
