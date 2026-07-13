#include "stdafx.h"
#include "resource.h"
#include "res1.h"

#include "utils.h"
#include "apputils.h"

//#include <atlgdix.h>

#if _WIN32_WINNT>=0x0501
#include <atltheme.h>
#endif


#include "OptDlg.h"
#include "RuntimeLocalization.h"
#include "Settings.h"

extern CSettings _Settings;

struct InterfaceLanguageChoice
{
	DWORD languageId;
	UINT stringId;
};

static const InterfaceLanguageChoice kInterfaceLanguages[] = {
	{ FBE_INTERFACE_LANGUAGE_AUTO, IDS_LANG_SYSTEM_DEFAULT },
	{ FBE_INTERFACE_LANGUAGE_ENGLISH, IDS_LANG_ENGLISH },
	{ FBE_INTERFACE_LANGUAGE_RUSSIAN, IDS_LANG_RUSSIAN },
	{ FBE_INTERFACE_LANGUAGE_UKRAINIAN, IDS_LANG_UKRAINIAN },
	{ FBE_INTERFACE_LANGUAGE_GERMAN, IDS_LANG_GERMAN },
	{ FBE_INTERFACE_LANGUAGE_FRENCH, IDS_LANG_FRENCH },
	{ FBE_INTERFACE_LANGUAGE_SPANISH, IDS_LANG_SPANISH },
	{ FBE_INTERFACE_LANGUAGE_ITALIAN, IDS_LANG_ITALIAN },
	{ FBE_INTERFACE_LANGUAGE_POLISH, IDS_LANG_POLISH },
	{ FBE_INTERFACE_LANGUAGE_PORTUGUESE, IDS_LANG_PORTUGUESE },
	{ FBE_INTERFACE_LANGUAGE_DUTCH, IDS_LANG_DUTCH },
	{ FBE_INTERFACE_LANGUAGE_CZECH, IDS_LANG_CZECH },
	{ FBE_INTERFACE_LANGUAGE_BULGARIAN, IDS_LANG_BULGARIAN },
};


static int __stdcall EnumFontProc(const ENUMLOGFONTEX *lfe,
				 const NEWTEXTMETRICEX *ntm,
				 DWORD type,
				 LPARAM data)
{
  CSimpleArray<CString>	*stringList=(CSimpleArray<CString>*)data;
  stringList->Add(lfe->elfLogFont.lfFaceName);
  return TRUE;
}

static int  font_sizes[]={8,9,10,11,12,13,14,15,16,18,20,22,24,26,28,36,48,72};

static void SetRuntimeDialogItemText(HWND dialog, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	if (!text.IsEmpty())
		::SetDlgItemText(dialog, controlId, text);
}

