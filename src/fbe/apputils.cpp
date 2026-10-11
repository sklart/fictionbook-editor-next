#include "stdafx.h"
#include "resource.h"
#include "res1.h"
#include "RuntimeLocalization.h"

#include "utils.h"
#include "apputils.h"

namespace AU {

// a getopt implementation
static int   xgetopt(
	CSimpleArray<CString>& argv,
	const TCHAR *ospec,
	int&	    argp,
	const TCHAR *&state,
	const TCHAR *&arg)
{
  const TCHAR	  *cp;
  TCHAR		  opt;

  if (!state || !state[0]) { // look a the next arg
    if (argp>=argv.GetSize() || argv[argp][0]!='-') // no more options
      return 0;
    if (!argv[argp][1]) // a lone '-', treat as an end of list
      return 0;
    if (argv[argp][1]=='-') { // '--', ignore rest of text and stop
      ++argp;
      return 0;
    }
    state=(const TCHAR *)argv[argp]+1;
    ++argp;
  }
  // we are in a middle of an arg
  opt=(unsigned)*state++;
  bool found = false;
  for (cp=ospec;*cp;++cp) {
    if (*cp==opt)
	{
		found = true;
		break;
	}      
    if (cp[1]==':')
      ++cp;
  }
  if(!found)
  {
	U::MessageBox(MB_OK|MB_ICONERROR, IDS_ERRMSGBOX_CAPTION, IDS_INVALID_CML_MSG, opt);
	return -1; // error
  }

  if (cp[1]==':') { // option requires an argument
    if (*state) { // use rest of string
      arg=state;
      state=NULL;
      return opt;
    }
    // use next arg if available
    if (argp<argv.GetSize()) {
      arg=argv[argp];
      ++argp;
      return opt;
    }
    // barf about missing args
    U::MessageBox(MB_OK|MB_ICONERROR, IDS_ERRMSGBOX_CAPTION, IDS_CML_ARGS_MSG, opt);
    return -1;
  }
  // just return current option
  return opt;
}

CmdLineArgs   _ARGS;

bool  ParseCmdLineArgs() {
  // DeploymentContext reads these switches directly from the process command
  // line.  Remove them before the legacy short-option parser sees the leading
  // "--" as its end-of-options marker; otherwise --portable/--installed is
  // accidentally treated as a document name.
  for (int index = _ARGV.GetSize() - 1; index >= 0; --index)
    if (_ARGV[index].CompareNoCase(L"--portable") == 0 ||
        _ARGV[index].CompareNoCase(L"--installed") == 0)
      _ARGV.RemoveAt(index);
  const TCHAR	*arg,*state=NULL;
  int		argp=0;
  int		ch;
  for (;;) {
	 switch ((ch=xgetopt(_ARGV,_T("ducb:s"),argp,state,arg))) {
    case 0: // end of options
      while (argp--)
	_ARGV.RemoveAt(0);
      return true;
    case _T('d'):
      _ARGS.start_in_desc_mode=true;
      break;
	case _T('u'):
	  _ARGS.disable_undo_selection_history=true;
	  break;
	case _T('c'):
	  _ARGS.run_source_view_cycles=true;
	  break;
	case _T('s'):
	  _ARGS.save_benchmark_document=true;
	  break;
	case _T('b'):
	  _ARGS.source_memory_benchmark_path=arg;
	  break;
    case -1: // error
      return false;
      // just ignore options for now :)
    }
  }
}

// Keep modal input-box placement separate from the modeless Find/Replace position.
static RECT s_inputBoxRect = {-1, -1, -1, -1};

// an input box
class CInputBox: public CDialogImpl<CInputBox>,
		 public CWinDataExchange<CInputBox>
{
public:
  enum { IDD=IDD_INPUTBOX };
  CString	m_text;
  const wchar_t	*m_title;
  const wchar_t	*m_prompt;

  CInputBox(const wchar_t *title,const wchar_t *prompt) : m_title(title), m_prompt(prompt) { }

  BEGIN_DDX_MAP(CInputBox)
    DDX_TEXT(IDC_INPUT,m_text)
  END_DDX_MAP()

  BEGIN_MSG_MAP(CInputBox)
        COMMAND_ID_HANDLER(IDYES, OnYes)
		COMMAND_ID_HANDLER(IDNO, OnCancel)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    FbeApplyRuntimeDialogLocalization(m_hWnd, IDD_INPUTBOX);
    DoDataExchange(FALSE);  // Populate the controls
    SetDlgItemText(IDC_PROMPT,m_prompt);
    SetWindowText(m_title);
	AdjustPromptLayout();
    return 0;
  }

