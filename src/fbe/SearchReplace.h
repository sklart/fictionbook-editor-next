#ifndef SEARCHREPLACE_H
#define SEARCHREPLACE_H

#include "ModelessDialog.h"
#include "Settings.h"
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
		HWND scope = GetDlgItem(IDC_FIND_SCOPE);
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
		const struct { LPCWSTR Text; AU::Search::SearchScope Value; } values[] = {
			{ FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.scope_whole_document", L"Whole document"), AU::Search::SearchScope::WholeDocument },
			{ FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_find.scope_current_section", L"Current section"), AU::Search::SearchScope::CurrentSection }
		};
		for (int index = 0; index != _countof(values); ++index)
		{
			const LRESULT item = ::SendMessage(scope, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(values[index].Text));
			::SendMessage(scope, CB_SETITEMDATA, item, static_cast<LPARAM>(values[index].Value));
		}
		if (m_view->HasTextSelection())
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
		m_view->m_fo.flags = flags & ~CFBEView::FRF_REGEX;

		m_view->m_startMatch = m_view->m_endMatch = 0;

		m_text = GetDlgItem(IDC_TEXT);

		// Set fields
		PutData();
		if (!isReplaceDialog)
		{
			SetRuntimeText(IDC_FIND_SCOPE_LABEL, L"fbe.dialog.idd_find.scope", L"Scope:");
			PopulateFindScopes();
		}

		return 0;
	}

	LRESULT OnTextChanged(WORD, WORD /* unused: wID */, HWND, BOOL&)
	{
		CheckInput();
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
		_Settings.SetSearchOptions(m_view->m_fo.flags | (m_view->m_fo.fRegexp ? CFBEView::FRF_REGEX : 0), true);
		SaveHistoryImp(m_fh,GetDlgItem(IDC_TEXT));
		SaveHistoryImp(m_rh,GetDlgItem(IDC_REPLACE));
	}
};

class CFindDlgBase: public CModelessDialogImpl<CFindDlgBase>, public FRBase
{
public:
	enum { IDD = IDD_FIND };

	CFindDlgBase(CFBEView *view) : FRBase(view){ }

	BEGIN_MSG_MAP(CFindDlgBase)
		COMMAND_ID_HANDLER(ID_FIND_NEXT, OnDoFind)
		COMMAND_ID_HANDLER(IDC_FIND_ALL, OnDoFindAll)
		COMMAND_HANDLER(IDC_FIND_SCOPE, CBN_SELCHANGE, OnScopeChanged)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		CHAIN_MSG_MAP_ALT(FRBase, 1)
	END_MSG_MAP()


	LRESULT OnCancel(WORD, WORD /* unused: wID */, HWND, BOOL&)
	{
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
		if (m_view->DoFindAll())
		{
			SaveString();
			SaveHistory();
			FRBase::SetDlgItemText(IDC_FIND_STATUS, m_view->FindAllResultStatus());
		}
		return 0;
	}

	LRESULT OnScopeChanged(WORD, WORD, HWND, BOOL&)
	{
		m_view->ResetSearchScope();
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

	explicit CFindResultsDlg(CFBEView* view) : m_view(view) { }

	BEGIN_MSG_MAP(CFindResultsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_HANDLER(IDC_FIND_RESULTS_LIST, LVN_ITEMACTIVATE, OnItemActivate)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

	void Refresh()
	{
		if (!IsValid())
			return;
		m_list.DeleteAllItems();
		if (!m_view->AreFindResultsCurrent())
		{
			::SetWindowText(GetDlgItem(IDC_FIND_RESULTS_STATUS), L"Search results are stale. Run Find All again.");
			return;
		}
		for (std::size_t index = 0; index < m_view->FindResultCount(); ++index)
		{
			CString number;
			number.Format(L"%Iu", index + 1);
			const int item = m_list.InsertItem(static_cast<int>(index), number);
			m_list.SetItemText(item, 1, m_view->FindResultPreview(index));
		}
		CString status;
		status.Format(L"%Iu results", m_view->FindResultCount());
		::SetWindowText(GetDlgItem(IDC_FIND_RESULTS_STATUS), status);
	}

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
	{
		m_list = GetDlgItem(IDC_FIND_RESULTS_LIST);
		m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		m_list.InsertColumn(0, L"#", LVCFMT_RIGHT, 38);
		m_list.InsertColumn(1, L"Context", LVCFMT_LEFT, 260);
		Refresh();
		return 0;
	}

	LRESULT OnItemActivate(int, LPNMHDR header, BOOL&)
	{
		const NMLISTVIEW* item = reinterpret_cast<const NMLISTVIEW*>(header);
		if (item->iItem < 0)
			return 0;
		if (!m_view->SelectFindResult(static_cast<std::size_t>(item->iItem)))
			Refresh();
		return 0;
	}

	LRESULT OnCancel(WORD, WORD, HWND, BOOL&)
	{
		m_view->CloseFindResultsDialog(this);
		return 0;
	}

private:
	CFBEView* m_view;
	CListViewCtrl m_list;
};

#endif