LRESULT COptDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
  m_fg.SubclassWindow(GetDlgItem(IDC_FG));
  m_bg.SubclassWindow(GetDlgItem(IDC_BG));  
  m_fonts=GetDlgItem(IDC_FONT);
  m_srcfonts=GetDlgItem(IDC_SRCFONT);
  m_fontsize=GetDlgItem(IDC_FONT_SIZE);
  m_fast_mode = GetDlgItem(IDC_FAST_MODE); 
  m_lang = GetDlgItem(IDC_LANG);
  m_lang.SetDroppedWidth(320);
  // SeNS
  m_usespell_check = GetDlgItem(IDC_USESPELLCHECKER);
  m_highlight_check = GetDlgItem(IDC_BACKGROUNDSPELLCHECK);
  m_custom_dict = GetDlgItem(IDC_CUSTOM_DICT);

  // init color controls
  m_bg.SetDefaultColor(::GetSysColor(COLOR_WINDOW));
  m_fg.SetDefaultColor(::GetSysColor(COLOR_WINDOWTEXT));
  m_bg.SetColor(_Settings.GetColorBG());
  m_fg.SetColor(_Settings.GetColorFG());

  wchar_t buf[MAX_LOAD_STRING + 1];
  const DWORD currentLanguage = _Settings.GetInterfaceLanguageID();
  int selectedLanguageIndex = 0;
  for(int i = 0; i < _countof(kInterfaceLanguages); ++i)
  {
	if(FbeLoadString(_Module.GetResourceInstance(), kInterfaceLanguages[i].stringId, buf, MAX_LOAD_STRING))
	{
	  const int item = m_lang.AddString(buf);
	  if(item >= 0)
	  {
		m_lang.SetItemData(item, kInterfaceLanguages[i].languageId);
		if(kInterfaceLanguages[i].languageId == currentLanguage)
		  selectedLanguageIndex = item;
	  }
	}
  }
  m_lang.SetCurSel(selectedLanguageIndex);

  // get font list
  CSimpleArray<CString> installedFonts;
  HDC	hDC=::CreateDC(_T("DISPLAY"),NULL,NULL,NULL);
  LOGFONT lf;
  memset(&lf,0,sizeof(lf));
  lf.lfCharSet=ANSI_CHARSET;
  ::EnumFontFamiliesEx(hDC,&lf,(FONTENUMPROC)&EnumFontProc,(LPARAM)&installedFonts,0);
  ::DeleteDC(hDC);

  for (int i=0; i<installedFonts.GetSize(); i++)
  {
	m_fonts.AddString(installedFonts[i]);
	m_srcfonts.AddString(installedFonts[i]);
  }

  // get body font name
  CString     fnt(_Settings.GetFont());
  int	      idx=m_fonts.FindStringExact(0,fnt);
  if (idx<0) idx=0;
  m_fonts.SetCurSel(idx);

  // get source font name
  fnt.SetString(_Settings.GetSrcFont());
  idx=m_srcfonts.FindStringExact(0,fnt);
  if (idx<0) idx=0;
  m_srcfonts.SetCurSel(idx);


  // init zoom
  m_fsz_val = _Settings.GetFontSize();
  CString     szstr;
  szstr.Format(_T("%d"),m_fsz_val);
  m_fontsize.SetWindowText(szstr);
  for (int i=0;i<sizeof(font_sizes)/sizeof(font_sizes[0]);++i) {
    szstr.Format(_T("%d"),font_sizes[i]);
    m_fontsize.AddString(szstr);
  }

  m_src_wrap=GetDlgItem(IDC_WRAP);
  m_src_hl=GetDlgItem(IDC_SYNTAXHL);
  m_src_taghl=GetDlgItem(IDC_TAGHL);
  m_src_eol=GetDlgItem(IDC_SHOWEOL);
  m_src_whitespace=GetDlgItem(IDC_SHOWWHITESPACE);
  m_src_line_numbers=GetDlgItem(IDC_SHOWLINENUMBERS);

  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_FOREGROUND_COLOR, L"fbe.dialog.idd_options.foreground_color", L"Text color:");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_BACKGROUND_COLOR, L"fbe.dialog.idd_options.background_color", L"Background:");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_BODY_FONT, L"fbe.dialog.idd_options.font", L"Editor font:");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_FONT_SIZE, L"fbe.dialog.idd_options.font_size", L"Font size:");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_INTERFACE_GROUP, L"fbe.dialog.idd_options.interface", L"Common");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_BODY_GROUP, L"fbe.dialog.idd_options.font_group", L"Body");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_SOURCE_GROUP, L"fbe.dialog.idd_options.source_view", L"XML source editor");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_LANGUAGE_LABEL, L"fbe.dialog.idd_options.language", L"Language:");
  SetRuntimeDialogItemText(m_hWnd, IDC_WRAP, L"fbe.dialog.idd_options.wrap_lines", L"Wrap lines");
  SetRuntimeDialogItemText(m_hWnd, IDC_SYNTAXHL, L"fbe.dialog.idd_options.syntax_highlight", L"Syntax highlighting");
  SetRuntimeDialogItemText(m_hWnd, IDC_SHOWEOL, L"fbe.dialog.idd_options.show_eol", L"Show end of line marks");
  SetRuntimeDialogItemText(m_hWnd, IDC_FAST_MODE, L"fbe.dialog.idd_options.fast_mode", L"Fast Mode");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_SPELLCHECK_GROUP, L"fbe.dialog.idd_options.spell_checking", L"Spellcheck");
  SetRuntimeDialogItemText(m_hWnd, IDC_BACKGROUNDSPELLCHECK, L"fbe.dialog.idd_options.background_spell_check", L"Highlight misspelled words");
  SetRuntimeDialogItemText(m_hWnd, IDC_USESPELLCHECKER, L"fbe.dialog.idd_options.use_spellchecker", L"Use spellchecker");
  SetRuntimeDialogItemText(m_hWnd, IDS_SPELL_CUSTOM_DICT, L"fbe.dialog.idd_options.custom_dict", L"Custom dictionary:");
  SetRuntimeDialogItemText(m_hWnd, IDC_SHOWLINENUMBERS, L"fbe.dialog.idd_options.show_line_numbers", L"Show line numbers");
  SetRuntimeDialogItemText(m_hWnd, IDC_TAGHL, L"fbe.dialog.idd_options.tag_highlight", L"Highlight matched tags");
  SetRuntimeDialogItemText(m_hWnd, IDC_OPTIONS_SOURCE_FONT, L"fbe.dialog.idd_options.source_font", L"Editor font:");
  SetRuntimeDialogItemText(m_hWnd, IDC_SHOWWHITESPACE, L"fbe.dialog.idd_options.show_whitespace", L"Show white spaces");

  // init controls
  m_src_wrap.SetCheck(_Settings.XmlSrcWrap());
  m_src_hl.SetCheck(_Settings.XmlSrcSyntaxHL());  
  m_src_taghl.SetCheck(_Settings.XmlSrcTagHL());  
  m_src_eol.SetCheck(_Settings.XmlSrcShowEOL());  
  m_src_whitespace.SetCheck(_Settings.XmlSrcShowSpace());
  m_src_line_numbers.SetCheck(_Settings.XMLSrcShowLineNumbers());
  
  if(_Settings.FastMode())
	m_fast_mode.SetCheck(BST_CHECKED);
  else
    m_fast_mode.SetCheck(BST_UNCHECKED);

  // SeNS
  if (_Settings.GetUseSpellChecker())
  {
	  m_usespell_check.SetCheck(BST_CHECKED);
	  m_highlight_check.EnableWindow(TRUE);
  }
  else
  {
	  m_usespell_check.SetCheck(BST_UNCHECKED);
	  m_highlight_check.EnableWindow(FALSE);
  }
  if(_Settings.GetHighlightMisspells())
	m_highlight_check.SetCheck(BST_CHECKED);
  else
	m_highlight_check.SetCheck(BST_UNCHECKED);

  m_custom_dict.SetWindowText (_Settings.GetCustomDict());

  return 0;
}