  void AdjustPromptLayout()
  {
	CRect window, prompt;
	GetWindowRect(window);
	GetDlgItem(IDC_PROMPT).GetWindowRect(prompt);
	ScreenToClient(prompt);
	const CString text(m_prompt ? m_prompt : L"");
	RECT measured = { 0, 0, prompt.Width(), 0 };
	HDC dc = ::GetDC(m_hWnd);
	const HFONT font = reinterpret_cast<HFONT>(::SendMessage(GetDlgItem(IDC_PROMPT), WM_GETFONT, 0, 0));
	const HFONT previous = dc && font ? static_cast<HFONT>(::SelectObject(dc, font)) : NULL;
	if(dc) ::DrawTextW(dc, text, text.GetLength(), &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
	if(previous) ::SelectObject(dc, previous);
	const int padding = dc ? ::MulDiv(4, ::GetDeviceCaps(dc, LOGPIXELSY), 96) : 4;
	if(dc) ::ReleaseDC(m_hWnd, dc);
	CRect wanted(window);
	if(s_inputBoxRect.left != -1) wanted.MoveToXY(s_inputBoxRect.left, s_inputBoxRect.top);
	MONITORINFO monitor = { sizeof(monitor) };
	if(!::GetMonitorInfoW(::MonitorFromRect(&wanted, MONITOR_DEFAULTTONEAREST), &monitor)) return;
	const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
	const int maxGrowth = (std::max)(0, workHeight - window.Height());
	const int desiredHeight = (std::max)(prompt.Height(), static_cast<int>(measured.bottom) + padding);
	const int growth = (std::min)(desiredHeight - prompt.Height(), maxGrowth);
	const bool needsScroll = desiredHeight > prompt.Height() + growth;
	if(growth > 0)
	{
		GetDlgItem(IDC_PROMPT).MoveWindow(prompt.left, prompt.top, prompt.Width(), prompt.Height() + growth);
		const int buttons[] = { IDC_INPUT, IDYES, IDNO, IDCANCEL };
		for(int id : buttons)
		{
			CRect bounds;
			GetDlgItem(id).GetWindowRect(bounds);
			ScreenToClient(bounds);
			GetDlgItem(id).MoveWindow(bounds.left, bounds.top + growth, bounds.Width(), bounds.Height());
		}
		wanted.bottom += growth;
	}
	if(needsScroll)
	{
		HWND originalPrompt = ::GetDlgItem(m_hWnd, IDC_PROMPT);
		::SetWindowLongPtrW(originalPrompt, GWLP_ID, static_cast<LONG_PTR>(IDC_STATIC));
		HWND details = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			prompt.left, prompt.top, prompt.Width(), prompt.Height() + growth,
			m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROMPT)), _Module.GetModuleInstance(), NULL);
		if(details)
		{
			::SendMessage(details, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
			::ShowWindow(originalPrompt, SW_HIDE);
		}
		else ::SetWindowLongPtrW(originalPrompt, GWLP_ID, static_cast<LONG_PTR>(IDC_PROMPT));
	}
	const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
	const int width = (std::min)(wanted.Width(), workWidth);
	const int height = (std::min)(wanted.Height(), workHeight);
	const int x = (std::min)((std::max)(wanted.left, monitor.rcWork.left), monitor.rcWork.right - width);
	const int y = (std::min)((std::max)(wanted.top, monitor.rcWork.top), monitor.rcWork.bottom - height);
	::SetWindowPos(m_hWnd, NULL, x, y, width, height,
		SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOZORDER);
  }

  BOOL EndDialog(int nRetCode)
  {
	::GetWindowRect(m_hWnd, &s_inputBoxRect);
	return ::EndDialog(m_hWnd, nRetCode);
  }

  LRESULT OnYes(WORD, WORD wID, HWND, BOOL&) {
    DoDataExchange(TRUE);  // Populate the data members
    EndDialog(wID);
    return 0L;
  }

  LRESULT OnCancel(WORD, WORD wID, HWND, BOOL&) {
    EndDialog(wID);
    return 0L;
  }
};

int	InputBox(CString& result, const wchar_t *title, const wchar_t *prompt) {
  CInputBox   dlg(title,prompt);
  dlg.m_text=result;
  int dlgResult = dlg.DoModal();
  if (dlgResult == IDYES) result=dlg.m_text;
  return dlgResult;
}

// html
CString GetAttrCS(MSHTML::IHTMLElement *elem,const wchar_t *attr) {
  if (!elem) return L"";
  _variant_t	    va(elem->getAttribute(attr,2));
  if (V_VT(&va)!=VT_BSTR)
    return CString();
  return V_BSTR(&va);
}

_bstr_t GetAttrB(MSHTML::IHTMLElement *elem,const wchar_t *attr) {
  if (!elem) return L"";
  _variant_t	    va(elem->getAttribute(attr,2));
  if (V_VT(&va)!=VT_BSTR)
    return _bstr_t();
  return _bstr_t(va.Detach().bstrVal,false);
}

char	*ToUtf8(const CString& s,int& patlen) {
  DWORD   len=::WideCharToMultiByte(CP_UTF8,0,
		  s,s.GetLength(),
		  NULL,0,NULL,NULL);
  char    *tmp=(char *)malloc(len+1);
  if (tmp) {
    ::WideCharToMultiByte(CP_UTF8,0,
		  s,s.GetLength(),
		  tmp,len,NULL,NULL);
    tmp[len]='\0';
  }
  patlen=len;
  return tmp;
}

}