LRESULT COptDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
  // fetch zoom
  CString   szstr(U::GetWindowText(m_fontsize));
  if (_stscanf(szstr,_T("%d"),&m_fsz_val)!=1 ||
    m_fsz_val<6 || m_fsz_val>72)
  {
    MessageBeep(MB_ICONERROR);
    m_fontsize.SetFocus();
    return 0;
  }

  // save colors to registry
  _Settings.SetColorBG(m_bg.GetColor());
  _Settings.SetColorFG(m_fg.GetColor());

  // save source font face
  m_face=U::GetWindowText(m_srcfonts);
  _Settings.SetSrcFont(m_face);

  // save body font face
  m_face=U::GetWindowText(m_fonts);
  _Settings.SetFont(m_face);

  // save zoom
  _Settings.SetFontSize(m_fsz_val);

  _Settings.SetXmlSrcWrap(m_src_wrap.GetCheck() != 0);
  _Settings.SetXmlSrcSyntaxHL(m_src_hl.GetCheck() != 0);
  _Settings.SetXmlSrcTagHL(m_src_taghl.GetCheck() != 0);
  _Settings.SetXmlSrcShowEOL(m_src_eol.GetCheck() != 0);
  _Settings.SetXmlSrcShowSpace(m_src_whitespace.GetCheck() != 0);
  _Settings.SetXMLSrcShowLineNumbers(m_src_line_numbers.GetCheck() != 0);

  _Settings.SetFastMode(m_fast_mode.GetCheck() != 0);  
  _Settings.SetUseSpellChecker(m_usespell_check.GetCheck() != 0); // SeNS
  _Settings.SetHighlightMisspells(m_highlight_check.GetCheck() != 0); // SeNS

  CString s;
  m_custom_dict.GetWindowText(s);
  _Settings.SetCustomDict(s);

  DWORD new_lang = _Settings.GetInterfaceLanguageID();
  const int selectedLanguageIndex = m_lang.GetCurSel();
  if(selectedLanguageIndex >= 0)
	new_lang = static_cast<DWORD>(m_lang.GetItemData(selectedLanguageIndex));

  if(new_lang != _Settings.GetInterfaceLanguageID())
  {
	_Settings.SetNeedRestart();
	_Settings.SetInterfaceLanguage(new_lang);
	FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName());
	FbeResetRuntimeLocalization();
  }

  return 0;
}

LRESULT COptDlg::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
  return 0;
}

TCHAR FileNameBuffer[_MAX_PATH];

LRESULT COptDlg::OnShowFileDialog(WORD, WORD, HWND, BOOL&)
{
	OPENFILENAME ofn;
	memset (&ofn, 0, sizeof (OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hInstance = _Module.m_hInst;
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrDefExt = L"dic";
	ofn.lpstrFilter = L"Dictionaries (*.dic)\0*.dic\0All files (*.*)\0*.*\0\0";
	m_custom_dict.GetWindowText(FileNameBuffer, _MAX_PATH);
	ofn.lpstrFile = FileNameBuffer;
    ofn.nFilterIndex = 0;
    ofn.nMaxFile = _MAX_PATH;
    ofn.nMaxFileTitle = _MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY;
	if (GetOpenFileName(&ofn))
	{
		m_custom_dict.SetWindowText(ofn.lpstrFile);
	}
    return 0;	
}
